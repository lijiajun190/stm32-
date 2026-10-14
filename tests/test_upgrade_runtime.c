/* 正式升级引擎的内存 Flash 模型：写入/擦除过程中直接掉电并重新恢复。 */
#include "boot_update.h"
#include "serial_protocol.h"
#include "runtime_monitor.h"
#include "motor_control.h"
#include "control_config.h"
#include "crc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <limits.h>
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { printf("FAIL line %u: %s\n", (unsigned)__LINE__, #x); exit(1); } } while (0)
typedef struct {
    uint8_t bytes[2][BOOT_SLOT_BYTES];
    int cut, writes, torn, erase_cut;
    bool fail;
} Flash_t;
static Flash_t flash, baseline;
static BootUpdate_t update;
static jmp_buf loss;
static bool read_flash(void *ctx, unsigned area, uint32_t offset, void *bytes, uint32_t count)
{
    Flash_t *f = (Flash_t *)ctx;
    if (area > 1 || offset > BOOT_SLOT_BYTES || count > BOOT_SLOT_BYTES - offset) return false;
    memcpy(bytes, f->bytes[area] + offset, count); return true;
}
static bool program_flash(void *ctx, unsigned area, uint32_t offset, uint32_t word)
{
    Flash_t *f = (Flash_t *)ctx; uint8_t data[4]; Protocol_Put32(data, word);
    if (area > 1 || offset % 4U || offset > BOOT_SLOT_BYTES - 4 || f->fail) return false;
    uint8_t *dst = f->bytes[area] + offset;
    if (f->cut >= 0 && f->writes == f->cut) {
        for (int i = 0; i < f->torn; ++i) dst[i] &= data[i];
        longjmp(loss, 1);
    }
    for (unsigned i = 0; i < 4; ++i) {
        if ((~dst[i]) & data[i]) return false;
        dst[i] &= data[i];
    }
    ++f->writes; return true;
}
static bool erase_flash(void *ctx, unsigned area)
{
    Flash_t *f = (Flash_t *)ctx;
    if (area > 1 || f->fail) return false;
    if (f->erase_cut >= 0) {
        memset(f->bytes[area], 0xFF, (unsigned)f->erase_cut); longjmp(loss, 1);
    }
    memset(f->bytes[area], 0xFF, BOOT_SLOT_BYTES); return true;
}
static void initialize(void)
{
    BootBackend_t backend = {0}; backend.context = &flash; backend.read = read_flash;
    backend.program = program_flash; backend.erase = erase_flash;
    BootUpdate_Init(&update, &backend);
}
static void empty(void)
{
    memset(&flash, 0, sizeof(flash)); memset(flash.bytes, 0xFF, sizeof(flash.bytes));
    flash.cut = flash.erase_cut = -1; initialize();
}
static uint8_t image[65];
static BootImage_t description;
static void fixture(uint32_t version)
{
    for (unsigned i = 0; i < sizeof(image); ++i) image[i] = (uint8_t)(i + version);
    Protocol_Put32(image, 0x20020000U); Protocol_Put32(image + 4, BOOT_APP_BASE + 9U);
    description.length = sizeof(image); description.crc = CRC32_Calculate(image, sizeof(image)); description.version = version;
}
static void stage(void)
{
    CHECK(BootUpdate_Begin(&update, &description) == REPLY_OK);
    CHECK(BootUpdate_Data(&update, 0, image, sizeof(image)) == REPLY_OK);
    CHECK(BootUpdate_Commit(&update) == REPLY_OK);
}
static void test_upgrade(void)
{
    empty(); fixture(1); CHECK(!BootUpdate_Recover(&update)); stage();
    CHECK(BootUpdate_ImageValid(&update, BOOT_AREA_STAGE, NULL));
    CHECK(BootUpdate_Commit(&update) == REPLY_OK); /* 重复 COMMIT。 */
    CHECK(BootUpdate_Recover(&update)); BootImage_t installed;
    CHECK(BootUpdate_ImageValid(&update, BOOT_AREA_APP, &installed) && installed.version == 1);
    CHECK(memcmp(image, flash.bytes[0], sizeof(image)) == 0);
    baseline = flash;
    fixture(2); CHECK(BootUpdate_Begin(&update, &description) == REPLY_OK);
    CHECK(BootUpdate_Commit(&update) == REPLY_BAD_STATE);
    CHECK(BootUpdate_Data(&update, 4, image, 4) == REPLY_BAD_ARGUMENT);
    CHECK(BootUpdate_Data(&update, 0, image, 32) == REPLY_OK);
    CHECK(BootUpdate_Data(&update, 0, image, 32) == REPLY_OK);
    CHECK(BootUpdate_Data(&update, 32, image + 32, 33) == REPLY_OK);
    CHECK(BootUpdate_Data(&update, 64, image + 64, 1) == REPLY_OK);
    CHECK(BootUpdate_Begin(&update, &description) == REPLY_OK && update.received == 65);
    CHECK(BootUpdate_Commit(&update) == REPLY_OK && BootUpdate_Recover(&update));
    CHECK(BootUpdate_ImageValid(&update, BOOT_AREA_APP, &installed) && installed.version == 2);
    /* 上传新镜像过程中断电：尚未提交暂存，旧应用仍然可启动。 */
    for (int cut = 0; cut < 17; ++cut) {
        flash = baseline; initialize(); fixture(2);
        CHECK(BootUpdate_Begin(&update, &description) == REPLY_OK);
        flash.writes = 0; flash.cut = cut; flash.torn = cut % 4;
        if (setjmp(loss) == 0) (void)BootUpdate_Data(&update, 0, image, sizeof(image));
        flash.cut = -1; initialize();
        CHECK(!BootUpdate_ImageValid(&update, BOOT_AREA_STAGE, NULL));
        CHECK(BootUpdate_Recover(&update));
        CHECK(BootUpdate_ImageValid(&update, BOOT_AREA_APP, &installed) && installed.version == 1);
    }
    /* 暂存 footer 每个写入边界的半写不能替换旧应用。 */
    for (int cut = 0; cut < 7; ++cut) {
        for (int torn = 0; torn < 4; ++torn) {
            flash = baseline; initialize(); fixture(2);
            CHECK(BootUpdate_Begin(&update, &description) == REPLY_OK);
            CHECK(BootUpdate_Data(&update, 0, image, sizeof(image)) == REPLY_OK);
            flash.writes = 0; flash.cut = cut; flash.torn = torn;
            if (setjmp(loss) == 0) (void)BootUpdate_Commit(&update);
            flash.cut = -1; initialize(); CHECK(BootUpdate_Recover(&update));
            CHECK(BootUpdate_ImageValid(&update, BOOT_AREA_APP, &installed) && installed.version == 1);
        }
    }
    flash = baseline; initialize(); fixture(2); stage(); baseline = flash;
    /* app 内容、footer、暂存安装状态的每个写边界均可在下次上电重装。 */
    for (int cut = 0; cut < 25; ++cut) {
        for (int torn = 0; torn < 4; ++torn) {
            flash = baseline; initialize(); flash.writes = 0; flash.cut = cut; flash.torn = torn;
            if (setjmp(loss) == 0) (void)BootUpdate_Recover(&update);
            flash.cut = -1; initialize(); CHECK(BootUpdate_Recover(&update));
            CHECK(BootUpdate_ImageValid(&update, BOOT_AREA_APP, &installed) && installed.version == 2);
            CHECK(memcmp(flash.bytes[0], image, sizeof(image)) == 0);
        }
    }
    for (unsigned erased = 0; erased <= BOOT_SLOT_BYTES; erased += 4096U) {
        flash = baseline; initialize(); flash.erase_cut = (int)erased;
        if (setjmp(loss) == 0) (void)BootUpdate_Recover(&update);
        flash.erase_cut = -1; initialize(); CHECK(BootUpdate_Recover(&update));
        CHECK(BootUpdate_ImageValid(&update, BOOT_AREA_APP, &installed) && installed.version == 2);
    }
    /* CRC/向量/边界及 Flash 失败。 */
    empty(); fixture(3); description.crc ^= 1; CHECK(BootUpdate_Begin(&update, &description) == REPLY_OK);
    CHECK(BootUpdate_Data(&update, 0, image, sizeof(image)) == REPLY_OK);
    CHECK(BootUpdate_Commit(&update) == REPLY_BAD_CRC && !BootUpdate_Recover(&update));
    empty(); fixture(3); Protocol_Put32(image + 4, 0x08000001U); description.crc = CRC32_Calculate(image, sizeof(image));
    CHECK(BootUpdate_Begin(&update, &description) == REPLY_OK);
    CHECK(BootUpdate_Data(&update, 0, image, sizeof(image)) == REPLY_OK);
    CHECK(BootUpdate_Commit(&update) == REPLY_BAD_ARGUMENT);
    description.length = BOOT_IMAGE_MAX + 1U; CHECK(BootUpdate_Begin(&update, &description) == REPLY_BAD_ARGUMENT);
    description.length = 1; CHECK(BootUpdate_Begin(&update, &description) == REPLY_BAD_ARGUMENT);
    empty(); fixture(4); flash.fail = true; CHECK(BootUpdate_Begin(&update, &description) == REPLY_FLASH_ERROR);
    flash.fail = false; stage(); CHECK(BootUpdate_Abort(&update) == REPLY_OK && !BootUpdate_Recover(&update));
    empty(); fixture(5); stage(); CHECK(BootUpdate_Recover(&update));
    flash.bytes[0][10] ^= 1; CHECK(BootUpdate_Recover(&update)); /* 已安装后应用损坏仍可从暂存恢复。 */
}
static void test_protocol(void)
{
    ProtocolParser_t parser; ProtocolFrame_t source = {0}, target;
    uint8_t bytes[PROTOCOL_FRAME_MAX]; Protocol_Init(&parser);
    source.command = CMD_DATA; source.sequence = 0x1234; source.length = 256;
    for (unsigned i = 0; i < 256; ++i) source.payload[i] = (uint8_t)i;
    uint16_t count = Protocol_Encode(&source, bytes, sizeof(bytes)); CHECK(count == sizeof(bytes));
    CHECK(Protocol_Encode(&source, bytes, 12) == 0);
    for (unsigned i = 0; i < count; ++i) CHECK(Protocol_Feed(&parser, bytes[i], i, &target) == (i == count - 1U));
    CHECK(target.sequence == source.sequence && memcmp(target.payload, source.payload, source.length) == 0);
    bytes[count - 1] ^= 1;
    for (unsigned i = 0; i < count; ++i) CHECK(!Protocol_Feed(&parser, bytes[i], 500 + i, &target));
    CHECK(parser.crc_errors == 1); bytes[count - 1] ^= 1;
    for (unsigned i = 0; i < count; ++i) (void)Protocol_Feed(&parser, bytes[i], 1000 + i, &target);
    CHECK(target.sequence == source.sequence);
    Protocol_Init(&parser); (void)Protocol_Feed(&parser, 0xA5, UINT_MAX - 10U, &target);
    CHECK(!Protocol_Feed(&parser, 0x5A, 2000, &target) && parser.used == 0);
    source.length = 0; count = Protocol_Encode(&source, bytes, sizeof(bytes));
    for (unsigned i = 0; i < count; ++i) (void)Protocol_Feed(&parser, bytes[i], 3000 + i, &target);
    CHECK(target.length == 0);
    CHECK(CRC32_Calculate((const uint8_t *)"123456789", 9) == 0xCBF43926U);
}
static void test_monitor(void)
{
    RuntimeMonitor_t monitor; RuntimeMonitor_Init(&monitor);
    RuntimeMonitor_Register(&monitor, HEARTBEAT_MOTOR, (void *)1);
    RuntimeMonitor_Register(&monitor, HEARTBEAT_IDLE, (void *)2);
    RuntimeMonitor_Switch(&monitor, (void *)1, UINT_MAX - 49U);
    RuntimeMonitor_Switch(&monitor, (void *)2, 50);
    RuntimeMonitor_Account(&monitor, 150);
    CHECK(monitor.tasks[HEARTBEAT_MOTOR].cpu_us == 100);
    CHECK(monitor.tasks[HEARTBEAT_IDLE].cpu_us == 100);
    RuntimeMonitor_Begin(&monitor, HEARTBEAT_MOTOR, 1000, 50000);
    RuntimeMonitor_End(&monitor, HEARTBEAT_MOTOR, 1200, 50000);
    RuntimeMonitor_Begin(&monitor, HEARTBEAT_MOTOR, 51500, 50000);
    RuntimeMonitor_End(&monitor, HEARTBEAT_MOTOR, 111501, 50000);
    Runtime_Task_t *task = &monitor.tasks[HEARTBEAT_MOTOR];
    CHECK(task->samples == 1 && task->period_min_us == 50500 && task->period_max_us == 50500);
    CHECK(task->jitter_max_us == 500 && task->jitter_sum_us == 500);
    CHECK(task->execution_max_us == 60001 && task->overruns == 1);
    RuntimeMonitor_Begin(&monitor, HEARTBEAT_MOTOR, 100000, 50000);
    CHECK(task->period_min_us == 48500 && task->jitter_max_us == 1500);
    RuntimeMonitor_Init(&monitor);
    RuntimeMonitor_Begin(&monitor, HEARTBEAT_UI, UINT_MAX - 999U, 10000);
    RuntimeMonitor_End(&monitor, HEARTBEAT_UI, 0, 10000);
    CHECK(monitor.tasks[HEARTBEAT_UI].execution_max_us == 1000);
    RuntimeMonitor_Begin(&monitor, HEARTBEAT_UI, 9000, 10000);
    CHECK(monitor.tasks[HEARTBEAT_UI].period_max_us == 10000);
}
static void test_slew(void)
{
    MotorControl_Output_t output = MotorControl_ApplyDutySlew(300, 800, 50);
    CHECK(output.applied_duty == 350 && output.limited);
    output = MotorControl_ApplyDutySlew(800, 100, 50); CHECK(output.applied_duty == 720);
    CHECK(MotorControl_ApplyDutySlew(0, 300, 25).applied_duty == 25);
    CHECK(MotorControl_ApplyDutySlew(0, 300, 100).applied_duty == 100);
    CHECK(MotorControl_ApplyDutySlew(300, 320, 50).applied_duty == 320);
    CHECK(MotorControl_ApplyDutySlew(300, 800, 0).applied_duty == 300);
    MotorControl_t control; MotorControl_Input_t input = {0}; SensorData_t sensor = {0};
    sensor.valid_flags = SENSOR_VALID_SHT30; sensor.temperature_raw = sensor.temperature = 30;
    input.sensor = &sensor; input.target_rpm = 600; MotorControl_Init(&control);
    uint16_t previous = 0;
    for (unsigned t = 0; t < 500; t += 50) {
        input.now_ms = input.command_timestamp_ms = sensor.timestamp_ms = t;
        uint16_t duty = MotorControl_Step(&control, &input);
        CHECK(duty >= previous && duty - previous <= CONTROL_DUTY_SLEW_UP);
        previous = duty;
    }
    input.target_rpm = 0; input.now_ms = sensor.timestamp_ms = 500;
    CHECK(MotorControl_Step(&control, &input) == 0);
    control.duty = 800; MotorControl_LatchFault(&control, CONTROL_FAULT_STALL);
    CHECK(control.duty == 0);
}
int main(void)
{
    test_upgrade(); test_protocol(); test_monitor(); test_slew();
    printf("PASS: %u upgrade/protocol/runtime/PWM-slew checks\n", checks); return 0;
}
