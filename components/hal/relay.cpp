/**
 * @file relay.cpp
 * @brief Реалізація класу Relay для роботи з реле
 */

#include "relay.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "Relay";

Relay::Relay(gpio_num_t pin, const std::string& name, bool active_low)
    : pin_(pin)
    , name_(name)
    , active_low_(active_low)
    , state_(false)
    , delay_ms_(0)
    , initialized_(false)
{
}

Relay::~Relay() {
    // Вимикаємо реле при знищенні об'єкта для безпеки
    if (initialized_) {
        set_state(false);
    }
}

esp_err_t Relay::init() {
    ESP_LOGI(TAG, "Ініціалізація реле '%s' на піні %d (active_low=%d)", 
             name_.c_str(), pin_, active_low_);
    
    // Перевірка валідності піна
    if (pin_ == GPIO_NUM_NC) {
        ESP_LOGE(TAG, "Невірний пін для реле '%s'", name_.c_str());
        return ESP_ERR_INVALID_ARG;
    }
    
    // Налаштування GPIO як виходу
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin_),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Помилка конфігурації GPIO для реле '%s': %s", 
                 name_.c_str(), esp_err_to_name(ret));
        return ret;
    }
    
    // Встановлюємо початковий стан (вимкнено)
    ret = apply_state(false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Помилка встановлення початкового стану для реле '%s': %s", 
                 name_.c_str(), esp_err_to_name(ret));
        return ret;
    }
    
    initialized_ = true;
    ESP_LOGI(TAG, "Реле '%s' ініціалізовано успішно", name_.c_str());
    return ESP_OK;
}

esp_err_t Relay::set_state(bool state) {
    if (!initialized_) {
        ESP_LOGW(TAG, "Спроба змінити стан неініціалізованого реле '%s'", name_.c_str());
        return ESP_ERR_INVALID_STATE;
    }
    
    // Перевіряємо, чи змінюється стан
    if (state_ == state) {
        return ESP_OK; // Стан не змінюється, нічого не робимо
    }
    
    // Якщо встановлена затримка, чекаємо перед зміною стану
    if (delay_ms_ > 0) {
        ESP_LOGD(TAG, "Затримка %d мс перед зміною стану реле '%s'", 
                 delay_ms_, name_.c_str());
        vTaskDelay(pdMS_TO_TICKS(delay_ms_));
    }
    
    // Застосовуємо новий стан
    esp_err_t ret = apply_state(state);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Помилка зміни стану реле '%s': %s", 
                 name_.c_str(), esp_err_to_name(ret));
        return ret;
    }
    
    // Зберігаємо новий стан
    state_ = state;
    ESP_LOGI(TAG, "Реле '%s' %s", name_.c_str(), state ? "увімкнено" : "вимкнено");
    return ESP_OK;
}

bool Relay::get_state() const {
    return state_;
}

esp_err_t Relay::toggle() {
    return set_state(!state_);
}

std::string Relay::get_type() const {
    return "Relay";
}

std::string Relay::get_name() const {
    return name_;
}

void Relay::set_delay(uint32_t delay_ms) {
    delay_ms_ = delay_ms;
    ESP_LOGI(TAG, "Встановлено затримку %d мс для реле '%s'", 
             delay_ms_, name_.c_str());
}

esp_err_t Relay::apply_state(bool logical_state) {
    // Перетворюємо логічний стан у фізичний з урахуванням active_low
    int level = (logical_state ^ active_low_) ? 1 : 0;
    
    // Встановлюємо рівень на пін
    esp_err_t ret = gpio_set_level(pin_, level);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Помилка gpio_set_level для реле '%s': %s", 
                 name_.c_str(), esp_err_to_name(ret));
    }
    
    return ret;
}
