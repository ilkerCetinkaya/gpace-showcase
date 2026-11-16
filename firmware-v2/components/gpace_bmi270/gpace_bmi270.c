#include "gpace_bmi270.h"

#include "gpace_bmi270_config_blob.h"

#define ODR_100HZ 0x08u
#define ODR_200HZ 0x09u
#define FILTER_PERF_BIT 0x80u
#define BWP_NORMAL (0x02u << 4)
#define INIT_STATUS_MASK 0x0Fu
#define INIT_STATUS_OK 0x01u
#define DEFAULT_UPLOAD_CHUNK 0u
#define MAX_UPLOAD_CHUNK 512u

static int write_reg(gpace_bmi270_t *dev, uint8_t reg, uint8_t value)
{
    return dev->bus.write(dev->bus.ctx, reg, &value, 1) == 0 ? GPACE_BMI270_OK : GPACE_BMI270_ERR_BUS;
}

static int read_regs(gpace_bmi270_t *dev, uint8_t reg, uint8_t *data, size_t len)
{
    return dev->bus.read(dev->bus.ctx, reg, data, len) == 0 ? GPACE_BMI270_OK : GPACE_BMI270_ERR_BUS;
}

static int upload_config(gpace_bmi270_t *dev)
{
    const uint16_t total = GPACE_BMI270_CONFIG_BLOB_SIZE;
    uint16_t chunk = dev->config.upload_chunk_bytes;
    int rc;

    if (chunk == 0 || chunk >= total) {
        return dev->bus.write(dev->bus.ctx, GPACE_BMI270_REG_INIT_DATA, gpace_bmi270_config_blob, total) == 0
                   ? GPACE_BMI270_OK
                   : GPACE_BMI270_ERR_BUS;
    }
    if ((chunk & 1u) != 0 || chunk > MAX_UPLOAD_CHUNK) {
        return GPACE_BMI270_ERR_ARG;
    }
    for (uint16_t offset = 0; offset < total; offset = (uint16_t)(offset + chunk)) {
        const uint16_t words = (uint16_t)(offset / 2u);
        const uint16_t remaining = (uint16_t)(total - offset);
        const uint16_t length = remaining < chunk ? remaining : chunk;
        rc = write_reg(dev, GPACE_BMI270_REG_INIT_ADDR_0, (uint8_t)(words & 0x0Fu));
        if (rc != GPACE_BMI270_OK) {
            return rc;
        }
        rc = write_reg(dev, GPACE_BMI270_REG_INIT_ADDR_1, (uint8_t)(words >> 4));
        if (rc != GPACE_BMI270_OK) {
            return rc;
        }
        if (dev->bus.write(dev->bus.ctx, GPACE_BMI270_REG_INIT_DATA, &gpace_bmi270_config_blob[offset], length) != 0) {
            return GPACE_BMI270_ERR_BUS;
        }
    }
    return GPACE_BMI270_OK;
}

static int wait_init_status(gpace_bmi270_t *dev)
{
    uint8_t status = 0;
    int rc;

    for (int attempt = 0; attempt < 10; attempt++) {
        dev->bus.delay_us(dev->bus.ctx, 20000);
        rc = read_regs(dev, GPACE_BMI270_REG_INTERNAL_STATUS, &status, 1);
        if (rc != GPACE_BMI270_OK) {
            return rc;
        }
        if ((status & INIT_STATUS_MASK) == INIT_STATUS_OK) {
            return GPACE_BMI270_OK;
        }
    }
    return GPACE_BMI270_ERR_INIT_STATUS;
}

void gpace_bmi270_config_default(gpace_bmi270_config_t *config)
{
    config->acc_odr_code = ODR_100HZ;
    config->gyr_odr_code = ODR_200HZ;
    config->acc_range = GPACE_BMI270_ACC_8G;
    config->gyr_range = GPACE_BMI270_GYR_2000DPS;
    config->upload_chunk_bytes = DEFAULT_UPLOAD_CHUNK;
    config->gyr_cas_correction = 1;
}

int gpace_bmi270_init(gpace_bmi270_t *dev, const gpace_bmi270_bus_t *bus, const gpace_bmi270_config_t *config)
{
    uint8_t value = 0;
    int rc;

    if (dev == NULL || bus == NULL || config == NULL || bus->read == NULL || bus->write == NULL || bus->delay_us == NULL) {
        return GPACE_BMI270_ERR_ARG;
    }
    dev->bus = *bus;
    dev->config = *config;
    dev->gyr_cas = 0;

    rc = read_regs(dev, GPACE_BMI270_REG_CHIP_ID, &value, 1);
    if (rc != GPACE_BMI270_OK) {
        return rc;
    }
    if (value != GPACE_BMI270_CHIP_ID) {
        return GPACE_BMI270_ERR_CHIP_ID;
    }

    if ((rc = write_reg(dev, GPACE_BMI270_REG_CMD, GPACE_BMI270_CMD_SOFT_RESET)) != GPACE_BMI270_OK) {
        return rc;
    }
    dev->bus.delay_us(dev->bus.ctx, 10000);
    if ((rc = write_reg(dev, GPACE_BMI270_REG_PWR_CONF, 0x00)) != GPACE_BMI270_OK) {
        return rc;
    }
    dev->bus.delay_us(dev->bus.ctx, 500);
    if ((rc = write_reg(dev, GPACE_BMI270_REG_INIT_CTRL, 0x00)) != GPACE_BMI270_OK) {
        return rc;
    }
    if ((rc = upload_config(dev)) != GPACE_BMI270_OK) {
        return rc;
    }
    if ((rc = write_reg(dev, GPACE_BMI270_REG_INIT_CTRL, 0x01)) != GPACE_BMI270_OK) {
        return rc;
    }
    if ((rc = wait_init_status(dev)) != GPACE_BMI270_OK) {
        return rc;
    }

    if ((rc = write_reg(dev, GPACE_BMI270_REG_PWR_CTRL, 0x0E)) != GPACE_BMI270_OK) {
        return rc;
    }
    if ((rc = write_reg(dev, GPACE_BMI270_REG_ACC_CONF, (uint8_t)(FILTER_PERF_BIT | BWP_NORMAL | config->acc_odr_code))) != GPACE_BMI270_OK) {
        return rc;
    }
    if ((rc = write_reg(dev, GPACE_BMI270_REG_ACC_RANGE, (uint8_t)config->acc_range)) != GPACE_BMI270_OK) {
        return rc;
    }
    if ((rc = write_reg(dev, GPACE_BMI270_REG_GYR_CONF, (uint8_t)(FILTER_PERF_BIT | BWP_NORMAL | config->gyr_odr_code))) != GPACE_BMI270_OK) {
        return rc;
    }
    if ((rc = write_reg(dev, GPACE_BMI270_REG_GYR_RANGE, (uint8_t)config->gyr_range)) != GPACE_BMI270_OK) {
        return rc;
    }
    if ((rc = write_reg(dev, GPACE_BMI270_REG_PWR_CONF, 0x02)) != GPACE_BMI270_OK) {
        return rc;
    }

    if ((rc = read_regs(dev, GPACE_BMI270_REG_GYR_CAS, &value, 1)) != GPACE_BMI270_OK) {
        return rc;
    }
    value &= 0x7Fu;
    dev->gyr_cas = (int8_t)((value & 0x40u) ? (int)value - 128 : (int)value);
    return GPACE_BMI270_OK;
}

static int16_t le16(const uint8_t *p)
{
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

int gpace_bmi270_read_raw(gpace_bmi270_t *dev, gpace_bmi270_raw_t *raw)
{
    uint8_t buffer[12];
    int rc = read_regs(dev, GPACE_BMI270_REG_ACC_X_LSB, buffer, sizeof(buffer));
    if (rc != GPACE_BMI270_OK) {
        return rc;
    }
    for (int axis = 0; axis < 3; axis++) {
        raw->acc[axis] = le16(&buffer[2 * axis]);
        raw->gyr[axis] = le16(&buffer[6 + 2 * axis]);
    }
    if (dev->config.gyr_cas_correction) {
        const int32_t corrected = (int32_t)raw->gyr[0] - ((int32_t)dev->gyr_cas * (int32_t)raw->gyr[2]) / 512;
        if (corrected > INT16_MAX) {
            raw->gyr[0] = INT16_MAX;
        } else if (corrected < INT16_MIN) {
            raw->gyr[0] = INT16_MIN;
        } else {
            raw->gyr[0] = (int16_t)corrected;
        }
    }
    return GPACE_BMI270_OK;
}

int32_t gpace_bmi270_acc_to_mg(const gpace_bmi270_t *dev, int16_t raw)
{
    static const int32_t full_scale_mg[] = { 2000, 4000, 8000, 16000 };
    return (int32_t)(((int64_t)raw * full_scale_mg[dev->config.acc_range]) / 32768);
}

int32_t gpace_bmi270_gyr_to_mdps(const gpace_bmi270_t *dev, int16_t raw)
{
    static const int32_t full_scale_mdps[] = { 2000000, 1000000, 500000, 250000, 125000 };
    return (int32_t)(((int64_t)raw * full_scale_mdps[dev->config.gyr_range]) / 32768);
}
