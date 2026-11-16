#include "gpace_hal.h"

#include <stdlib.h>
#include <string.h>

#include "driver/i2c_master.h"
#include "driver/uart.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gpace_board.h"

#define GNSS_UART UART_NUM_1
#define GNSS_RX_BUFFER_BYTES 1024
#define IMU_BUS_FREQ_HZ 400000
#define IMU_TIMEOUT_MS 100
#define IMU_CONFIG_TRANSFER_BYTES 8192

static i2c_master_bus_handle_t imu_bus;
static i2c_master_dev_handle_t imu_dev;
static uint8_t *imu_scratch;
static bool gnss_installed;

static int gnss_write(void *ctx, const uint8_t *data, size_t len)
{
    (void)ctx;
    return uart_write_bytes(GNSS_UART, data, len);
}

static int gnss_read(void *ctx, uint8_t *data, size_t capacity, uint32_t timeout_ms)
{
    int count;
    (void)ctx;
    count = uart_read_bytes(GNSS_UART, data, capacity, pdMS_TO_TICKS(timeout_ms));
    return count < 0 ? 0 : count;
}

static int gnss_set_baud(void *ctx, uint32_t baud)
{
    (void)ctx;
    return uart_set_baudrate(GNSS_UART, baud) == ESP_OK ? 0 : -1;
}

static void gnss_flush(void *ctx)
{
    (void)ctx;
    uart_flush_input(GNSS_UART);
}

static uint32_t clock_ms(void *ctx)
{
    (void)ctx;
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void sleep_ms(void *ctx, uint32_t ms)
{
    (void)ctx;
    vTaskDelay(pdMS_TO_TICKS(ms));
}

esp_err_t gpace_hal_gnss_link_open(gpace_ubx_link_t *link)
{
    const uart_config_t config = {
        .baud_rate = 460800,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t err;

    if (!gnss_installed) {
        err = uart_driver_install(GNSS_UART, GNSS_RX_BUFFER_BYTES, 0, 0, NULL, 0);
        if (err != ESP_OK) {
            return err;
        }
        gnss_installed = true;
    }
    err = uart_param_config(GNSS_UART, &config);
    if (err != ESP_OK) {
        return err;
    }
    err = uart_set_pin(GNSS_UART, GPACE_PIN_GNSS_UART_TX, GPACE_PIN_GNSS_UART_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        return err;
    }
    memset(link, 0, sizeof(*link));
    link->write = gnss_write;
    link->read = gnss_read;
    link->set_baud = gnss_set_baud;
    link->flush = gnss_flush;
    link->now_ms = clock_ms;
    link->delay_ms = sleep_ms;
    return ESP_OK;
}

static int imu_read(void *ctx, uint8_t reg, uint8_t *data, size_t len)
{
    (void)ctx;
    return i2c_master_transmit_receive(imu_dev, &reg, 1, data, len, IMU_TIMEOUT_MS) == ESP_OK ? 0 : -1;
}

static int imu_write(void *ctx, uint8_t reg, const uint8_t *data, size_t len)
{
    (void)ctx;
    if (len + 1 > IMU_CONFIG_TRANSFER_BYTES + 1) {
        return -1;
    }
    imu_scratch[0] = reg;
    memcpy(&imu_scratch[1], data, len);
    return i2c_master_transmit(imu_dev, imu_scratch, len + 1, IMU_TIMEOUT_MS * 4) == ESP_OK ? 0 : -1;
}

static void imu_delay_us(void *ctx, uint32_t us)
{
    (void)ctx;
    if (us >= 1000) {
        vTaskDelay(pdMS_TO_TICKS((us + 999) / 1000));
    } else {
        esp_rom_delay_us(us);
    }
}

esp_err_t gpace_hal_imu_bus_open(gpace_bmi270_bus_t *bus)
{
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = GPACE_PIN_IMU_SDA,
        .scl_io_num = GPACE_PIN_IMU_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = GPACE_BMI270_I2C_ADDR,
        .scl_speed_hz = IMU_BUS_FREQ_HZ,
    };
    esp_err_t err;

    if (imu_bus != NULL) {
        i2c_master_bus_rm_device(imu_dev);
        i2c_del_master_bus(imu_bus);
        imu_bus = NULL;
        imu_dev = NULL;
    }
    if (imu_scratch == NULL) {
        imu_scratch = malloc(IMU_CONFIG_TRANSFER_BYTES + 1);
        if (imu_scratch == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
    err = i2c_new_master_bus(&bus_config, &imu_bus);
    if (err != ESP_OK) {
        return err;
    }
    err = i2c_master_bus_add_device(imu_bus, &device_config, &imu_dev);
    if (err != ESP_OK) {
        i2c_del_master_bus(imu_bus);
        imu_bus = NULL;
        return err;
    }
    bus->ctx = NULL;
    bus->read = imu_read;
    bus->write = imu_write;
    bus->delay_us = imu_delay_us;
    return ESP_OK;
}
