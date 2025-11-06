#include "max30102.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "MAX30102";

// I2C дескриптор (глобальний для цього файлу)
static i2c_master_dev_handle_t max30102_dev = NULL;

// ==== ОНОВЛЕНІ I2C ФУНКЦІЇ ДЛЯ MAX30102 =================
static esp_err_t max30102_read_reg(uint8_t reg, uint8_t *value)
{
    if (max30102_dev == NULL || value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = i2c_master_transmit_receive(max30102_dev, &reg, 1, value, 1, -1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C read reg 0x%02X failed: 0x%X", reg, ret);
    }
    return ret;
}

static esp_err_t max30102_write_reg(uint8_t reg, uint8_t value)
{
    if (max30102_dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t write_buf[2] = {reg, value};
    esp_err_t ret = i2c_master_transmit(max30102_dev, write_buf, sizeof(write_buf), -1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C write reg 0x%02X failed: 0x%X", reg, ret);
    }
    return ret;
}

static esp_err_t max30102_read_fifo(uint8_t *buf)
{
    if (max30102_dev == NULL || buf == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t reg = REG_FIFO_DATA;
    esp_err_t ret = i2c_master_transmit_receive(max30102_dev, &reg, 1, buf, 6, -1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C FIFO read failed: 0x%X", ret);
    }
    return ret;
}

// ================== ОНОВЛЕНІ ФУНКЦІЇ MAX30102 =================
esp_err_t max30102_init(void)
{
    ESP_LOGI(TAG, "🔧 Initializing MAX30102 with new I2C driver...");

    // Перевірка зв'язку - читаємо Part ID
    uint8_t part_id;
    esp_err_t ret = max30102_read_reg(REG_PART_ID, &part_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ MAX30102 not responding - check wiring");
        ESP_LOGE(TAG, "🔌 SDA=GPIO%d, SCL=GPIO%d, Addr=0x%02X",
                8, 9, MAX30102_I2C_ADDR);
        return ret;
    }

    if (part_id != 0x15) {
        ESP_LOGW(TAG, "⚠️  Unexpected Part ID: 0x%02X (expected 0x15)", part_id);
    } else {
        ESP_LOGI(TAG, "✅ MAX30102 Part ID: 0x%02X", part_id);
    }

    // Soft reset
    ret = max30102_write_reg(REG_MODE_CONFIG, 0x40);
    if (ret != ESP_OK) return ret;

    vTaskDelay(pdMS_TO_TICKS(100));

    // Чекаємо завершення reset
    uint8_t mode_reg;
    int attempts = 0;
    do {
        vTaskDelay(pdMS_TO_TICKS(10));
        ret = max30102_read_reg(REG_MODE_CONFIG, &mode_reg);
        if (ret != ESP_OK) return ret;
        attempts++;
    } while ((mode_reg & 0x40) != 0 && attempts < 10);

    if (attempts >= 10) {
        ESP_LOGE(TAG, "❌ Sensor reset timeout");
        return ESP_ERR_TIMEOUT;
    }

    // Конфігурація
    ret = max30102_write_reg(REG_INTR_ENABLE_1, 0xC0);
    if (ret != ESP_OK) return ret;

    ret = max30102_write_reg(REG_INTR_ENABLE_2, 0x00);
    if (ret != ESP_OK) return ret;

    // Очищення FIFO
    ret = max30102_write_reg(REG_FIFO_WR_PTR, 0x00);
    if (ret != ESP_OK) return ret;

    ret = max30102_write_reg(REG_OVF_COUNTER, 0x00);
    if (ret != ESP_OK) return ret;

    ret = max30102_write_reg(REG_FIFO_RD_PTR, 0x00);
    if (ret != ESP_OK) return ret;

    // FIFO конфігурація
    ret = max30102_write_reg(REG_FIFO_CONFIG, 0x4F);
    if (ret != ESP_OK) return ret;

    // Режим SpO2
    ret = max30102_write_reg(REG_MODE_CONFIG, MAX30102_MODE_SPO2);
    if (ret != ESP_OK) return ret;

    // SpO2 конфігурація
    ret = max30102_write_reg(REG_SPO2_CONFIG, 0x47);
    if (ret != ESP_OK) return ret;

    // LED потужність
    ret = max30102_write_reg(REG_LED1_PA, 0x2F); // ~12.5mA
    if (ret != ESP_OK) return ret;

    ret = max30102_write_reg(REG_LED2_PA, 0x2F); // ~12.5mA
    if (ret != ESP_OK) return ret;

    ESP_LOGI(TAG, "✅ MAX30102 initialized successfully");
    return ESP_OK;
}

esp_err_t max30102_read_sample(max30102_sample_t *sample)
{
    if (sample == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t buf[6];
    esp_err_t ret = max30102_read_fifo(buf);
    if (ret != ESP_OK) {
        return ret;
    }

    sample->red = ((uint32_t)(buf[0] & 0x03) << 16) | ((uint32_t)buf[1] << 8) | buf[2];
    sample->ir  = ((uint32_t)(buf[3] & 0x03) << 16) | ((uint32_t)buf[4] << 8) | buf[5];

    return ESP_OK;
}

esp_err_t max30102_set_led_current(uint8_t red_hex, uint8_t ir_hex)
{
    esp_err_t e1 = max30102_write_reg(REG_LED1_PA, red_hex);
    esp_err_t e2 = max30102_write_reg(REG_LED2_PA, ir_hex);

    if (e1 == ESP_OK && e2 == ESP_OK) {
        ESP_LOGI(TAG, "💡 LED current: RED=0x%02X, IR=0x%02X", red_hex, ir_hex);
        return ESP_OK;
    }
    return ESP_FAIL;
}

// ================== ДОДАТКОВІ ФУНКЦІЇ =================
esp_err_t max30102_setup_i2c(i2c_master_bus_handle_t bus_handle)
{
    if (bus_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MAX30102_I2C_ADDR,
        .scl_speed_hz = 400000,
    };

    esp_err_t ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &max30102_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to add MAX30102 device: 0x%X", ret);
        return ret;
    }

    ESP_LOGI(TAG, "✅ MAX30102 I2C device configured");
    return ESP_OK;
}

esp_err_t max30102_cleanup(void)
{
    if (max30102_dev != NULL) {
        esp_err_t ret = i2c_master_bus_rm_device(max30102_dev);
        max30102_dev = NULL;
        return ret;
    }
    return ESP_OK;
}