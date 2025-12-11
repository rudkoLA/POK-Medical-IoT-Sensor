#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

#include "max30102.h"
#include "filters.h"
#include "hr_algo.h"
#include "spo2_algo.h"
#include "esp_event.h"
#include "mqtt_client.h"
#include "certs.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_event.h"

#include "lwip/netdb.h"
#include "freertos/event_groups.h"
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

#define WIFI_SSID "UCU_Guest"
#define WIFI_PASS ""
#define IOTHUB_HOSTNAME "MedicalIotTest.azure-devices.net"
#define DEVICE_ID "myiottest"

static const char *WIFI_TAG = "wifi_sta_app";
static esp_mqtt_client_handle_t mqtt_client = NULL;

static const char *TAG = "APP";

#define I2C_MASTER_SCL_IO 9
#define I2C_MASTER_SDA_IO 8
#define I2C_MASTER_PORT I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 400000

static i2c_master_bus_handle_t i2c_bus_handle = NULL;


#define AMBIENT_SAMPLES 15
#define DETECT_MULTIPLIER 2.0f
#define DETECT_MIN_ABS 2000
#define REMOVE_FRACTION 0.35f
#define STABLE_COUNT 3

#define WARMUP_SAMPLES 150
#define REPORT_EVERY 100
#define LOOP_DELAY_MS 3

#define MIN_AC_DC_ABS 5.0f
#define MIN_AC_DC_RATIO 0.0003f
#define MAX_AC_DC_RATIO 0.15f
#define QUALITY_ALPHA 0.02f

#define DC_DROP_THRESHOLD 0.40f
#define DERIV_LIMIT 0.20f
#define DERIV_SMOOTH_ALPHA 0.05f

#define LED_LOW 0x1F
#define LED_MEDIUM 0x2F
#define LED_HIGH 0x3F

typedef struct
{
    float min_ac_ratio;
    float max_ac_ratio;
    float avg_ac_ratio;
    int total_samples;
    int valid_samples;
    int motion_events;
    int quality_events;
    int hr_updates;
    int spo2_updates;
    int64_t start_time_us;
    float current_quality;
} debug_stats_t;

static debug_stats_t debug_stats = {0};

static hr_algo_t hr;
static spo2_algo_t spo2;

static esp_err_t i2c_init(void)
{
    ESP_LOGI(TAG, "Initializing I2C master...");

    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_MASTER_PORT,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&i2c_bus_config, &i2c_bus_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "❌ Failed to initialize I2C bus: 0x%X", ret);
        return ret;
    }

    ret = max30102_setup_i2c(i2c_bus_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "❌ Failed to setup MAX30102 device");
        i2c_del_master_bus(i2c_bus_handle);
        i2c_bus_handle = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "✅ I2C master and MAX30102 device initialized successfully");
    return ESP_OK;
}

static void reset_filters(ma_filter_ac_t *ir_ma, ma_filter_ac_t *red_ma,
                          ma_filter_dc_t *ir_dc_ma, ma_filter_dc_t *red_dc_ma,
                          highpass_t *hp_ir, highpass_t *hp_red)
{
    if (ir_ma)
        ma_ac_init(ir_ma);
    if (red_ma)
        ma_ac_init(red_ma);
    if (ir_dc_ma)
        ma_dc_init(ir_dc_ma);
    if (red_dc_ma)
        ma_dc_init(red_dc_ma);
    if (hp_ir)
        highpass_init(hp_ir, 0.5f, 100.0f);
    if (hp_red)
        highpass_init(hp_red, 0.5f, 100.0f);
}

static uint8_t adaptive_led_control(float ac_ratio, float ac_amplitude, float dc_value)
{
    if (ac_amplitude < 10.0f || ac_ratio < 0.0005f)
    {
        return LED_HIGH;
    }
    else if (ac_amplitude < 30.0f || ac_ratio < 0.001f)
    {
        return LED_MEDIUM;
    }
    else
    {
        return LED_LOW;
    }
}

static bool evaluate_signal_quality(float ac_ratio, float ac_amplitude, float dc_value, bool motion_detected)
{
    bool amplitude_ok = (ac_amplitude >= MIN_AC_DC_ABS);
    bool ratio_ok = (ac_ratio >= MIN_AC_DC_RATIO) && (ac_ratio <= MAX_AC_DC_RATIO);
    bool dc_ok = (dc_value >= 10000.0f) && (dc_value <= 200000.0f);

    return amplitude_ok && ratio_ok && dc_ok && !motion_detected;
}

static inline void update_debug_stats(float ac_ratio, bool is_valid, bool is_motion, float signal_quality)
{
    debug_stats.total_samples++;
    debug_stats.current_quality = signal_quality;

    if (is_valid)
    {
        debug_stats.valid_samples++;
    }
    else
    {
        debug_stats.quality_events++;
    }

    if (is_motion)
    {
        debug_stats.motion_events++;
    }

    if (ac_ratio > 0.0001f)
    {
        if (debug_stats.avg_ac_ratio == 0)
        {
            debug_stats.avg_ac_ratio = ac_ratio;
            debug_stats.min_ac_ratio = ac_ratio;
            debug_stats.max_ac_ratio = ac_ratio;
        }
        else
        {
            debug_stats.avg_ac_ratio = QUALITY_ALPHA * ac_ratio +
                                       (1 - QUALITY_ALPHA) * debug_stats.avg_ac_ratio;

            if (ac_ratio < debug_stats.min_ac_ratio)
            {
                debug_stats.min_ac_ratio = ac_ratio;
            }
            if (ac_ratio > debug_stats.max_ac_ratio)
            {
                debug_stats.max_ac_ratio = ac_ratio;
            }
        }
    }
}

static void print_debug_stats(void)
{
    if (debug_stats.total_samples == 0)
        return;

    float valid_percent = (float)debug_stats.valid_samples / debug_stats.total_samples * 100.0f;
    int64_t uptime_us = esp_timer_get_time() - debug_stats.start_time_us;
    float uptime_sec = uptime_us / 1000000.0f;

    ESP_LOGI(TAG, "Uptime: %.1fs | Samples: %d (%d valid, %.1f%%)",
             uptime_sec, debug_stats.total_samples, debug_stats.valid_samples, valid_percent);

    ESP_LOGI(TAG, "Events: Motion=%d | Quality=%d | HR=%d | SpO2=%d",
             debug_stats.motion_events, debug_stats.quality_events,
             debug_stats.hr_updates, debug_stats.spo2_updates);

    ESP_LOGI(TAG, "AC/DC: Min=%.3f%% Avg=%.3f%% Max=%.3f%% Quality=%.1f%%",
             debug_stats.min_ac_ratio * 100, debug_stats.avg_ac_ratio * 100,
             debug_stats.max_ac_ratio * 100, debug_stats.current_quality * 100);

    if (uptime_sec > 1.0f)
    {
        float samples_per_sec = debug_stats.total_samples / uptime_sec;
        ESP_LOGI(TAG, "Performance: %.1f samples/sec", samples_per_sec);
    }
}

static void reset_debug_stats(void)
{
    memset(&debug_stats, 0, sizeof(debug_stats));
    debug_stats.start_time_us = esp_timer_get_time();
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGI(WIFI_TAG, "Disconnected. Reconnecting...");
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(WIFI_TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL)
    {
        ESP_LOGE(TAG, "❌ Failed to create Wi-Fi event group");
        return;
    }
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_PASS[0] == '\0' ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {.capable = true, .required = false},
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(WIFI_TAG, "Wi-Fi STA initialized");
}
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "✅ MQTT connected to Azure IoT Hub");
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "⚠️ MQTT disconnected");
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "❌ MQTT error occurred");
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "Message published successfully (msg_id=%d)", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "Incoming data: topic=%.*s, data=%.*s",
                 event->topic_len, event->topic,
                 event->data_len, event->data);
        break;
    default:
        ESP_LOGI(TAG, "MQTT event id: %d", event_id);
        break;
    }
}

static void mqtt_app_start(void)
{
    char username[128];
    snprintf(username, sizeof(username),
             "%s/%s/?api-version=2021-04-12",
             IOTHUB_HOSTNAME, DEVICE_ID);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtts://" IOTHUB_HOSTNAME ":8883",
        .credentials.client_id = DEVICE_ID,
        .credentials.username = username,

        .broker.verification.certificate = azure_root_cert,

        .credentials.authentication.certificate = device_cert_pem,

        .credentials.authentication.key = private_key_pem,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    ESP_LOGI(TAG, "Connecting to Azure IoT Hub...");
    esp_err_t err = esp_mqtt_client_start(mqtt_client);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "❌ Failed to start MQTT client: %s", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "MQTT client started successfully");
    }
}

static void send_telemetry_bpm(float bpm)
{
    if (!mqtt_client)
    {
        ESP_LOGE(TAG, "❌ MQTT client not initialized");
        return;
    }

    if (bpm <= 0 || bpm > 300)
    {
        ESP_LOGW(TAG, "Invalid BPM value: %.1f", bpm);
        return;
    }

    char payload[64];
    snprintf(payload, sizeof(payload), "{\"bpm\":%.1f}", bpm);

    int msg_id = esp_mqtt_client_publish(
        mqtt_client,
        "devices/" DEVICE_ID "/messages/events/$.ct=application%2Fjson&$.ce=utf-8",
        payload, 0, 1, 0);

    if (msg_id < 0)
    {
        ESP_LOGE(TAG, "❌ Failed to publish BPM telemetry");
    }
    else
    {
        ESP_LOGI(TAG, "Sent BPM telemetry (msg_id=%d): %s", msg_id, payload);
    }
}


static void send_telemetry_spo2(float spo2_val)
{
    if (!mqtt_client)
    {
        ESP_LOGE(TAG, "❌ MQTT client not initialized");
        return;
    }

    if (spo2_val < 70.0f || spo2_val > 100.0f)
    {
        ESP_LOGW(TAG, "Invalid SpO2 value: %.1f%%. Not sending telemetry.", spo2_val);
        return;
    }

    char payload[64];
    snprintf(payload, sizeof(payload), "{\"spo2\":%.1f}", spo2_val);

    int msg_id = esp_mqtt_client_publish(
        mqtt_client,
        "devices/" DEVICE_ID "/messages/events/$.ct=application%2Fjson&$.ce=utf-8",
        payload, 0, 1, 0);

    if (msg_id < 0)
    {
        ESP_LOGE(TAG, "❌ Failed to publish SpO2 telemetry");
    }
    else
    {
        ESP_LOGI(TAG, "Sent SpO2 telemetry (msg_id=%d): %s", msg_id, payload);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting MAX30102 with Optimized Settings");

    wifi_init_sta();

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "✅ Wi-Fi connected!");
    }
    else
    {
        ESP_LOGE(TAG, "❌ Wi-Fi failed to connect");
        return;
    }

    struct addrinfo *res;
    int err = getaddrinfo("MedicalIotTest.azure-devices.net", NULL, NULL, &res);
    ESP_LOGI(TAG, "DNS lookup result: %d", err);
    if (err == 0)
        freeaddrinfo(res);

    if (i2c_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "❌ I2C initialization failed");
        return;
    }

    if (max30102_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "❌ MAX30102 initialization failed");
        return;
    }

    ESP_LOGI(TAG, "✅ MAX30102 ready");

    mqtt_app_start();
    vTaskDelay(pdMS_TO_TICKS(3000));
    uint8_t current_led = LED_MEDIUM;
    max30102_set_led_current(current_led, current_led);
    ESP_LOGI(TAG, "Initial LED current: 0x%02X", current_led);
    vTaskDelay(pdMS_TO_TICKS(200));

    ESP_LOGI(TAG, "Calibrating ambient light...");
    uint32_t ir_sum = 0, red_sum = 0;
    int ambient_samples = 0;

    for (int i = 0; i < AMBIENT_SAMPLES; ++i)
    {
        max30102_sample_t s;
        if (max30102_read_sample(&s) == ESP_OK)
        {
            ir_sum += s.ir;
            red_sum += s.red;
            ambient_samples++;
        }
        vTaskDelay(pdMS_TO_TICKS(15));
    }

    if (ambient_samples == 0)
    {
        ESP_LOGE(TAG, "❌ No ambient samples received - sensor not working");
        return;
    }

    float ir_baseline = (float)(ir_sum / ambient_samples);
    float red_baseline = (float)(red_sum / ambient_samples);
    float detect_threshold = fmaxf(DETECT_MIN_ABS, ir_baseline * DETECT_MULTIPLIER);

    ESP_LOGI(TAG, "Ambient: IR=%.0f, RED=%.0f | Threshold: %.0f",
             ir_baseline, red_baseline, detect_threshold);

    ESP_LOGI(TAG, "Initializing algorithms...");
    hr_init(&hr);
    spo2_init(&spo2);

    static ma_filter_ac_t ir_ma, red_ma;
    static ma_filter_dc_t ir_dc_ma, red_dc_ma;
    static highpass_t hp_ir, hp_red;
    reset_filters(&ir_ma, &red_ma, &ir_dc_ma, &red_dc_ma, &hp_ir, &hp_red);
    reset_debug_stats();

    bool finger_detected = false;
    int stable_cnt = 0;
    int remove_cnt = 0;
    float baseline_dc = 0.0f;
    float prev_dc = 0.0f;
    float deriv_smooth = 0.0f;
    int samples = 0;
    int ok_streak = 0;
    int debug_counter = 0;
    int consecutive_no_samples = 0;
    int no_signal_counter = 0;
    uint32_t last_led_update = 0;
    float last_good_ac = 0.0f;
    static float spo2_sum = 0.0f;
    static int spo2_count = 0;

    ESP_LOGI(TAG, "Ready! Place finger on sensor...");

    while (1)
    {
        int64_t loop_start = esp_timer_get_time();

        max30102_sample_t s;
        esp_err_t read_result = max30102_read_sample(&s);

        if (read_result != ESP_OK)
        {
            if (++consecutive_no_samples > 50)
            {
                ESP_LOGW(TAG, "Sensor communication issues");
                consecutive_no_samples = 0;
            }
            vTaskDelay(pdMS_TO_TICKS(LOOP_DELAY_MS));
            continue;
        }
        consecutive_no_samples = 0;

        float ir = (float)s.ir;
        float red = (float)s.red;

        if (!finger_detected)
        {
            bool finger_present = (ir > detect_threshold) && (red > detect_threshold * 0.8f);

            if (finger_present)
            {
                if (++stable_cnt >= STABLE_COUNT)
                {
                    finger_detected = true;
                    stable_cnt = 0;
                    remove_cnt = 0;

                    ESP_LOGI(TAG, "Fast warmup...");
                    for (int i = 0; i < WARMUP_SAMPLES; ++i)
                    {
                        max30102_sample_t p;
                        if (max30102_read_sample(&p) == ESP_OK)
                        {
                            ma_dc_update(&ir_dc_ma, (float)p.ir);
                            ma_dc_update(&red_dc_ma, (float)p.red);
                        }
                        vTaskDelay(pdMS_TO_TICKS(5));
                    }

                    baseline_dc = ma_dc_update(&ir_dc_ma, ir);
                    prev_dc = baseline_dc;
                    deriv_smooth = 0.0f;
                    ok_streak = 0;
                    no_signal_counter = 0;
                    last_good_ac = 0.0f;
                    reset_debug_stats();

                    ESP_LOGI(TAG, "✅ FINGER DETECTED! Starting measurement...");
                }
            }
            else
            {
                stable_cnt = 0;
                if ((samples % 300) == 0)
                {
                    ESP_LOGI(TAG, "… waiting: IR=%.0f/%.0f", ir, detect_threshold);
                }
            }
            samples++;
            vTaskDelay(pdMS_TO_TICKS(LOOP_DELAY_MS));
            continue;
        }

        float remove_threshold = detect_threshold * REMOVE_FRACTION;
        if (ir < remove_threshold || red < remove_threshold)
        {
            if (++remove_cnt >= STABLE_COUNT)
            {
                ESP_LOGW(TAG, "Finger removed");
                finger_detected = false;
                remove_cnt = 0;
                print_debug_stats();
                ESP_LOGI(TAG, "Ready for next measurement...");
            }
            vTaskDelay(pdMS_TO_TICKS(LOOP_DELAY_MS));
            continue;
        }
        remove_cnt = 0;

        float ir_dc = ma_dc_update(&ir_dc_ma, ir);
        float red_dc = ma_dc_update(&red_dc_ma, red);
        float ir_ac_raw = ir - ir_dc;
        float red_ac_raw = red - red_dc;

        float ir_ac = highpass_update(&hp_ir, ir_ac_raw);
        float red_ac = highpass_update(&hp_red, red_ac_raw);

        ir_ac = ma_ac_update(&ir_ma, ir_ac);
        red_ac = ma_ac_update(&red_ma, red_ac);

        if (baseline_dc == 0.0f)
        {
            baseline_dc = ir_dc;
        }
        else
        {
            float alpha = 0.001f;
            baseline_dc = (1 - alpha) * baseline_dc + alpha * ir_dc;
        }

        samples++;
        debug_counter++;

        float dc_deriv = (prev_dc > 1000.0f) ? fabsf(ir_dc - prev_dc) / prev_dc : 0.0f;
        deriv_smooth = DERIV_SMOOTH_ALPHA * dc_deriv + (1 - DERIV_SMOOTH_ALPHA) * deriv_smooth;
        prev_dc = ir_dc;

        bool motion_detected = (deriv_smooth > DERIV_LIMIT);

        float ac_ratio = (ir_dc > 0.0f) ? fabsf(ir_ac) / ir_dc : 0.0f;
        float ac_abs = fabsf(ir_ac);
        float signal_quality = fminf(ac_ratio / 0.01f, 1.0f);

        bool good_quality = evaluate_signal_quality(ac_ratio, ac_abs, ir_dc, motion_detected);

        update_debug_stats(ac_ratio, good_quality, motion_detected, signal_quality);

        if (debug_counter - last_led_update > 50)
        {
            uint8_t new_led = adaptive_led_control(ac_ratio, ac_abs, ir_dc);
            if (new_led != current_led)
            {
                max30102_set_led_current(new_led, new_led);
                if (abs((int)new_led - (int)current_led) > 0x10)
                {
                    ESP_LOGI(TAG, "LED: 0x%02X (AC=%.1f, %.3f%%)",
                             new_led, ac_abs, ac_ratio * 100);
                }
                current_led = new_led;
            }
            last_led_update = debug_counter;
        }

        if ((debug_counter % REPORT_EVERY) == 0)
        {
            const char *quality_indicator = good_quality ? "✅" : "❌";
            const char *motion_indicator = motion_detected ? "🚶" : "";

            ESP_LOGI(TAG, "DC=%.0f AC=%.1f(%.3f%%) %s%s LED=0x%02X",
                     ir_dc, ac_abs, ac_ratio * 100,
                     quality_indicator, motion_indicator, current_led);

            if ((debug_counter % (REPORT_EVERY * 3)) == 0)
            {
                print_debug_stats();
            }
        }

        if (!good_quality)
        {
            no_signal_counter++;
            if (no_signal_counter > 150)
            {
                ESP_LOGW(TAG, "Adjust finger position");
                no_signal_counter = 0;
            }
            vTaskDelay(pdMS_TO_TICKS(LOOP_DELAY_MS));
            continue;
        }
        no_signal_counter = 0;

        float ac_change = (last_good_ac > 0.0f) ? fabsf(ac_abs - last_good_ac) / fmaxf(ac_abs, 1.0f) : 0.0f;

        if (ac_change < 0.5f)
        {
            float bpm = 0.0f;
            bool hr_ok = hr_update(&hr, ir_ac, esp_timer_get_time(), &bpm);
            if (hr_ok)
            {
                debug_stats.hr_updates++;

                static int beat_count = 0;
                static int64_t start_time_us = 0;

                if (start_time_us == 0)
                    start_time_us = esp_timer_get_time();
                beat_count++;

                int64_t elapsed_us = esp_timer_get_time() - start_time_us;
                float elapsed_sec = elapsed_us / 1000000.0f;

                if (elapsed_sec >= 60.0f)
                {
                    float bpm_minute = (float)beat_count * 60.0f / elapsed_sec;
                    printf("❤️ BPM (1 min): %.0f\n", bpm_minute);
                    send_telemetry_bpm(bpm_minute);
                    if (spo2_count > 0)
                    {
                        float spo2_average = spo2_sum / spo2_count;
                        printf("🫁SpO2 (1 min avg): %.1f%%\n", spo2_average);
                        send_telemetry_spo2(spo2_average);
                    }

                    beat_count = 0;
                    start_time_us = esp_timer_get_time();
                    spo2_sum = 0.0f;
                    spo2_count = 0;
                }
            }

            float spo2_val = 0.0f;
            bool spo2_ok = spo2_update(&spo2, red_ac, red_dc, ir_ac, ir_dc, &spo2_val);
            if (spo2_ok && spo2_val >= 70.0f && spo2_val <= 100.0f)
            {
                debug_stats.spo2_updates++;
                spo2_sum += spo2_val;
                spo2_count++;
            }
            else
            {
                spo2_ok = false;
            }

            if (hr_ok)
            {
                ok_streak++;
                if (ok_streak >= 2)
                {
                    printf("\n");
                    printf("╔══════════════════════════════╗\n");
                    printf("║        HEALTH MONITOR        ║\n");
                    printf("╠══════════════════════════════╣\n");
                    printf("║ Pulse: %3.0f BPM         ║\n", bpm);
                    if (spo2_ok)
                    {
                        printf("║ SpO2:   %2.0f %%           ║\n", spo2_val);
                    }
                    else
                    {
                        printf("║ SpO2:   -- %%            ║\n");
                    }
                    printf("║ Quality: %2.0f %%           ║\n", signal_quality * 100);
                    printf("╚══════════════════════════════╝\n");
                }
            }
            else if (ok_streak > 0)
            {
                ok_streak = 0;
            }

            last_good_ac = ac_abs;
        }

        int64_t loop_time = esp_timer_get_time() - loop_start;
        int32_t delay_needed = LOOP_DELAY_MS * 1000 - (int32_t)loop_time;

        if (delay_needed > 1000)
        {
            vTaskDelay(pdMS_TO_TICKS(delay_needed / 1000));
        }
    }

    max30102_cleanup();
    if (i2c_bus_handle != NULL)
    {
        i2c_del_master_bus(i2c_bus_handle);
    }
}
