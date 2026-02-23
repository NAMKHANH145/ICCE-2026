#include "max30102_custom.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MAX30102_CUSTOM";

static esp_err_t write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(dev, buf, 2, -1);
}

static esp_err_t read_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *val) {
    return i2c_master_transmit_receive(dev, &reg, 1, val, 1, -1);
}

esp_err_t max30102_custom_check_id(i2c_master_dev_handle_t dev) {
    uint8_t id, rev;
    read_reg(dev, REG_PART_ID, &id);
    read_reg(dev, REG_REV_ID, &rev);
    if (id != 0x15) {
        ESP_LOGE(TAG, "Sai thiet bi! Part ID: 0x%02X (Mong doi: 0x15)", id);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Device OK. Part ID: 0x%02X, Rev ID: 0x%02X", id, rev);
    return ESP_OK;
}

esp_err_t max30102_custom_init(i2c_master_dev_handle_t dev, max30102_config_t *cfg) {
    if (max30102_custom_check_id(dev) != ESP_OK) return ESP_FAIL;

    write_reg(dev, REG_MODE_CONFIG, 0x40); // Reset
    vTaskDelay(pdMS_TO_TICKS(100));

    // A. Cấu hình Chế độ & ADC
    write_reg(dev, REG_MODE_CONFIG, cfg->mode); 
    write_reg(dev, REG_SPO2_CONFIG, (cfg->adc_range << 5) | (cfg->sample_rate << 2) | cfg->pulse_width);

    // B. Cấu hình Dòng LED
    write_reg(dev, REG_LED1_PA, cfg->led_current_red);
    write_reg(dev, REG_LED2_PA, cfg->led_current_ir);

    // C. Quản lý FIFO nâng cao
    uint8_t smp_ave_bits = 0;
    if (cfg->sample_avg == 2) smp_ave_bits = 1;
    else if (cfg->sample_avg == 4) smp_ave_bits = 2;
    else if (cfg->sample_avg == 8) smp_ave_bits = 3;
    else if (cfg->sample_avg == 16) smp_ave_bits = 4;
    else if (cfg->sample_avg == 32) smp_ave_bits = 5;

    // FIFO_CONFIG: Sample Avg (7:5), Roll Over (4), Almost Full (3:0)
    write_reg(dev, REG_FIFO_CONFIG, (smp_ave_bits << 5) | (cfg->fifo_roll_over << 4) | (cfg->fifo_almost_full & 0x0F)); 

    // Nếu ở chế độ Multi-LED, cấu hình Slot
    if (cfg->mode == 0x07) {
        write_reg(dev, REG_MULTI_LED_1, 0x21); // Slot 1: Red, Slot 2: IR
    }

    // Reset con trỏ FIFO
    write_reg(dev, REG_FIFO_WR_PTR, 0x00);
    write_reg(dev, REG_OVF_COUNTER, 0x00);
    write_reg(dev, REG_FIFO_RD_PTR, 0x00);
    
    return ESP_OK;
}

esp_err_t max30102_custom_read_fifo(i2c_master_dev_handle_t dev, max30102_raw_data_t *data) {
    uint8_t buf[6];
    uint8_t reg = REG_FIFO_DATA;
    esp_err_t err = i2c_master_transmit_receive(dev, &reg, 1, buf, 6, -1);
    if (err == ESP_OK) {
        data->red = ((uint32_t)buf[0] << 16 | (uint32_t)buf[1] << 8 | buf[2]) & 0x3FFFF;
        data->ir  = ((uint32_t)buf[3] << 16 | (uint32_t)buf[4] << 8 | buf[5]) & 0x3FFFF;
    }
    return err;
}

esp_err_t max30102_custom_read_temp(i2c_master_dev_handle_t dev, float *temp) {
    esp_err_t err;
    err = write_reg(dev, REG_TEMP_EN, 0x01); // Kích hoạt đo
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Loi kich hoat do nhiet do: %s", esp_err_to_name(err));
        return err;
    }
    
    uint8_t en = 1;
    int timeout = 20; // Tang timeout len 200ms
    while (timeout--) {
        err = read_reg(dev, REG_TEMP_EN, &en);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Loi doc trang thai nhiet do: %s", esp_err_to_name(err));
            return err;
        }
        if (!(en & 0x01)) break;
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (en & 0x01) {
        ESP_LOGW(TAG, "Timeout cho nhiet do (TEMP_EN van bang 1)");
    }

    uint8_t temp_buf[2];
    uint8_t reg = REG_TEMP_INT;
    err = i2c_master_transmit_receive(dev, &reg, 1, temp_buf, 2, -1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Loi doc du lieu nhiet do: %s", esp_err_to_name(err));
        return err;
    }
    
    int8_t tint = (int8_t)temp_buf[0];
    uint8_t tfrac = temp_buf[1];
    
    *temp = (float)tint + ((float)tfrac * 0.0625f);
    return ESP_OK;
}

esp_err_t max30102_custom_set_proximity(i2c_master_dev_handle_t dev, uint8_t proximity_thresh, uint8_t pilot_pa) {
    write_reg(dev, REG_PROX_INT_THR, proximity_thresh);
    write_reg(dev, REG_PILOT_PA, pilot_pa);
    return ESP_OK;
}
