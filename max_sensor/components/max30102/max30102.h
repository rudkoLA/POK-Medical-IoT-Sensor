#ifndef MAX30102_H
#define MAX30102_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#define MAX30102_I2C_ADDR 0x57

// Register addresses
#define REG_INTR_STATUS_1   0x00
#define REG_INTR_STATUS_2   0x01
#define REG_INTR_ENABLE_1   0x02
#define REG_INTR_ENABLE_2   0x03
#define REG_FIFO_WR_PTR     0x04
#define REG_OVF_COUNTER     0x05
#define REG_FIFO_RD_PTR     0x06
#define REG_FIFO_DATA       0x07
#define REG_FIFO_CONFIG     0x08
#define REG_MODE_CONFIG     0x09
#define REG_SPO2_CONFIG     0x0A
#define REG_LED1_PA         0x0C
#define REG_LED2_PA         0x0D
#define REG_PILOT_PA        0x10
#define REG_PART_ID         0xFF

// Mode configuration
#define MAX30102_MODE_SPO2  0x03

typedef struct {
    uint32_t red;
    uint32_t ir;
} max30102_sample_t;

// Основні функції
esp_err_t max30102_init(void);
esp_err_t max30102_read_sample(max30102_sample_t *sample);
esp_err_t max30102_set_led_current(uint8_t red_hex, uint8_t ir_hex);

// I2C setup/cleanup функції
esp_err_t max30102_setup_i2c(i2c_master_bus_handle_t bus_handle);
esp_err_t max30102_cleanup(void);

#endif // MAX30102_H