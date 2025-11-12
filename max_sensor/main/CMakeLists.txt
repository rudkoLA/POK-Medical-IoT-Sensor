idf_component_register(
    SRCS "certs.c" "main.c"
    INCLUDE_DIRS "."
    REQUIRES max30102
    PRIV_REQUIRES esp_event esp_wifi mqtt nvs_flash
)