idf_component_register(
        SRCS
        "max30102.c"
        "filters.c"
        "hr_algo.c"
        "spo2_algo.c"
        INCLUDE_DIRS "."
        REQUIRES driver esp_timer
)
