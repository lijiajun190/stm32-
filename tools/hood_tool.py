"""Range hood serial client, firmware packager, and native desktop console."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import struct
import time
import zlib

APP_BASE = 0x08020000
IMAGE_MAX = 0x20000 - 32
HEADER = struct.Struct("<4s5I")
CONFIG = struct.Struct("<IIfffffI")
CONFIG_KEYS = ("manual_rpm", "max_rpm", "kp", "ki", "kd", "gas_low_v", "gas_high_v", "shutdown_delay_ms")
TASK_NAMES = ("MotorCtrl", "Sensor", "UI", "SysMgr", "Logger", "Storage", "Idle", "Watchdog")
ERRORS = ("OK", "bad argument", "busy", "CRC mismatch", "Flash failure", "invalid state", "unsupported")
INFO, BEGIN, DATA, COMMIT, ABORT, RUN = 1, 0x10, 0x11, 0x12, 0x13, 0x14
STATUS, GET_CONFIG, SET_CONFIG, MONITOR, STOP, BOOT = 0x20, 0x21, 0x22, 0x23, 0x24, 0x30


def encode(command: int, sequence: int, payload: bytes = b"") -> bytes:
    if len(payload) > 256:
        raise ValueError("Payload exceeds 256 bytes")
    body = struct.pack("<BBHH", 1, command, sequence, len(payload)) + payload
    return b"\xa5\x5a" + body + struct.pack("<I", zlib.crc32(body))


class Parser:
    """Discard log text and damaged frames while recovering the next valid frame."""

    def __init__(self):
        self.buffer = bytearray()
        self.last_byte_time = None

    def feed(self, data: bytes):
        now = time.monotonic()
        # 与设备端使用相同的字节间隙超时，丢弃截断帧后恢复下一次回复。
        if self.buffer and self.last_byte_time is not None and now - self.last_byte_time > 1.5:
            self.buffer.clear()
        if data:
            self.last_byte_time = now
        self.buffer.extend(data)
        frames = []
        while self.buffer:
            start = self.buffer.find(b"\xa5\x5a")
            if start < 0:
                self.buffer[:] = b"\xa5" if self.buffer[-1] == 0xA5 else b""
                break
            del self.buffer[:start]
            if len(self.buffer) < 8:
                break
            version, command, sequence, length = struct.unpack_from("<BBHH", self.buffer, 2)
            if version != 1 or length > 256:
                del self.buffer[0]
                continue
            total = length + 12
            if len(self.buffer) < total:
                break
            body = self.buffer[2:8 + length]
            expected = struct.unpack_from("<I", self.buffer, 8 + length)[0]
            if zlib.crc32(body) != expected:
                del self.buffer[0]
                continue
            frames.append((command, sequence, bytes(self.buffer[8:8 + length])))
            del self.buffer[:total]
        return frames


def validate_image(image: bytes):
    if not 8 <= len(image) <= IMAGE_MAX:
        raise ValueError(f"Firmware must be 8..{IMAGE_MAX} bytes")
    stack, entry = struct.unpack_from("<II", image)
    if not (0x20000000 < stack <= 0x20020000 and stack % 8 == 0):
        raise ValueError("Invalid initial stack pointer")
    if not (entry & 1 and APP_BASE <= (entry & ~1) < APP_BASE + len(image)):
        raise ValueError("Firmware is not linked at 0x08020000; use the Upgrade build")


def package(image: bytes, version: int) -> bytes:
    validate_image(image)
    header = struct.pack("<4s4I", b"HUP1", APP_BASE, len(image), zlib.crc32(image), version)
    return header + struct.pack("<I", zlib.crc32(header)) + image


def unpack_package(data: bytes):
    if len(data) < HEADER.size:
        raise ValueError("Truncated upgrade package")
    magic, address, length, checksum, version, header_crc = HEADER.unpack_from(data)
    if magic != b"HUP1" or address != APP_BASE or zlib.crc32(data[:20]) != header_crc:
        raise ValueError("Invalid package header")
    image = data[HEADER.size:]
    if len(image) != length or zlib.crc32(image) != checksum:
        raise ValueError("Invalid firmware length or CRC")
    validate_image(image)
    return image, version


def installed_image(image: bytes, version: int) -> bytes:
    """ST-LINK initial application image with the same committed footer as the loader."""
    validate_image(image)
    footer = bytearray(b"\xff" * 32)
    struct.pack_into("<4I", footer, 0, 0x484F4F44, len(image), zlib.crc32(image), version)
    struct.pack_into("<I", footer, 20, zlib.crc32(footer[:16]))
    struct.pack_into("<I", footer, 24, 0x434F4D54)
    return image + b"\xff" * (IMAGE_MAX - len(image)) + footer


class Client:
    def __init__(self, port: str, baud: int = 115200, transport=None):
        if transport is None:
            try:
                import serial
            except ImportError as exc:
                raise RuntimeError("Install pyserial: python -m pip install -r tools/requirements.txt") from exc
            transport = serial.Serial(port, baud, timeout=0.05, write_timeout=2)
        self.serial = transport
        self.parser = Parser()
        self.sequence = 0

    def close(self):
        self.serial.close()

    def request(self, command, payload=b"", timeout=3.0, retries=3):
        self.sequence = (self.sequence + 1) & 0xFFFF
        encoded = encode(command, self.sequence, payload)
        for _ in range(retries):
            self.serial.write(encoded)
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                for response, sequence, data in self.parser.feed(self.serial.read(512)):
                    if response != command | 0x80 or sequence != self.sequence:
                        continue
                    if not data:
                        raise RuntimeError("Reply has no status byte")
                    if data[0]:
                        text = ERRORS[data[0]] if data[0] < len(ERRORS) else str(data[0])
                        raise RuntimeError(f"Command 0x{command:02X}: {text}")
                    return data[1:]
        raise TimeoutError(f"No reply for command 0x{command:02X}")

    def info(self):
        data = self.request(INFO)
        if len(data) not in (20, 40):
            raise RuntimeError("Malformed device information")
        values = struct.unpack("<" + "I" * (len(data) // 4), data)
        return dict(zip(("role", "upgrade_layout", "app_base", "image_max", "chunk_max",
                         "app_valid", "app_version", "stage_valid", "stage_version", "received"), values))

    def status(self):
        data = self.request(STATUS)
        words = struct.unpack_from("<14I", data)
        result = dict(zip(("state", "target_rpm", "rpm", "faults", "duty", "run_requested", "motor_enabled",
                           "sensor_valid", "sensor_errors", "uptime_ms", "config_saved", "shutdown_remaining_ms",
                           "boost_remaining_ms", "rx_dropped"), words))
        result.update(zip(("temperature", "humidity", "gas_voltage", "wind_speed", "pressure"), struct.unpack_from("<5f", data, 56)))
        return result

    def config(self):
        return dict(zip(CONFIG_KEYS, CONFIG.unpack(self.request(GET_CONFIG))))

    def set_config(self, changes):
        config = self.config()
        unknown = changes.keys() - config.keys()
        if unknown:
            raise ValueError(f"Unknown settings: {sorted(unknown)}")
        config.update(changes)
        self.request(SET_CONFIG, CONFIG.pack(*(config[key] for key in CONFIG_KEYS)))
        return config

    def monitor(self):
        result = []
        for task_id, name in enumerate(TASK_NAMES):
            values = struct.unpack("<15I", self.request(MONITOR, bytes([task_id])))
            item = dict(zip(("id", "samples", "period_min_us", "period_max_us", "jitter_max_us", "execution_max_us",
                             "overruns", "cpu_low", "cpu_high", "stack_free_bytes", "heap_free_bytes", "heap_min_bytes",
                             "total_low", "total_high", "jitter_mean_us"), values))
            item["cpu_us"] = item.pop("cpu_low") | item.pop("cpu_high") << 32
            total = item.pop("total_low") | item.pop("total_high") << 32
            item["cpu_percent"] = round(item["cpu_us"] * 100 / total, 2) if total else 0
            item["name"] = name
            result.append(item)
        return result

    def flash(self, data: bytes, progress=lambda *_: None):
        image, version = unpack_package(data)
        info = self.info()
        if info["role"] == 1:
            if not info["upgrade_layout"]:
                raise RuntimeError("Standalone firmware cannot enter this loader; install Upgrade layout with ST-LINK first")
            self.request(STOP)
            deadline = time.monotonic() + 20
            while time.monotonic() < deadline:
                state = self.status()
                if not state["run_requested"] and not state["motor_enabled"] and state["rpm"] < 100 and state["config_saved"]:
                    break
                time.sleep(0.2)
            else:
                raise TimeoutError("Motor has not stopped or configuration has not been saved")
            self.request(BOOT, retries=1)
            time.sleep(2)
            info = self.info()
        if info["role"] != 0 or info["app_base"] != APP_BASE:
            raise RuntimeError("Device is not running the expected bootloader")
        self.request(BEGIN, struct.pack("<III", len(image), zlib.crc32(image), version), timeout=12)
        for offset in range(0, len(image), 252):
            block = image[offset:offset + 252]
            received = struct.unpack("<I", self.request(DATA, struct.pack("<I", offset) + block))[0]
            if received != offset + len(block):
                raise RuntimeError("Unexpected upload offset")
            progress(received, len(image))
        self.request(COMMIT, timeout=12)
        self.request(RUN, timeout=12, retries=1)
        # 安装完成后验证应用确实启动；仅收到 RUN ACK 还不能说明安装成功。
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            time.sleep(1)
            try:
                if self.info()["role"] == 1:
                    return
            except (TimeoutError, RuntimeError):
                pass
        raise TimeoutError("Image committed, but application startup was not confirmed; reconnect to inspect loader")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port")
    parser.add_argument("--baud", type=int, default=115200)
    commands = parser.add_subparsers(dest="command", required=True)
    for name in ("info", "status", "config", "monitor", "stop", "gui"):
        commands.add_parser(name)
    setter = commands.add_parser("set-config")
    setter.add_argument("json", type=Path)
    packer = commands.add_parser("pack")
    packer.add_argument("binary", type=Path)
    packer.add_argument("output", type=Path)
    packer.add_argument("--version", type=int, required=True)
    packer.add_argument("--installed", type=Path, help="Also emit initial ST-LINK application image")
    flashing = commands.add_parser("flash")
    flashing.add_argument("package", type=Path)
    args = parser.parse_args()
    if args.command == "pack":
        image = args.binary.read_bytes()
        args.output.write_bytes(package(image, args.version))
        if args.installed:
            args.installed.write_bytes(installed_image(image, args.version))
        print(f"Packed {len(image)} bytes, version {args.version}")
        return
    if args.command == "gui":
        from hood_gui import run
        run()
        return
    if not args.port:
        parser.error("--port is required for serial commands")
    client = Client(args.port, args.baud)
    try:
        if args.command == "flash":
            client.flash(args.package.read_bytes(), lambda count, total: print(f"\rUpload {count}/{total}", end="", flush=True))
            print("\nApplication startup confirmed")
        elif args.command == "set-config":
            print(json.dumps(client.set_config(json.loads(args.json.read_text(encoding="utf-8"))), indent=2))
        elif args.command == "stop":
            client.request(STOP)
            print("Stop requested")
        else:
            print(json.dumps(getattr(client, args.command)(), indent=2))
    finally:
        client.close()


if __name__ == "__main__":
    main()
