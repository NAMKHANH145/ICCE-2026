#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "max30102_custom.h"
#include "ina219.h"
#include <math.h>

static const char *TAG = "APP_ESP32D";

#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

/* 
 * ================================================================================
 * BIẾN CẤU HÌNH TRỌNG TÂM CHO CÁC BÀI TEST (CHỈNH SỬA TẠI ĐÂY)
 * ================================================================================
 * Công thức: Dòng điện (mA) ≈ Giá trị Hex * 0.2
 * 
 * BẢNG TRA CỨU DÒNG ĐIỆN LED (0mA - 51mA):
 * --------------------------------------------------------------------------------
 * 0mA:  0x00 | 11mA: 0x37 | 22mA: 0x6E | 33mA: 0xA5 | 44mA: 0xDC
 * 1mA:  0x05 | 12mA: 0x3C | 23mA: 0x73 | 34mA: 0xAA | 45mA: 0xE1
 * 2mA:  0x0A | 13mA: 0x41 | 24mA: 0x78 | 35mA: 0xAF | 46mA: 0xE6
 * 3mA:  0x0F | 14mA: 0x46 | 25mA: 0x7D | 36mA: 0xB4 | 47mA: 0xEB
 * 4mA:  0x14 | 15mA: 0x4B | 26mA: 0x82 | 37mA: 0xB9 | 48mA: 0xF0
 * 5mA:  0x19 | 16mA: 0x50 | 27mA: 0x87 | 38mA: 0xBE | 49mA: 0xF5
 * 6mA:  0x1E | 17mA: 0x55 | 28mA: 0x8C | 39mA: 0xC3 | 50mA: 0xFA
 * 7mA:  0x23 | 18mA: 0x5A | 29mA: 0x91 | 40mA: 0xC8 | 51mA: 0xFF (MAX)
 * 8mA:  0x28 | 19mA: 0x5F | 30mA: 0x96 | 41mA: 0xCD
 * 9mA:  0x2D | 20mA: 0x64 | 31mA: 0x9B | 42mA: 0xD2
 * 10mA: 0x32 | 21mA: 0x69 | 32mA: 0xA0 | 43mA: 0xD7
 * --------------------------------------------------------------------------------
 */
uint8_t TEST_LED_CURRENT_PA = 0x64; 
//bài test 1: ngón trỏ phải
//bài test 2: ngón cái phải
//bài test 3: ngón giữa phải
//bài test 4: ngón trỏ trái
void app_main(void) {
    // 1. Khởi tạo Bus I2C
    i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = I2C_SCL_PIN,
        .sda_io_num = I2C_SDA_PIN,
        .glitch_ignore_cnt = 7,
    };
    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus_handle));

    // 2. Thêm thiết bị MAX30102
    i2c_device_config_t max_dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MAX30102_ADDR,
        .scl_speed_hz = 400000,
    };
    i2c_master_dev_handle_t max_handle;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &max_dev_cfg, &max_handle));

    // 3. Thêm thiết bị INA219
    i2c_device_config_t ina_dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = INA219_ADDR,
        .scl_speed_hz = 400000,
    };
    i2c_master_dev_handle_t ina_handle;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &ina_dev_cfg, &ina_handle));

    // 4. Khởi tạo các cảm biến
    max30102_config_t sensor_cfg = {
        .sample_avg = 4,           
        .mode = 0x03,              // SpO2 Mode
        .adc_range = 0x01,         // 4096nA
        .sample_rate = 0x01,       // 100Hz
        .pulse_width = 0x03,       // 411us
        .led_current_red = TEST_LED_CURRENT_PA,
        .led_current_ir = TEST_LED_CURRENT_PA,
        .fifo_roll_over = 1,
        .fifo_almost_full = 0,
    };

    ESP_ERROR_CHECK(max30102_custom_init(max_handle, &sensor_cfg));
    ESP_ERROR_CHECK(ina219_init(ina_handle));

    vTaskDelay(pdMS_TO_TICKS(15000));

    max30102_raw_data_t raw;
    float temp = 0;
    
    // Doc nhiet do lan dau de dam bao thanh ghi co du lieu
    max30102_custom_read_temp(max_handle, &temp);

    ESP_LOGI(TAG, "He thong san sang. Dang do voi LED_PA = 0x%02X", TEST_LED_CURRENT_PA);
    printf("Time(ms),Red,IR,Temp,Bus_V,Current_mA\n"); 

    float ema_current = 0, ema_bus_v = 0;
    const float alpha_ina = 0.1f; 
    uint32_t last_temp_read = 0;

    while (1) {
        if (max30102_custom_read_fifo(max_handle, &raw) == ESP_OK) {
            
            // 1. Xử lý nhiệt độ định kỳ
            if (pdTICKS_TO_MS(xTaskGetTickCount()) - last_temp_read > 2000) {
                esp_err_t temp_err = max30102_custom_read_temp(max_handle, &temp);
                if (temp_err != ESP_OK) {
                    ESP_LOGE(TAG, "Loi doc nhiet do: %s", esp_err_to_name(temp_err));
                }
                last_temp_read = pdTICKS_TO_MS(xTaskGetTickCount());
            }

            // 2. Xử lý INA219 (Dòng/Áp) với lọc EMA để ổn định số liệu nghiên cứu
            float raw_v = 0, raw_ma = 0;
            ina219_get_bus_voltage_v(ina_handle, &raw_v);
            ina219_get_current_ma(ina_handle, &raw_ma);
            
            if (ema_current == 0) {
                ema_current = raw_ma;
                ema_bus_v = raw_v;
            } else {
                ema_current = (alpha_ina * raw_ma) + (1.0f - alpha_ina) * ema_current;
                ema_bus_v = (alpha_ina * raw_v) + (1.0f - alpha_ina) * ema_bus_v;
            }

            // In dữ liệu thô từ MAX30102 và dữ liệu đã làm mịn từ INA219
            printf("%ld,%lu,%lu,%.2f,%.3f,%.3f\n", 
                   (long)pdTICKS_TO_MS(xTaskGetTickCount()), 
                   raw.red, raw.ir, temp, ema_bus_v, ema_current);
        }
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}
