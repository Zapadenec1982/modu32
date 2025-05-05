/**
 * @file hal.cpp
 * @brief Реалізація HAL для ModuChill
 */

#include "hal.h"
#include "esp_log.h"
#include "config.h"

static const char* TAG = "HAL";

// Ініціалізація статичних членів
std::map<std::string, gpio_num_t> HAL::component_to_pin_map;
std::map<hal_component_type_t, std::vector<std::string>> HAL::component_type_map;
bool HAL::initialized = false;

esp_err_t HAL::init() {
    if (initialized) {
        ESP_LOGW(TAG, "HAL вже ініціалізовано");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Ініціалізація HAL...");
    
    // Очищення карт зіставлень
    component_to_pin_map.clear();
    component_type_map.clear();
    
    // Ініціалізація пінів GPIO
    
    // 1. Реле
    gpio_config_t relay_config = {
        .pin_bit_mask = (1ULL << BOARD_PINS_CONFIG.relay1_pin) |
                         (1ULL << BOARD_PINS_CONFIG.relay2_pin) |
                         (1ULL << BOARD_PINS_CONFIG.relay3_pin) |
                         (1ULL << BOARD_PINS_CONFIG.relay4_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&relay_config));
    
    // 2. Кнопки
    gpio_config_t button_config = {
        .pin_bit_mask = (1ULL << BOARD_PINS_CONFIG.button1_pin) |
                         (1ULL << BOARD_PINS_CONFIG.button2_pin) |
                         (1ULL << BOARD_PINS_CONFIG.button3_pin) |
                         (1ULL << BOARD_PINS_CONFIG.button4_pin) |
                         (1ULL << BOARD_PINS_CONFIG.button5_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    ESP_ERROR_CHECK(gpio_config(&button_config));
    
    // Початковий стан: всі реле вимкнені
    gpio_set_level(BOARD_PINS_CONFIG.relay1_pin, 0);
    gpio_set_level(BOARD_PINS_CONFIG.relay2_pin, 0);
    gpio_set_level(BOARD_PINS_CONFIG.relay3_pin, 0);
    gpio_set_level(BOARD_PINS_CONFIG.relay4_pin, 0);
    
    // Завантаження конфігурації логічних імен з файлу конфігурації
    // Наприклад, rel1 -> "компресор", rel2 -> "вентилятор", тощо
    
    // Зазвичай ця інформація зберігається в конфігурації системи
    std::string relay1_logical_name = ConfigLoader::get<std::string>("/hardware/relay1_name", "relay1");
    std::string relay2_logical_name = ConfigLoader::get<std::string>("/hardware/relay2_name", "relay2");
    std::string relay3_logical_name = ConfigLoader::get<std::string>("/hardware/relay3_name", "relay3");
    std::string relay4_logical_name = ConfigLoader::get<std::string>("/hardware/relay4_name", "relay4");
    
    // Зіставлення логічних імен з фізичними пінами
    component_to_pin_map[relay1_logical_name] = BOARD_PINS_CONFIG.relay1_pin;
    component_to_pin_map[relay2_logical_name] = BOARD_PINS_CONFIG.relay2_pin;
    component_to_pin_map[relay3_logical_name] = BOARD_PINS_CONFIG.relay3_pin;
    component_to_pin_map[relay4_logical_name] = BOARD_PINS_CONFIG.relay4_pin;
    
    // Групування компонентів за типами
    component_type_map[HAL_COMPONENT_RELAY] = {
        relay1_logical_name, relay2_logical_name, relay3_logical_name, relay4_logical_name
    };
    
    initialized = true;
    ESP_LOGI(TAG, "HAL ініціалізовано успішно");
    return ESP_OK;
}

gpio_num_t HAL::get_pin_for_component(const std::string& logical_name, hal_component_type_t component_type) {
    if (!initialized) {
        ESP_LOGW(TAG, "HAL не ініціалізовано при запиті піна для %s", logical_name.c_str());
        return GPIO_NUM_NC; // No Connection
    }
    
    auto it = component_to_pin_map.find(logical_name);
    if (it != component_to_pin_map.end()) {
        return it->second;
    }
    
    ESP_LOGW(TAG, "Не знайдено пін для компонента %s", logical_name.c_str());
    return GPIO_NUM_NC;
}

esp_err_t HAL::map_component_to_pin(const std::string& logical_name, hal_component_type_t component_type, int pin_index) {
    if (!initialized) {
        ESP_LOGW(TAG, "HAL не ініціалізовано при спробі зіставлення %s", logical_name.c_str());
        return ESP_ERR_INVALID_STATE;
    }
    
    gpio_num_t pin = GPIO_NUM_NC;
    
    // Вибір піна на основі типу компонента та індексу
    switch (component_type) {
        case HAL_COMPONENT_RELAY:
            switch (pin_index) {
                case 1: pin = BOARD_PINS_CONFIG.relay1_pin; break;
                case 2: pin = BOARD_PINS_CONFIG.relay2_pin; break;
                case 3: pin = BOARD_PINS_CONFIG.relay3_pin; break;
                case 4: pin = BOARD_PINS_CONFIG.relay4_pin; break;
                default: return ESP_ERR_INVALID_ARG;
            }
            break;
            
        case HAL_COMPONENT_BUTTON:
            switch (pin_index) {
                case 1: pin = BOARD_PINS_CONFIG.button1_pin; break;
                case 2: pin = BOARD_PINS_CONFIG.button2_pin; break;
                case 3: pin = BOARD_PINS_CONFIG.button3_pin; break;
                case 4: pin = BOARD_PINS_CONFIG.button4_pin; break;
                case 5: pin = BOARD_PINS_CONFIG.button5_pin; break;
                default: return ESP_ERR_INVALID_ARG;
            }
            break;
            
        case HAL_COMPONENT_TEMP_SENSOR:
            switch (pin_index) {
                case 1: pin = BOARD_PINS_CONFIG.ds18b20_pin1; break;
                case 2: pin = BOARD_PINS_CONFIG.ds18b20_pin2; break;
                default: return ESP_ERR_INVALID_ARG;
            }
            break;
            
        default:
            return ESP_ERR_INVALID_ARG;
    }
    
    // Зберігаємо зіставлення
    component_to_pin_map[logical_name] = pin;
    
    // Додаємо компонент до відповідного типу
    auto it = component_type_map.find(component_type);
    if (it != component_type_map.end()) {
        // Видаляємо попередні зіставлення з цим іменем в цьому типі
        it->second.erase(
            std::remove(it->second.begin(), it->second.end(), logical_name),
            it->second.end()
        );
        // Додаємо нове зіставлення
        it->second.push_back(logical_name);
    } else {
        // Створюємо новий запис для цього типу
        component_type_map[component_type] = {logical_name};
    }
    
    ESP_LOGI(TAG, "Зіставлено компонент %s з піном %d", logical_name.c_str(), pin);
    
    // Зберігаємо зіставлення в конфігурації для використання при наступних запусках
    std::string config_path = "/hardware/mapping/" + logical_name;
    ConfigLoader::set<int>(config_path.c_str(), pin);
    
    return ESP_OK;
}
