"""Host client tests use a byte-stream transport; no serial hardware required."""
import sys
from pathlib import Path
import struct
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import hood_tool as tool


def fixture():
    return struct.pack("<II", 0x20020000, tool.APP_BASE + 9) + bytes(range(100))


class Transport:
    def __init__(self, handler):
        self.handler = handler
        self.parser = tool.Parser()
        self.bytes = bytearray()
        self.writes = 0
        self.closed = False

    def write(self, data):
        self.writes += 1
        for cmd, seq, payload in self.parser.feed(data):
            result = self.handler(cmd, payload)
            if result is not None:
                # Periodic text logs and a fragmented response share the same line.
                self.bytes.extend(b"[CONTROL] stopped\r\n" + tool.encode(cmd | 0x80, seq, result))
        return len(data)

    def read(self, count):
        count = min(count, 3)
        data = bytes(self.bytes[:count])
        del self.bytes[:count]
        return data

    def close(self):
        self.closed = True


class ProtocolTests(unittest.TestCase):
    def test_stream_fragments_and_noise(self):
        parser = tool.Parser()
        packet = tool.encode(tool.DATA, 65535, bytes(range(256)))
        frames = []
        for byte in b"noise" + packet:
            frames.extend(parser.feed(bytes([byte])))
        self.assertEqual(frames, [(tool.DATA, 65535, bytes(range(256)))])

    def test_bad_crc_then_valid(self):
        packet = bytearray(tool.encode(tool.INFO, 1))
        packet[-1] ^= 1
        frames = tool.Parser().feed(bytes(packet) + tool.encode(tool.INFO, 2))
        self.assertEqual(frames, [(tool.INFO, 2, b"")])

    def test_bad_length(self):
        parser = tool.Parser()
        frames = parser.feed(b"\xa5\x5a\x01\x01\x00\x00\xff\xff" + tool.encode(tool.INFO, 3))
        self.assertEqual(frames, [(tool.INFO, 3, b"")])
        with self.assertRaises(ValueError):
            tool.encode(tool.INFO, 1, bytes(257))

    def test_truncated_frame_timeout(self):
        parser = tool.Parser()
        truncated = tool.encode(tool.DATA, 1, bytes(256))[:12]
        reply = tool.encode(tool.INFO, 2, b"device")
        with patch.object(tool.time, "monotonic", side_effect=[10.0, 10.5, 12.1]):
            self.assertEqual(parser.feed(truncated), [])
            self.assertEqual(parser.feed(b""), [])
            self.assertEqual(parser.feed(reply), [(tool.INFO, 2, b"device")])

    def test_empty_reads_do_not_extend_frame_timeout(self):
        parser = tool.Parser()
        reply = tool.encode(tool.INFO, 3, b"device")
        with patch.object(tool.time, "monotonic", side_effect=[10.0, 11.0, 11.6]):
            self.assertEqual(parser.feed(reply[:8]), [])
            self.assertEqual(parser.feed(b""), [])
            self.assertEqual(parser.feed(reply), [(tool.INFO, 3, b"device")])

    def test_package_roundtrip(self):
        packed = tool.package(fixture(), 7)
        self.assertEqual(tool.unpack_package(packed), (fixture(), 7))
        installed = tool.installed_image(fixture(), 7)
        self.assertEqual(len(installed), 128 * 1024)
        footer = installed[-32:]
        self.assertEqual(struct.unpack_from("<I", footer, 24)[0], 0x434F4D54)

    def test_package_damage(self):
        packed = bytearray(tool.package(fixture(), 7))
        packed[-1] ^= 1
        with self.assertRaises(ValueError):
            tool.unpack_package(packed)
        packed = bytearray(tool.package(fixture(), 7))
        packed[4] ^= 1
        with self.assertRaises(ValueError):
            tool.unpack_package(packed)
        with self.assertRaises(ValueError):
            tool.unpack_package(b"HUP1")

    def test_standalone_vector_rejected(self):
        binary = struct.pack("<II", 0x20020000, 0x08000009) + bytes(100)
        with self.assertRaises(ValueError):
            tool.package(binary, 1)
        with self.assertRaises(ValueError):
            tool.package(bytes(tool.IMAGE_MAX + 1), 1)

    def test_request_matching(self):
        transport = Transport(lambda *_: b"\x00" + b"device")
        client = tool.Client("fake", transport=transport)
        self.assertEqual(client.request(tool.INFO), b"device")
        client.close()
        self.assertTrue(transport.closed)

    def test_retry_after_lost_ack(self):
        calls = []
        def handler(cmd, payload):
            calls.append((cmd, payload))
            return b"\x00done" if len(calls) == 2 else None
        client = tool.Client("fake", transport=Transport(handler))
        self.assertEqual(client.request(tool.COMMIT, timeout=0.001), b"done")
        self.assertEqual(len(calls), 2)

    def test_device_rejection(self):
        client = tool.Client("fake", transport=Transport(lambda *_: b"\x04"))
        with self.assertRaisesRegex(RuntimeError, "Flash failure"):
            client.request(tool.COMMIT)

    def test_config_wire(self):
        config = dict(zip(tool.CONFIG_KEYS, (600, 1800, 0.25, 0.2, 0, 0.4, 2.5, 30000)))
        requests = []
        def handler(cmd, payload):
            requests.append((cmd, payload))
            if cmd == tool.GET_CONFIG:
                return b"\x00" + tool.CONFIG.pack(*config.values())
            return b"\x00"
        client = tool.Client("fake", transport=Transport(handler))
        result = client.set_config({"manual_rpm": 700})
        self.assertEqual(result["manual_rpm"], 700)
        self.assertEqual(struct.unpack_from("<I", requests[-1][1])[0], 700)
        with self.assertRaises(ValueError):
            client.set_config({"invalid": 1})

    def test_upload_and_start_confirmation(self):
        image = fixture() * 4
        progress = []
        receiving = bytearray()
        state = {"role": 0, "committed": False}
        def handler(cmd, payload):
            if cmd == tool.INFO:
                return b"\x00" + struct.pack("<5I", state["role"], 1, tool.APP_BASE, tool.IMAGE_MAX, 252)
            if cmd == tool.BEGIN:
                self.assertEqual(struct.unpack("<III", payload), (len(image), tool.zlib.crc32(image), 4))
            elif cmd == tool.DATA:
                offset = struct.unpack_from("<I", payload)[0]
                self.assertEqual(offset, len(receiving))
                receiving.extend(payload[4:])
                return b"\x00" + struct.pack("<I", len(receiving))
            elif cmd == tool.COMMIT:
                self.assertEqual(bytes(receiving), image)
                state["committed"] = True
            elif cmd == tool.RUN:
                self.assertTrue(state["committed"])
                state["role"] = 1
            return b"\x00"
        client = tool.Client("fake", transport=Transport(handler))
        with patch.object(tool.time, "sleep"):
            client.flash(tool.package(image, 4), lambda count, total: progress.append((count, total)))
        self.assertEqual(progress[-1], (len(image), len(image)))
        self.assertEqual(state["role"], 1)

    def test_standalone_upload_rejected(self):
        transport = Transport(lambda *_: b"\x00" + struct.pack("<5I", 1, 0, tool.APP_BASE, tool.IMAGE_MAX, 252))
        client = tool.Client("fake", transport=transport)
        with self.assertRaisesRegex(RuntimeError, "Standalone"):
            client.flash(tool.package(fixture(), 1))
        self.assertEqual(transport.writes, 1)

    def test_application_to_loader(self):
        state = {"role": 1, "committed": False}
        commands = []
        def handler(cmd, payload):
            commands.append(cmd)
            if cmd == tool.INFO:
                return b"\x00" + struct.pack("<5I", state["role"], 1, tool.APP_BASE, tool.IMAGE_MAX, 252)
            if cmd == tool.STATUS:
                values = [0] * 14
                values[10] = 1
                return b"\x00" + struct.pack("<14I5f", *values, *([0] * 5))
            if cmd == tool.BOOT:
                state["role"] = 0
            if cmd == tool.DATA:
                offset = struct.unpack_from("<I", payload)[0]
                return b"\x00" + struct.pack("<I", offset + len(payload) - 4)
            if cmd == tool.RUN:
                state["role"] = 1
            return b"\x00"
        client = tool.Client("fake", transport=Transport(handler))
        with patch.object(tool.time, "sleep"):
            client.flash(tool.package(fixture(), 1))
        self.assertLess(commands.index(tool.STOP), commands.index(tool.BOOT))
        self.assertLess(commands.index(tool.BOOT), commands.index(tool.BEGIN))


if __name__ == "__main__":
    unittest.main()
