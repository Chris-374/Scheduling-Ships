idf_component_register(
    SRCS
        "../../../main.c"
        "../../../canal.c"
        "../../../ship_tasks.c"
        "../../../ready_queue.c"
        "../../../lcd_display.c"

        "../../../schedulers/scheduler_policy.c"
        "../../../schedulers/scheduler_rr.c"
        "../../../schedulers/scheduler_fcfs.c"
        "../../../schedulers/scheduler_priority.c"
        "../../../schedulers/scheduler_sjf.c"
        "../../../schedulers/scheduler_strn.c"
        "../../../schedulers/scheduler_edf.c"

    INCLUDE_DIRS
        "."
        "../../.."
        "../../../schedulers"

    REQUIRES
        esp_driver_i2c
        esp_driver_gpio
        led_strip
)