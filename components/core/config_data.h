#ifndef CONFIG_DATA_H
#define CONFIG_DATA_H

// Default configuration for ModuChill
// This is a fallback if the system can't load the configuration from flash

#include <stddef.h>

// Default config JSON as a null-terminated string
const char* default_config_json = R"CONFIG_JSON({
    "device": {
        "name": "ModuChill-Controller",
        "id": "MC-001",
        "version": "0.1.0"
    },
    "wifi": {
        "ssid": "",
        "pass": "",
        "ap_name": "ModuChill-Setup",
        "ap_pass": ""
    },
    "web": {
        "port": 80,
        "username": "admin",
        "password": "admin"
    },
    "hardware": {
        "board_type": "test_board",
        "relay1_name": "compressor",
        "relay2_name": "fan",
        "relay3_name": "defrost",
        "relay4_name": "light",
        "ds18b20_1_name": "chamber_temp",
        "ds18b20_2_name": "evaporator_temp",
        "mapping": {
            "compressor": 1,
            "fan": 2,
            "defrost": 3,
            "light": 4,
            "chamber_temp": 8,
            "evaporator_temp": 7
        }
    },
    "control": {
        "set_temp": 4.0,
        "hysteresis": 1.0,
        "min_compressor_off_time": 300,
        "defrost_interval_hours": 8,
        "defrost_duration_minutes": 30,
        "max_runtime_hours": 6
    },
    "sensors": {
        "temp_sensor_type": "ds18b20",
        "temp_read_interval": 5,
        "temp_filtering": true,
        "display_update_interval": 5
    },
    "display": {
        "type": "oled_i2c",
        "i2c_addr": "0x3C",
        "show_status": true,
        "show_temp": true,
        "show_humidity": false,
        "rotation": 0
    },
    "logging": {
        "level": "info",
        "to_flash": true,
        "max_log_size": 10240
    }
})CONFIG_JSON";

// Size of the default config JSON (without the null-terminator)
const size_t default_config_json_size = sizeof(default_config_json) - 1;

#endif // CONFIG_DATA_H