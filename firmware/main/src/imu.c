#include "imu.h"
#include "driver/i2c_master.h"
#include "rom/ets_sys.h"
#include "config_file.h"


static i2c_master_bus_handle_t bus_handle = NULL;
static i2c_master_dev_handle_t dev_handle = NULL;

int init_imu()
{

    //Init i2c protocol with new drivers

    uint8_t connection_status = 0; // This variable will be used to check if the i2c connection is established
   

    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_new_master_bus(&bus_config, &bus_handle));


    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BMI270_ADDR,
        .scl_speed_hz = 400000, // Based on espressif reference, max scl speed shoud be 400khz
    };
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle));
    

    ESP_ERROR_CHECK_WITHOUT_ABORT(imu_read(dev_handle, WHOAMI_ADDR, &connection_status, 1));  // This read operation of register 0x00 shoud return 0x24 (36) value. 

    switch (connection_status) // Switch structure is used to speed up the process. If structure is a bit slower compared to switch structure
    {
    case WHOAMI_VAL:   

        ESP_LOGI("IMU","IMU connected successfully");
        uint8_t init_status = 0;
        
        /* Following process is from the documentation of the BMI270 (initialization)*/

        // Soft reset
        imu_write(dev_handle, CMD, SOFT_RST);
        vTaskDelay(10/portTICK_PERIOD_MS);  // Wait for soft reset to be done

        imu_write(dev_handle, PWR_CONF, 0);
        ets_delay_us(500);
        imu_write(dev_handle, INIT_CTRL, 0);
        imu_burst_write(dev_handle, INIT_DATA ,bmi270_config_file);
        ets_delay_us(5);
        imu_write(dev_handle, INIT_CTRL, 1);
        vTaskDelay(300/portTICK_PERIOD_MS);
        imu_read(dev_handle, INTERNAL_STATUS, &init_status, 1);   
        
        imu_write(dev_handle, PWR_CTRL, 0x0E);
        imu_write(dev_handle, ACC_CONF, 0xA8);
        imu_write(dev_handle, GYR_CONF, 0xA9);
        imu_write(dev_handle, PWR_CONF, 0x02);

    
        return init_status;
        break;
    
    default:

        ESP_LOGI("IMU","IMU setup unsuccessfull");

        ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_bus_rm_device(dev_handle));
        ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_del_master_bus(bus_handle));
    
        return 0;
        break;
    }
        
}

int imu_read(i2c_master_dev_handle_t dev_handle, uint8_t regAddr, uint8_t* data, size_t length)
{
    
    return i2c_master_transmit_receive(dev_handle, &regAddr, 1, data, length, 1000 / portTICK_PERIOD_MS);
}

int imu_write(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = {reg_addr, data}; // Write byte to desired register
    return i2c_master_transmit(dev_handle, write_buf, sizeof(write_buf), 1000 / portTICK_PERIOD_MS);

}

int imu_burst_write(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, const uint8_t* id_data)
{

    uint8_t *write_buf = (uint8_t*) malloc(8193);  //
    write_buf[0] = reg_addr; // First byte is the address where the configuration will be written.
    for (int i = 0; i < 8192; i++)
    {
        write_buf[1 + i] = id_data[i];
    }
    
    int ret = i2c_master_transmit(dev_handle, write_buf, 8193U, 1000 / portTICK_PERIOD_MS);
    
    free(write_buf);

    return ret;

}

int imu_readaccgyro(struct BMI_270_Result *result)
{
    /* Acceleration and gyroscope data received from the IMU */

    uint8_t acc_x[2] = {0};
    uint8_t acc_y[2] = {0};
    uint8_t acc_z[2] = {0};
    uint8_t gyro_x[2] = {0};
    uint8_t gyro_y[2] = {0};
    uint8_t gyro_z[2] = {0};
    uint8_t factor_xz[1] = {0};

    imu_read(dev_handle, ACC_X1, acc_x, 1);
    imu_read(dev_handle, ACC_X2, (acc_x + 1), 1);
    result->acc_x = ((acc_x[1] << 8) | acc_x[0]);

    imu_read(dev_handle, ACC_Y1, acc_y, 1);
    imu_read(dev_handle, ACC_Y2, (acc_y + 1), 1);
    result->acc_y = ((acc_y[1] << 8) | acc_y[0]);

    imu_read(dev_handle, ACC_Z1, acc_z, 1);
    imu_read(dev_handle, ACC_Z2, (acc_z + 1), 1);
    result->acc_z = ((acc_z[1] << 8) | acc_z[0]);

    imu_read(dev_handle, GYRO_X1, gyro_x, 1);
    imu_read(dev_handle, GYRO_X2, (gyro_x + 1), 1);
    result->gyro_x = ((gyro_x[1] << 8) | gyro_x[0]);

    imu_read(dev_handle, GYRO_Y1, gyro_y, 1);
    imu_read(dev_handle, GYRO_Y2, (gyro_y + 1), 1);
    result->gyro_y = ((gyro_y[1] << 8) | gyro_y[0]);

    imu_read(dev_handle, GYRO_Z1, gyro_z, 1);
    imu_read(dev_handle, GYRO_Z2, (gyro_z + 1), 1);
    result->gyro_z = ((gyro_z[1] << 8) | gyro_z[0]);

    // Gyroscope data post-processing according to documentation

    imu_read(dev_handle, GYR_POSTPROC, factor_xz, 1);

    result->gyro_x = result->gyro_x - factor_xz[0]*result->gyro_z;


    //ESP_LOGI("IMU","accx: %lf, accy: %lf, accz: %lf",((int16_t)((acc_x[1] << 8) | acc_x[0]))*g_factor, ((int16_t)((acc_y[1] << 8) | acc_y[0]))*g_factor, ((int16_t)((acc_z[1] << 8) | acc_z[0]))*g_factor);
    

    return 1;
}
