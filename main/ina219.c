#include "ina219.h"
#include "esp_log.h"

static esp_err_t write_reg16(i2c_master_dev_handle_t dev, uint8_t reg, uint16_t val) {
    uint8_t buf[3] = {reg, (uint8_t)(val >> 8), (uint8_t)(val & 0xFF)};
    return i2c_master_transmit(dev, buf, 3, -1);
}

static esp_err_t read_reg16(i2c_master_dev_handle_t dev, uint8_t reg, uint16_t *val) {
    uint8_t buf[2];
    esp_err_t err = i2c_master_transmit_receive(dev, &reg, 1, buf, 2, -1);
    if (err == ESP_OK) {
        *val = (buf[0] << 8) | buf[1];
    }
    return err;
}

esp_err_t ina219_init(i2c_master_dev_handle_t dev) {
    esp_err_t err;
    // Calibration cho R_shunt = 0.1 Ohm, Max Current = 400mA
    // Current_LSB = 0.01mA (10uA) -> Cal = 40960
    err = write_reg16(dev, INA219_REG_CALIBRATION, 40960);
    if (err != ESP_OK) return err;
    
    /**
     * CONFIG REGISTER (0x00):
     * Bits 13: 0 (16V Bus Range - Tăng độ phân giải cho 3.3V)
     * Bits 11-12: 00 (Gain /1, +/- 40mV Range - Tăng độ phân giải cho dòng nhỏ)
     * Bits 7-10: 1111 (Bus ADC: 128 samples averaging)
     * Bits 3-6: 1111 (Shunt ADC: 128 samples averaging)
     * Bits 0-2: 111 (Continuous Mode)
     * Hex: 0x07FF
     */
    return write_reg16(dev, INA219_REG_CONFIG, 0x07FF);
}

esp_err_t ina219_get_current_ma(i2c_master_dev_handle_t dev, float *current_ma) {
    uint16_t val;
    esp_err_t err = read_reg16(dev, INA219_REG_CURRENT, &val);
    if (err == ESP_OK) {
        // Current LSB = 0.01mA
        *current_ma = (int16_t)val * 0.01f;
    }
    return err;
}

esp_err_t ina219_get_bus_voltage_v(i2c_master_dev_handle_t dev, float *voltage_v) {
    uint16_t val;
    esp_err_t err = read_reg16(dev, INA219_REG_BUSVOLTAGE, &val);
    if (err == ESP_OK) {
        *voltage_v = (val >> 3) * 0.004f;
    }
    return err;
}
