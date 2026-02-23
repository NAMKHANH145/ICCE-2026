#ifndef INA219_H
#define INA219_H

#include "driver/i2c_master.h"
#include "esp_err.h"

#define INA219_ADDR 0x40 // Địa chỉ mặc định

// Thanh ghi INA219
#define INA219_REG_CONFIG       0x00
#define INA219_REG_SHUNTVOLTAGE 0x01
#define INA219_REG_BUSVOLTAGE   0x02
#define INA219_REG_POWER        0x03
#define INA219_REG_CURRENT      0x04
#define INA219_REG_CALIBRATION  0x05

esp_err_t ina219_init(i2c_master_dev_handle_t dev);
esp_err_t ina219_get_current_ma(i2c_master_dev_handle_t dev, float *current_ma);
esp_err_t ina219_get_bus_voltage_v(i2c_master_dev_handle_t dev, float *voltage_v);

#endif
