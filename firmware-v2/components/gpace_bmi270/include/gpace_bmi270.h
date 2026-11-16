#ifndef GPACE_BMI270_H
#define GPACE_BMI270_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GPACE_BMI270_I2C_ADDR 0x69u
#define GPACE_BMI270_CHIP_ID 0x24u

#define GPACE_BMI270_REG_CHIP_ID 0x00u
#define GPACE_BMI270_REG_ACC_X_LSB 0x0Cu
#define GPACE_BMI270_REG_INTERNAL_STATUS 0x21u
#define GPACE_BMI270_REG_ACC_CONF 0x40u
#define GPACE_BMI270_REG_ACC_RANGE 0x41u
#define GPACE_BMI270_REG_GYR_CONF 0x42u
#define GPACE_BMI270_REG_GYR_RANGE 0x43u
#define GPACE_BMI270_REG_INIT_CTRL 0x59u
#define GPACE_BMI270_REG_INIT_ADDR_0 0x5Bu
#define GPACE_BMI270_REG_INIT_ADDR_1 0x5Cu
#define GPACE_BMI270_REG_INIT_DATA 0x5Eu
#define GPACE_BMI270_REG_GYR_CAS 0x3Cu
#define GPACE_BMI270_REG_PWR_CONF 0x7Cu
#define GPACE_BMI270_REG_PWR_CTRL 0x7Du
#define GPACE_BMI270_REG_CMD 0x7Eu
#define GPACE_BMI270_CMD_SOFT_RESET 0xB6u

typedef enum {
    GPACE_BMI270_OK = 0,
    GPACE_BMI270_ERR_BUS = -1,
    GPACE_BMI270_ERR_CHIP_ID = -2,
    GPACE_BMI270_ERR_INIT_STATUS = -3,
    GPACE_BMI270_ERR_ARG = -4
} gpace_bmi270_err_t;

typedef struct {
    void *ctx;
    int (*read)(void *ctx, uint8_t reg, uint8_t *data, size_t len);
    int (*write)(void *ctx, uint8_t reg, const uint8_t *data, size_t len);
    void (*delay_us)(void *ctx, uint32_t us);
} gpace_bmi270_bus_t;

typedef enum {
    GPACE_BMI270_ACC_2G = 0,
    GPACE_BMI270_ACC_4G = 1,
    GPACE_BMI270_ACC_8G = 2,
    GPACE_BMI270_ACC_16G = 3
} gpace_bmi270_acc_range_t;

typedef enum {
    GPACE_BMI270_GYR_2000DPS = 0,
    GPACE_BMI270_GYR_1000DPS = 1,
    GPACE_BMI270_GYR_500DPS = 2,
    GPACE_BMI270_GYR_250DPS = 3,
    GPACE_BMI270_GYR_125DPS = 4
} gpace_bmi270_gyr_range_t;

typedef struct {
    uint8_t acc_odr_code;
    uint8_t gyr_odr_code;
    gpace_bmi270_acc_range_t acc_range;
    gpace_bmi270_gyr_range_t gyr_range;
    uint16_t upload_chunk_bytes;
    uint8_t gyr_cas_correction;
} gpace_bmi270_config_t;

typedef struct {
    int16_t acc[3];
    int16_t gyr[3];
} gpace_bmi270_raw_t;

typedef struct {
    gpace_bmi270_bus_t bus;
    gpace_bmi270_config_t config;
    int8_t gyr_cas;
} gpace_bmi270_t;

void gpace_bmi270_config_default(gpace_bmi270_config_t *config);
int gpace_bmi270_init(gpace_bmi270_t *dev, const gpace_bmi270_bus_t *bus, const gpace_bmi270_config_t *config);
int gpace_bmi270_read_raw(gpace_bmi270_t *dev, gpace_bmi270_raw_t *raw);
int32_t gpace_bmi270_acc_to_mg(const gpace_bmi270_t *dev, int16_t raw);
int32_t gpace_bmi270_gyr_to_mdps(const gpace_bmi270_t *dev, int16_t raw);

#ifdef __cplusplus
}
#endif

#endif
