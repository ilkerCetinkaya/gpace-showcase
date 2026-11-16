#include <string.h>

#include "gpace_bmi270.h"
#include "gpace_bmi270_config_blob.h"
#include "testing.h"

#define LOG_CAPACITY 64

typedef struct {
    uint8_t regs[128];
    uint8_t memory[GPACE_BMI270_CONFIG_BLOB_SIZE];
    uint32_t address_bytes;
    uint8_t chip_id;
    int fail_after_writes;
    int writes;
    uint8_t log_reg[LOG_CAPACITY];
    uint8_t log_val[LOG_CAPACITY];
    int log_count;
    int data_writes;
    int corrupt_upload;
} fake_bmi_t;

static int fake_read(void *ctx, uint8_t reg, uint8_t *data, size_t len)
{
    fake_bmi_t *fake = ctx;
    for (size_t i = 0; i < len; i++) {
        data[i] = fake->regs[(reg + i) & 0x7F];
    }
    if (reg == GPACE_BMI270_REG_CHIP_ID) {
        data[0] = fake->chip_id;
    }
    return 0;
}

static int fake_write(void *ctx, uint8_t reg, const uint8_t *data, size_t len)
{
    fake_bmi_t *fake = ctx;
    if (fake->fail_after_writes >= 0 && fake->writes >= fake->fail_after_writes) {
        return -1;
    }
    fake->writes++;
    if (reg == GPACE_BMI270_REG_INIT_DATA) {
        fake->data_writes++;
        memcpy(&fake->memory[fake->address_bytes], data, len);
        if (fake->corrupt_upload) {
            fake->memory[0] ^= 0xFF;
        }
        fake->address_bytes += (uint32_t)len;
        return 0;
    }
    if (len != 1) {
        return -1;
    }
    if (fake->log_count < LOG_CAPACITY) {
        fake->log_reg[fake->log_count] = reg;
        fake->log_val[fake->log_count++] = data[0];
    }
    fake->regs[reg] = data[0];
    if (reg == GPACE_BMI270_REG_INIT_ADDR_0 || reg == GPACE_BMI270_REG_INIT_ADDR_1) {
        fake->address_bytes = (((uint32_t)fake->regs[GPACE_BMI270_REG_INIT_ADDR_1] << 4) | (fake->regs[GPACE_BMI270_REG_INIT_ADDR_0] & 0x0Fu)) * 2u;
    }
    if (reg == GPACE_BMI270_REG_INIT_CTRL && data[0] == 0) {
        fake->address_bytes = 0;
        fake->regs[GPACE_BMI270_REG_INIT_ADDR_0] = 0;
        fake->regs[GPACE_BMI270_REG_INIT_ADDR_1] = 0;
    }
    if (reg == GPACE_BMI270_REG_INIT_CTRL && data[0] == 1) {
        fake->regs[GPACE_BMI270_REG_INTERNAL_STATUS] = memcmp(fake->memory, gpace_bmi270_config_blob, sizeof(fake->memory)) == 0 ? 1 : 2;
    }
    return 0;
}

static void fake_delay(void *ctx, uint32_t us)
{
    (void)ctx;
    (void)us;
}

static void setup(fake_bmi_t *fake, gpace_bmi270_bus_t *bus)
{
    memset(fake, 0, sizeof(*fake));
    fake->chip_id = GPACE_BMI270_CHIP_ID;
    fake->fail_after_writes = -1;
    bus->ctx = fake;
    bus->read = fake_read;
    bus->write = fake_write;
    bus->delay_us = fake_delay;
}

static int first_index_of(const fake_bmi_t *fake, uint8_t reg, uint8_t value)
{
    for (int i = 0; i < fake->log_count; i++) {
        if (fake->log_reg[i] == reg && fake->log_val[i] == value) {
            return i;
        }
    }
    return -1;
}

static void test_blob_has_the_expected_size(void)
{
    CHECK_EQ(GPACE_BMI270_CONFIG_BLOB_SIZE, 8192);
}

static void test_init_follows_the_datasheet_sequence(void)
{
    fake_bmi_t fake;
    gpace_bmi270_bus_t bus;
    gpace_bmi270_config_t config;
    gpace_bmi270_t dev;

    setup(&fake, &bus);
    gpace_bmi270_config_default(&config);
    CHECK_EQ(gpace_bmi270_init(&dev, &bus, &config), GPACE_BMI270_OK);
    CHECK(memcmp(fake.memory, gpace_bmi270_config_blob, sizeof(fake.memory)) == 0);
    CHECK_EQ(fake.data_writes, 1);
    CHECK_EQ(fake.regs[GPACE_BMI270_REG_ACC_CONF], 0xA8);
    CHECK_EQ(fake.regs[GPACE_BMI270_REG_ACC_RANGE], 0x02);
    CHECK_EQ(fake.regs[GPACE_BMI270_REG_GYR_CONF], 0xA9);
    CHECK_EQ(fake.regs[GPACE_BMI270_REG_GYR_RANGE], 0x00);
    CHECK_EQ(fake.regs[GPACE_BMI270_REG_PWR_CTRL], 0x0E);
    CHECK_EQ(fake.regs[GPACE_BMI270_REG_PWR_CONF], 0x02);
    CHECK(first_index_of(&fake, GPACE_BMI270_REG_CMD, GPACE_BMI270_CMD_SOFT_RESET) == 0);
    CHECK(first_index_of(&fake, GPACE_BMI270_REG_INIT_CTRL, 0) < first_index_of(&fake, GPACE_BMI270_REG_INIT_CTRL, 1));
    CHECK(first_index_of(&fake, GPACE_BMI270_REG_INIT_CTRL, 1) < first_index_of(&fake, GPACE_BMI270_REG_PWR_CTRL, 0x0E));
}

static void test_chunked_upload_writes_every_byte_at_the_right_address(void)
{
    const uint16_t chunks[] = { 32, 128, 256, 100 };
    for (size_t i = 0; i < sizeof(chunks) / sizeof(chunks[0]); i++) {
        fake_bmi_t fake;
        gpace_bmi270_bus_t bus;
        gpace_bmi270_config_t config;
        gpace_bmi270_t dev;

        setup(&fake, &bus);
        gpace_bmi270_config_default(&config);
        config.upload_chunk_bytes = chunks[i];
        CHECK_EQ(gpace_bmi270_init(&dev, &bus, &config), GPACE_BMI270_OK);
        CHECK(memcmp(fake.memory, gpace_bmi270_config_blob, sizeof(fake.memory)) == 0);
        CHECK_EQ(fake.data_writes, (8192 + chunks[i] - 1) / chunks[i]);
    }
}

static void test_odd_chunk_size_is_rejected(void)
{
    fake_bmi_t fake;
    gpace_bmi270_bus_t bus;
    gpace_bmi270_config_t config;
    gpace_bmi270_t dev;

    setup(&fake, &bus);
    gpace_bmi270_config_default(&config);
    config.upload_chunk_bytes = 33;
    CHECK_EQ(gpace_bmi270_init(&dev, &bus, &config), GPACE_BMI270_ERR_ARG);
}

static void test_init_reports_failures(void)
{
    fake_bmi_t fake;
    gpace_bmi270_bus_t bus;
    gpace_bmi270_config_t config;
    gpace_bmi270_t dev;

    setup(&fake, &bus);
    fake.chip_id = 0x12;
    gpace_bmi270_config_default(&config);
    CHECK_EQ(gpace_bmi270_init(&dev, &bus, &config), GPACE_BMI270_ERR_CHIP_ID);

    setup(&fake, &bus);
    fake.corrupt_upload = 1;
    CHECK_EQ(gpace_bmi270_init(&dev, &bus, &config), GPACE_BMI270_ERR_INIT_STATUS);

    setup(&fake, &bus);
    fake.fail_after_writes = 3;
    CHECK_EQ(gpace_bmi270_init(&dev, &bus, &config), GPACE_BMI270_ERR_BUS);
}

static void put_sample(fake_bmi_t *fake, int16_t ax, int16_t ay, int16_t az, int16_t gx, int16_t gy, int16_t gz)
{
    const int16_t values[6] = { ax, ay, az, gx, gy, gz };
    for (unsigned i = 0; i < 6; i++) {
        fake->regs[GPACE_BMI270_REG_ACC_X_LSB + 2u * i] = (uint8_t)((uint16_t)values[i] & 0xFFu);
        fake->regs[GPACE_BMI270_REG_ACC_X_LSB + 2u * i + 1u] = (uint8_t)((uint16_t)values[i] >> 8);
    }
}

static void test_burst_read_decodes_little_endian_samples(void)
{
    fake_bmi_t fake;
    gpace_bmi270_bus_t bus;
    gpace_bmi270_config_t config;
    gpace_bmi270_t dev;
    gpace_bmi270_raw_t raw;

    setup(&fake, &bus);
    gpace_bmi270_config_default(&config);
    CHECK_EQ(gpace_bmi270_init(&dev, &bus, &config), GPACE_BMI270_OK);
    put_sample(&fake, 1000, -2000, 4096, 100, -200, 300);
    CHECK_EQ(gpace_bmi270_read_raw(&dev, &raw), GPACE_BMI270_OK);
    CHECK_EQ(raw.acc[0], 1000);
    CHECK_EQ(raw.acc[1], -2000);
    CHECK_EQ(raw.acc[2], 4096);
    CHECK_EQ(raw.gyr[0], 100);
    CHECK_EQ(raw.gyr[1], -200);
    CHECK_EQ(raw.gyr[2], 300);
}

static void test_gyro_cross_axis_correction(void)
{
    fake_bmi_t fake;
    gpace_bmi270_bus_t bus;
    gpace_bmi270_config_t config;
    gpace_bmi270_t dev;
    gpace_bmi270_raw_t raw;

    setup(&fake, &bus);
    fake.regs[GPACE_BMI270_REG_GYR_CAS] = 0x7F;
    gpace_bmi270_config_default(&config);
    CHECK_EQ(gpace_bmi270_init(&dev, &bus, &config), GPACE_BMI270_OK);
    CHECK_EQ(dev.gyr_cas, -1);
    put_sample(&fake, 0, 0, 0, 100, 5, 20480);
    CHECK_EQ(gpace_bmi270_read_raw(&dev, &raw), GPACE_BMI270_OK);
    CHECK_EQ(raw.gyr[0], 140);
    CHECK_EQ(raw.gyr[1], 5);
    CHECK_EQ(raw.gyr[2], 20480);

    setup(&fake, &bus);
    fake.regs[GPACE_BMI270_REG_GYR_CAS] = 0x40;
    CHECK_EQ(gpace_bmi270_init(&dev, &bus, &config), GPACE_BMI270_OK);
    CHECK_EQ(dev.gyr_cas, -64);

    setup(&fake, &bus);
    fake.regs[GPACE_BMI270_REG_GYR_CAS] = 0x05;
    CHECK_EQ(gpace_bmi270_init(&dev, &bus, &config), GPACE_BMI270_OK);
    CHECK_EQ(dev.gyr_cas, 5);
    put_sample(&fake, 0, 0, 0, 100, 0, 5120);
    CHECK_EQ(gpace_bmi270_read_raw(&dev, &raw), GPACE_BMI270_OK);
    CHECK_EQ(raw.gyr[0], 50);

    setup(&fake, &bus);
    fake.regs[GPACE_BMI270_REG_GYR_CAS] = 0x05;
    config.gyr_cas_correction = 0;
    CHECK_EQ(gpace_bmi270_init(&dev, &bus, &config), GPACE_BMI270_OK);
    put_sample(&fake, 0, 0, 0, 100, 0, 5120);
    CHECK_EQ(gpace_bmi270_read_raw(&dev, &raw), GPACE_BMI270_OK);
    CHECK_EQ(raw.gyr[0], 100);
}

static void test_unit_conversion_matches_the_configured_ranges(void)
{
    fake_bmi_t fake;
    gpace_bmi270_bus_t bus;
    gpace_bmi270_config_t config;
    gpace_bmi270_t dev;

    setup(&fake, &bus);
    gpace_bmi270_config_default(&config);
    CHECK_EQ(gpace_bmi270_init(&dev, &bus, &config), GPACE_BMI270_OK);
    CHECK_EQ(gpace_bmi270_acc_to_mg(&dev, 4096), 1000);
    CHECK_EQ(gpace_bmi270_acc_to_mg(&dev, -4096), -1000);
    CHECK_EQ(gpace_bmi270_gyr_to_mdps(&dev, 16384), 1000000);

    config.acc_range = GPACE_BMI270_ACC_2G;
    config.gyr_range = GPACE_BMI270_GYR_250DPS;
    setup(&fake, &bus);
    CHECK_EQ(gpace_bmi270_init(&dev, &bus, &config), GPACE_BMI270_OK);
    CHECK_EQ(fake.regs[GPACE_BMI270_REG_ACC_RANGE], 0x00);
    CHECK_EQ(fake.regs[GPACE_BMI270_REG_GYR_RANGE], 0x03);
    CHECK_EQ(gpace_bmi270_acc_to_mg(&dev, 16384), 1000);
    CHECK_EQ(gpace_bmi270_gyr_to_mdps(&dev, 16384), 125000);
}

void run_bmi270_tests(void)
{
    RUN(test_blob_has_the_expected_size);
    RUN(test_init_follows_the_datasheet_sequence);
    RUN(test_chunked_upload_writes_every_byte_at_the_right_address);
    RUN(test_odd_chunk_size_is_rejected);
    RUN(test_init_reports_failures);
    RUN(test_burst_read_decodes_little_endian_samples);
    RUN(test_gyro_cross_axis_correction);
    RUN(test_unit_conversion_matches_the_configured_ranges);
}
