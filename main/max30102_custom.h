#ifndef MAX30102_CUSTOM_H
#define MAX30102_CUSTOM_H

#include "driver/i2c_master.h"
#include "esp_err.h"

#define MAX30102_ADDR 0x57

// A. Nhóm điều khiển hệ thống & ID
#define REG_INTR_STATUS_1 0x00
#define REG_INTR_ENABLE_1 0x02
#define REG_MODE_CONFIG   0x09
#define REG_SPO2_CONFIG   0x0A
#define REG_REV_ID        0xFE
#define REG_PART_ID       0xFF // Default: 0x15

// B. Nhóm cấu hình dòng điện & Proximity
#define REG_LED1_PA       0x0C 
#define REG_LED2_PA       0x0D 
#define REG_PILOT_PA      0x10 
#define REG_PROX_INT_THR  0x30 

// C. Nhóm quản lý dữ liệu FIFO
#define REG_FIFO_WR_PTR   0x04
#define REG_OVF_COUNTER   0x05
#define REG_FIFO_RD_PTR   0x06
#define REG_FIFO_DATA     0x07
#define REG_FIFO_CONFIG   0x08

// D. Nhóm Nhiệt độ
#define REG_TEMP_INT      0x1F
#define REG_TEMP_FRAC     0x20
#define REG_TEMP_EN       0x21

// E. Multi-LED Slot Control
#define REG_MULTI_LED_1   0x11
#define REG_MULTI_LED_2   0x12

typedef struct {
    uint32_t red;
    uint32_t ir;
} max30102_raw_data_t;

typedef struct {
    uint8_t sample_avg;    
    uint8_t mode;          
    uint8_t adc_range;     
    uint8_t sample_rate;   
    uint8_t pulse_width;   
    uint8_t led_current_red; 
    uint8_t led_current_ir;
    uint8_t fifo_roll_over; // 1: Enable, 0: Disable
    uint8_t fifo_almost_full; // 0-15 (số mẫu còn trống để báo ngắt)
} max30102_config_t;

esp_err_t max30102_custom_init(i2c_master_dev_handle_t dev, max30102_config_t *cfg);
esp_err_t max30102_custom_read_fifo(i2c_master_dev_handle_t dev, max30102_raw_data_t *data);
esp_err_t max30102_custom_read_temp(i2c_master_dev_handle_t dev, float *temp);
esp_err_t max30102_custom_set_proximity(i2c_master_dev_handle_t dev, uint8_t proximity_thresh, uint8_t pilot_pa);
esp_err_t max30102_custom_check_id(i2c_master_dev_handle_t dev);

#endif
