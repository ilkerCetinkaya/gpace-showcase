#ifndef IMU_H
#define IMU_H

#include "driver/i2c_master.h"
#include "common.h"

// BMI270 Register addresses
#define BMI270_ADDR 0x69
#define WHOAMI_ADDR 0x00
#define CMD 0x7E
#define SOFT_RST 0xB6
#define PWR_CTRL 0x7D
#define PWR_CONF 0x7C
#define ACC_CONF 0x40
#define ACC_RANGE 0x41
#define GYR_CONF 0x42
#define GYR_RANGE 0x43
#define INIT_CTRL 0x59
#define INIT_DATA 0x5E
#define GYR_POSTPROC 0x3C
#define INTERNAL_STATUS 0x21

// BMI270 Data Registers
#define ACC_X1 0x0C
#define ACC_X2 0x0D
#define ACC_Y1 0x0E
#define ACC_Y2 0x0F
#define ACC_Z1 0x10
#define ACC_Z2 0x11
#define GYRO_X1 0x12
#define GYRO_X2 0x13
#define GYRO_Y1 0x14
#define GYRO_Y2 0x15
#define GYRO_Z1 0x16
#define GYRO_Z2 0x17

// BMI270 register values
#define WHOAMI_VAL  0x24

// GPIO pin assignments
#define I2C_MASTER_SDA_IO GPIO_NUM_10
#define I2C_MASTER_SCL_IO GPIO_NUM_11

// Result struct to get acceleration and gyroscope samples
struct BMI_270_Result
{
    int16_t acc_x;
    int16_t acc_y;
    int16_t acc_z;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
     
};

// IMU function declarations
int init_imu();
int imu_read(i2c_master_dev_handle_t dev_handle, uint8_t regAddr, uint8_t* data, size_t length);
int imu_write(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t data);
int imu_burst_write(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, const uint8_t* id_data);
int imu_readaccgyro(struct BMI_270_Result *result);


#endif