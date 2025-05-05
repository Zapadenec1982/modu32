/**
 * @file cooling_control.cpp
 * @brief Реалізація модуля керування охолодженням
 */

#include "cooling_control.h"
#include "cooling_control_events.h"
#include "cooling_control_state.h"
#include "esp_log.h"
#include "shared_state.h"
#include "event_bus.h"
#include <ctime>

static const char* TAG = "CoolingControl";

// Конструктор модуля
CoolingControlModule::CoolingControlModule()
    : chamber_temp_sensor_(nullptr),
      compressor_relay_(nullptr),
      fan_relay_(nullptr),
      target_temp_c_(4.0f),  // За замовчуванням 4°C
      hysteresis_c_(1.0f),   // За замовчуванням 1°C
      mode_(OperationMode::AUTO),
      min_compressor_off_time_sec_(300), // 5 хвилин за замовчуванням
      current_chamber_temp_c_(0.0f),
      compressor_running_(false),
      fan_running_(false),
      last_compressor_stop_time_(0),
      last_temp_read_time_(0),
      compressor_on_time_(0),
      compressor_cycles_(0),
      compressor_start_time_(0)
{
    // Нічого не потрібно робити тут
}

// Деструктор модуля
CoolingControlModule::~CoolingControlModule()
{
    stop();
}

// Отримання імені модуля
const char* CoolingControlModule::getName() const
{
    return "cooling_control";
}

// Ініціалізація модуля
esp_err_t CoolingControlModule::init()
{
    ESP_LOGI(TAG, "Ініціалізація модуля");
    
    // Ініціалізація датчиків
    esp_err_t sensor_result = init_sensors();
    if (sensor_result != ESP_OK) {
        ESP_LOGE(TAG, "Помилка ініціалізації датчиків: %s", esp_err_to_name(sensor_result));
        return sensor_result;
    }
    
    // Ініціалізація актуаторів
    esp_err_t actuator_result = init_actuators();
    if (actuator_result != ESP_OK) {
        ESP_LOGE(TAG, "Помилка ініціалізації актуаторів: %s", esp_err_to_name(actuator_result));
        return actuator_result;
    }
    
    // Завантаження конфігурації
    target_temp_c_ = SharedState::get<float>(cooling_state::KEY_TEMP_TARGET, 4.0f);
    hysteresis_c_ = SharedState::get<float>(cooling_state::KEY_TEMP_HYSTERESIS, 1.0f);
    mode_ = static_cast<OperationMode>(SharedState::get<int>(cooling_state::KEY_OPERATION_MODE, static_cast<int>(OperationMode::AUTO)));
    
    // Завантаження статистики, якщо є в SharedState
    compressor_cycles_ = SharedState::get<uint32_t>(cooling_state::KEY_STATS_COMPRESSOR_CYCLES, 0);
    compressor_on_time_ = SharedState::get<uint32_t>(cooling_state::KEY_STATS_COMPRESSOR_RUNTIME, 0);
    
    // Збереження початкового стану в SharedState
    SharedState::set<float>(cooling_state::KEY_TEMP_TARGET, target_temp_c_);
    SharedState::set<float>(cooling_state::KEY_TEMP_HYSTERESIS, hysteresis_c_);
    SharedState::set<int>(cooling_state::KEY_OPERATION_MODE, static_cast<int>(mode_));
    SharedState::set<bool>(cooling_state::KEY_COMPRESSOR_STATE, compressor_running_);
    SharedState::set<bool>(cooling_state::KEY_FAN_STATE, fan_running_);
    
    // Підписка на події
    EventBus::subscribe("SystemStarted", [this](const void* data) {
        ESP_LOGI(TAG, "Отримано подію SystemStarted");
    });
    
    // Підписка на події про зміну режиму від інших модулів
    // Наприклад, коли модуль розморожування вмикається, треба зупинити компресор
    EventBus::subscribe("defrost.started", [this](const void* data) {
        ESP_LOGI(TAG, "Отримано подію defrost.started - зупиняємо охолодження");
        if (compressor_running_) {
            set_compressor_state(false);
        }
    });
    
    // Початкове зчитування температури
    read_temperatures();
    
    ESP_LOGI(TAG, "Модуль успішно ініціалізовано");
    return ESP_OK;
}

// Періодичне оновлення модуля
void CoolingControlModule::tick()
{
    // Зчитування температури з періодичністю (кожні 5 секунд)
    uint32_t current_time = static_cast<uint32_t>(time(nullptr));
    if (current_time - last_temp_read_time_ >= 5) {
        read_temperatures();
        last_temp_read_time_ = current_time;
    }
    
    // Запуск термостатичної логіки, якщо режим AUTO
    if (mode_ == OperationMode::AUTO) {
        run_thermostat_logic();
    }
    
    // Оновлення статистики
    update_compressor_statistics();
}

// Зупинка модуля
void CoolingControlModule::stop()
{
    ESP_LOGI(TAG, "Зупинка модуля");
    
    // Вимкнення компресора і вентилятора перед зупинкою
    if (compressor_relay_) {
        compressor_relay_->set_state(false);
        compressor_running_ = false;
    }
    
    if (fan_relay_) {
        fan_relay_->set_state(false);
        fan_running_ = false;
    }
    
    // Збереження статистики в SharedState
    SharedState::set<uint32_t>(cooling_state::KEY_STATS_COMPRESSOR_CYCLES, compressor_cycles_);
    SharedState::set<uint32_t>(cooling_state::KEY_STATS_COMPRESSOR_RUNTIME, compressor_on_time_);
    
    ESP_LOGI(TAG, "Модуль зупинено");
}

// Генерація схеми UI
esp_err_t CoolingControlModule::get_ui_schema(cJSON* module_schema_parent)
{
    if (!module_schema_parent) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Створення схеми UI для веб-інтерфейсу
    cJSON* module_obj = cJSON_CreateObject();
    if (!module_obj) {
        return ESP_ERR_NO_MEM;
    }
    
    // Додаємо метаінформацію
    cJSON_AddStringToObject(module_obj, "name", "Керування охолодженням");
    cJSON_AddStringToObject(module_obj, "description", "Керування компресором та вентилятором");
    cJSON_AddStringToObject(module_obj, "icon", "snowflake");
    
    // Додаємо секцію статусу
    cJSON* status_obj = cJSON_CreateObject();
    if (status_obj) {
        cJSON_AddStringToObject(status_obj, "type", "status");
        
        // Елементи статусу
        cJSON* status_items = cJSON_CreateArray();
        if (status_items) {
            // Температура камери
            cJSON* temp_item = cJSON_CreateObject();
            if (temp_item) {
                cJSON_AddStringToObject(temp_item, "type", "value");
                cJSON_AddStringToObject(temp_item, "name", "chamber_temp");
                cJSON_AddStringToObject(temp_item, "label", "Температура камери");
                cJSON_AddStringToObject(temp_item, "value_key", cooling_state::KEY_TEMP_CHAMBER);
                cJSON_AddStringToObject(temp_item, "unit", "°C");
                cJSON_AddNumberToObject(temp_item, "precision", 1);
                cJSON_AddItemToArray(status_items, temp_item);
            }
            
            // Стан компресора
            cJSON* compressor_item = cJSON_CreateObject();
            if (compressor_item) {
                cJSON_AddStringToObject(compressor_item, "type", "indicator");
                cJSON_AddStringToObject(compressor_item, "name", "compressor");
                cJSON_AddStringToObject(compressor_item, "label", "Компресор");
                cJSON_AddStringToObject(compressor_item, "value_key", cooling_state::KEY_COMPRESSOR_STATE);
                cJSON_AddItemToArray(status_items, compressor_item);
            }
            
            cJSON_AddItemToObject(status_obj, "items", status_items);
        }
        
        cJSON_AddItemToObject(module_obj, "status", status_obj);
    }
    
    // Додаємо секцію конфігурації
    cJSON* config_obj = cJSON_CreateObject();
    if (config_obj) {
        cJSON_AddStringToObject(config_obj, "type", "config");
        
        // Елементи конфігурації
        cJSON* config_items = cJSON_CreateArray();
        if (config_items) {
            // Цільова температура
            cJSON* target_temp_item = cJSON_CreateObject();
            if (target_temp_item) {
                cJSON_AddStringToObject(target_temp_item, "type", "slider");
                cJSON_AddStringToObject(target_temp_item, "name", "target_temp");
                cJSON_AddStringToObject(target_temp_item, "label", "Цільова температура");
                cJSON_AddStringToObject(target_temp_item, "config_key", "cooling/target_temperature");
                cJSON_AddStringToObject(target_temp_item, "unit", "°C");
                cJSON_AddNumberToObject(target_temp_item, "min", 0);
                cJSON_AddNumberToObject(target_temp_item, "max", 15);
                cJSON_AddNumberToObject(target_temp_item, "step", 0.5);
                cJSON_AddItemToArray(config_items, target_temp_item);
            }
            
            // Гістерезис
            cJSON* hysteresis_item = cJSON_CreateObject();
            if (hysteresis_item) {
                cJSON_AddStringToObject(hysteresis_item, "type", "slider");
                cJSON_AddStringToObject(hysteresis_item, "name", "hysteresis");
                cJSON_AddStringToObject(hysteresis_item, "label", "Гістерезис");
                cJSON_AddStringToObject(hysteresis_item, "config_key", "cooling/hysteresis");
                cJSON_AddStringToObject(hysteresis_item, "unit", "°C");
                cJSON_AddNumberToObject(hysteresis_item, "min", 0.5);
                cJSON_AddNumberToObject(hysteresis_item, "max", 3);
                cJSON_AddNumberToObject(hysteresis_item, "step", 0.1);
                cJSON_AddItemToArray(config_items, hysteresis_item);
            }
            
            cJSON_AddItemToObject(config_obj, "items", config_items);
        }
        
        cJSON_AddItemToObject(module_obj, "config", config_obj);
    }
    
    // Додаємо секцію керування
    cJSON* controls_obj = cJSON_CreateObject();
    if (controls_obj) {
        cJSON_AddStringToObject(controls_obj, "type", "controls");
        
        // Елементи керування
        cJSON* controls_items = cJSON_CreateArray();
        if (controls_items) {
            // Вибір режиму (AUTO, MANUAL, OFF)
            cJSON* mode_select = cJSON_CreateObject();
            if (mode_select) {
                cJSON_AddStringToObject(mode_select, "type", "select");
                cJSON_AddStringToObject(mode_select, "name", "mode");
                cJSON_AddStringToObject(mode_select, "label", "Режим роботи");
                cJSON_AddStringToObject(mode_select, "value_key", cooling_state::KEY_OPERATION_MODE);
                cJSON_AddStringToObject(mode_select, "action", "cooling.set_mode");
                
                // Варіанти вибору
                cJSON* options = cJSON_CreateArray();
                if (options) {
                    cJSON* auto_opt = cJSON_CreateObject();
                    if (auto_opt) {
                        cJSON_AddStringToObject(auto_opt, "label", "Автоматичний");
                        cJSON_AddNumberToObject(auto_opt, "value", static_cast<int>(OperationMode::AUTO));
                        cJSON_AddItemToArray(options, auto_opt);
                    }
                    
                    cJSON* manual_opt = cJSON_CreateObject();
                    if (manual_opt) {
                        cJSON_AddStringToObject(manual_opt, "label", "Ручний");
                        cJSON_AddNumberToObject(manual_opt, "value", static_cast<int>(OperationMode::MANUAL));
                        cJSON_AddItemToArray(options, manual_opt);
                    }
                    
                    cJSON* off_opt = cJSON_CreateObject();
                    if (off_opt) {
                        cJSON_AddStringToObject(off_opt, "label", "Вимкнено");
                        cJSON_AddNumberToObject(off_opt, "value", static_cast<int>(OperationMode::OFF));
                        cJSON_AddItemToArray(options, off_opt);
                    }
                    
                    cJSON_AddItemToObject(mode_select, "options", options);
                }
                
                cJSON_AddItemToArray(controls_items, mode_select);
            }
            
            // Ручне керування компресором (для ручного режиму)
            cJSON* compressor_btn = cJSON_CreateObject();
            if (compressor_btn) {
                cJSON_AddStringToObject(compressor_btn, "type", "toggle");
                cJSON_AddStringToObject(compressor_btn, "name", "compressor_control");
                cJSON_AddStringToObject(compressor_btn, "label", "Компресор");
                cJSON_AddStringToObject(compressor_btn, "value_key", cooling_state::KEY_COMPRESSOR_STATE);
                cJSON_AddStringToObject(compressor_btn, "action", "cooling.set_compressor");
                cJSON_AddStringToObject(compressor_btn, "condition", "mode==1"); // Доступно тільки в ручному режимі
                
                cJSON_AddItemToArray(controls_items, compressor_btn);
            }
            
            // Ручне керування вентилятором (для ручного режиму)
            cJSON* fan_btn = cJSON_CreateObject();
            if (fan_btn) {
                cJSON_AddStringToObject(fan_btn, "type", "toggle");
                cJSON_AddStringToObject(fan_btn, "name", "fan_control");
                cJSON_AddStringToObject(fan_btn, "label", "Вентилятор");
                cJSON_AddStringToObject(fan_btn, "value_key", cooling_state::KEY_FAN_STATE);
                cJSON_AddStringToObject(fan_btn, "action", "cooling.set_fan");
                cJSON_AddStringToObject(fan_btn, "condition", "mode==1"); // Доступно тільки в ручному режимі
                
                cJSON_AddItemToArray(controls_items, fan_btn);
            }
            
            cJSON_AddItemToObject(controls_obj, "items", controls_items);
        }
        
        cJSON_AddItemToObject(module_obj, "controls", controls_obj);
    }
    
    // Додаємо об'єкт модуля до батьківського об'єкта
    cJSON_AddItemToObject(module_schema_parent, "cooling_control", module_obj);
    
    return ESP_OK;
}

// Встановлення цільової температури
esp_err_t CoolingControlModule::set_target_temperature(float temp_c)
{
    if (temp_c < 0.0f || temp_c > 15.0f) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Збереження попереднього значення
    float old_temp = target_temp_c_;
    
    // Оновлення значення
    target_temp_c_ = temp_c;
    
    // Оновлення в SharedState
    SharedState::set<float>(cooling_state::KEY_TEMP_TARGET, target_temp_c_);
    
    // Публікація події
    cooling_events::TargetTemperatureChangedEvent event = {
        .old_temperature = old_temp,
        .new_temperature = target_temp_c_,
        .timestamp = static_cast<uint64_t>(time(nullptr) * 1000),
        .is_manual = true
    };
    
    EventBus::publish(cooling_events::EVENT_TARGET_TEMPERATURE_CHANGED, &event);
    
    ESP_LOGI(TAG, "Встановлено цільову температуру: %.1f°C", target_temp_c_);
    return ESP_OK;
}

// Отримання поточної цільової температури
float CoolingControlModule::get_target_temperature() const
{
    return target_temp_c_;
}

// Встановлення гістерезису
esp_err_t CoolingControlModule::set_hysteresis(float hysteresis_c)
{
    if (hysteresis_c < 0.5f || hysteresis_c > 3.0f) {
        return ESP_ERR_INVALID_ARG;
    }
    
    hysteresis_c_ = hysteresis_c;
    
    // Оновлення в SharedState
    SharedState::set<float>(cooling_state::KEY_TEMP_HYSTERESIS, hysteresis_c_);
    
    ESP_LOGI(TAG, "Встановлено гістерезис: %.1f°C", hysteresis_c_);
    return ESP_OK;
}

// Отримання поточного гістерезису
float CoolingControlModule::get_hysteresis() const
{
    return hysteresis_c_;
}

// Встановлення режиму роботи
esp_err_t CoolingControlModule::set_mode(OperationMode mode)
{
    if (static_cast<int>(mode) < 0 || static_cast<int>(mode) > 2) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Збереження попереднього режиму
    OperationMode old_mode = mode_;
    
    // Встановлення нового режиму
    mode_ = mode;
    
    // Оновлення в SharedState
    SharedState::set<int>(cooling_state::KEY_OPERATION_MODE, static_cast<int>(mode_));
    
    // При переході в режим OFF, вимкнути компресор і вентилятор
    if (mode_ == OperationMode::OFF) {
        if (compressor_running_) {
            set_compressor_state(false);
        }
        
        if (fan_running_) {
            set_fan_state(false);
        }
    }
    
    // Публікація події
    cooling_events::ModeChangedEvent event = {
        .old_mode = static_cast<int>(old_mode),
        .new_mode = static_cast<int>(mode_),
        .timestamp = static_cast<uint64_t>(time(nullptr) * 1000),
        .is_manual = true
    };
    
    EventBus::publish(cooling_events::EVENT_MODE_CHANGED, &event);
    
    ESP_LOGI(TAG, "Встановлено режим роботи: %d", static_cast<int>(mode_));
    return ESP_OK;
}

// Отримання поточного режиму роботи
CoolingControlModule::OperationMode CoolingControlModule::get_mode() const
{
    return mode_;
}

// Встановлення стану компресора
esp_err_t CoolingControlModule::set_compressor_state(bool state)
{
    // Якщо стан не змінюється, нічого не робимо
    if (compressor_running_ == state) {
        return ESP_OK;
    }
    
    // Перевірка мінімального часу вимкнення при спробі увімкнути
    if (state && !is_min_compressor_off_time_elapsed()) {
        ESP_LOGW(TAG, "Неможливо увімкнути компресор: не минув мінімальний час простою (%d сек)", 
                 min_compressor_off_time_sec_);
        return ESP_ERR_NOT_FINISHED;
    }
    
    // Встановлення фізичного стану реле
    if (!compressor_relay_) {
        ESP_LOGE(TAG, "Помилка: реле компресора не ініціалізовано");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t result = compressor_relay_->set_state(state);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Помилка керування реле компресора: %s", esp_err_to_name(result));
        return result;
    }
    
    // Оновлення внутрішнього стану
    compressor_running_ = state;
    
    // Запам'ятати час зупинки або запуску
    uint32_t current_time = static_cast<uint32_t>(time(nullptr));
    if (state) {
        compressor_start_time_ = current_time;
        // Збільшення лічильника циклів при увімкненні
        compressor_cycles_++;
    } else {
        last_compressor_stop_time_ = current_time;
        // Оновлення загального часу роботи при вимкненні
        if (compressor_start_time_ > 0) {
            compressor_on_time_ += (current_time - compressor_start_time_);
        }
    }
    
    // Оновлення стану в SharedState
    SharedState::set<bool>(cooling_state::KEY_COMPRESSOR_STATE, compressor_running_);
    
    // Публікація події про зміну стану компресора
    cooling_events::CompressorStateChangedEvent event = {
        .is_running = compressor_running_,
        .timestamp = static_cast<uint64_t>(current_time * 1000),
        .runtime_sec = (state && compressor_start_time_ > 0) ? 0 : (current_time - compressor_start_time_)
    };
    
    EventBus::publish(cooling_events::EVENT_COMPRESSOR_STATE_CHANGED, &event);
    
    ESP_LOGI(TAG, "Компресор %s", state ? "увімкнено" : "вимкнено");
    return ESP_OK;
}

// Отримання поточного стану компресора
bool CoolingControlModule::is_compressor_running() const
{
    return compressor_running_;
}

// Встановлення стану вентилятора
esp_err_t CoolingControlModule::set_fan_state(bool state)
{
    // Якщо стан не змінюється, нічого не робимо
    if (fan_running_ == state) {
        return ESP_OK;
    }
    
    // Встановлення фізичного стану реле
    if (!fan_relay_) {
        ESP_LOGE(TAG, "Помилка: реле вентилятора не ініціалізовано");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t result = fan_relay_->set_state(state);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Помилка керування реле вентилятора: %s", esp_err_to_name(result));
        return result;
    }
    
    // Оновлення внутрішнього стану
    fan_running_ = state;
    
    // Оновлення стану в SharedState
    SharedState::set<bool>(cooling_state::KEY_FAN_STATE, fan_running_);
    
    // Публікація події про зміну стану вентилятора
    cooling_events::FanStateChangedEvent event = {
        .is_running = fan_running_,
        .timestamp = static_cast<uint64_t>(time(nullptr) * 1000)
    };
    
    EventBus::publish(cooling_events::EVENT_FAN_STATE_CHANGED, &event);
    
    ESP_LOGI(TAG, "Вентилятор %s", state ? "увімкнено" : "вимкнено");
    return ESP_OK;
}

// Отримання поточного стану вентилятора
bool CoolingControlModule::is_fan_running() const
{
    return fan_running_;
}

// Отримання поточної температури камери
float CoolingControlModule::get_chamber_temperature() const
{
    return current_chamber_temp_c_;
}

// Зчитування температури з датчиків
esp_err_t CoolingControlModule::read_temperatures()
{
    // Перевірка ініціалізації датчика
    if (!chamber_temp_sensor_) {
        ESP_LOGW(TAG, "Датчик температури камери не ініціалізовано");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Зчитування значення температури
    float chamber_temp = 0.0f;
    esp_err_t result = chamber_temp_sensor_->read(&chamber_temp);
    
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Помилка зчитування датчика температури камери: %s", esp_err_to_name(result));
        return result;
    }
    
    // Збереження попереднього значення для порівняння
    float prev_temp = current_chamber_temp_c_;
    
    // Оновлення внутрішнього стану
    current_chamber_temp_c_ = chamber_temp;
    
    // Оновлення SharedState
    SharedState::set<float>(cooling_state::KEY_TEMP_CHAMBER, chamber_temp);
    
    // Якщо температура змінилася суттєво (більше 0.1°C), публікуємо подію
    if (std::abs(prev_temp - chamber_temp) > 0.1f) {
        cooling_events::TemperatureChangedEvent event = {
            .temperature = chamber_temp,
            .timestamp = static_cast<uint64_t>(time(nullptr) * 1000)
        };
        
        EventBus::publish(cooling_events::EVENT_TEMPERATURE_CHANGED, &event);
        
        // Логування при значній зміні (більше 0.5°C)
        if (std::abs(prev_temp - chamber_temp) > 0.5f) {
            ESP_LOGI(TAG, "Температура камери: %.1f°C", chamber_temp);
        }
        
        // Оновлення середньої температури
        float avg_temp = SharedState::get<float>(cooling_state::KEY_STATS_AVG_TEMPERATURE, chamber_temp);
        avg_temp = (avg_temp * 0.9f) + (chamber_temp * 0.1f); // Просте згладжування
        SharedState::set<float>(cooling_state::KEY_STATS_AVG_TEMPERATURE, avg_temp);
    }
    
    return ESP_OK;
}

// Виконання термостатичної логіки
esp_err_t CoolingControlModule::run_thermostat_logic()
{
    // Якщо режим не AUTO або активне розморожування, не виконуємо термостатну логіку
    if (mode_ != OperationMode::AUTO) {
        return ESP_OK;
    }
    
    // Перевіряємо, чи ініціалізовано реле компресора
    if (!compressor_relay_) {
        ESP_LOGW(TAG, "Реле компресора не ініціалізовано");
        return ESP_ERR_NOT_FOUND;
    }
    
    // Логіка термостата з гістерезисом
    if (compressor_running_) {
        // Компресор працює, перевіряємо, чи треба вимкнути
        if (current_chamber_temp_c_ <= target_temp_c_) {
            // Досягнуто цільову температуру, вимикаємо компресор
            ESP_LOGI(TAG, "Досягнуто цільову температуру %.1f°C, вимикаємо компресор", target_temp_c_);
            set_compressor_state(false);
            
            // Збільшуємо лічильник циклів компресора
            compressor_cycles_++;
            SharedState::set<uint32_t>(cooling_state::KEY_STATS_COMPRESSOR_CYCLES, compressor_cycles_);
        }
    } else {
        // Компресор вимкнений, перевіряємо, чи треба увімкнути
        if (current_chamber_temp_c_ >= (target_temp_c_ + hysteresis_c_)) {
            // Перевіряємо, чи минув мінімальний час вимкнення компресора
            if (is_min_compressor_off_time_elapsed()) {
                // Температура вище цільової + гістерезис, вмикаємо компресор
                ESP_LOGI(TAG, "Температура %.1f°C перевищує поріг %.1f°C, вмикаємо компресор",
                         current_chamber_temp_c_, target_temp_c_ + hysteresis_c_);
                
                set_compressor_state(true);
                
                // Також вмикаємо вентилятор разом з компресором
                if (fan_relay_ && !fan_running_) {
                    set_fan_state(true);
                }
            } else {
                ESP_LOGD(TAG, "Потрібно увімкнути компресор, але не минув мінімальний час вимкнення");
            }
        }
    }
    
    return ESP_OK;
}

// Ініціалізація актуаторів
esp_err_t CoolingControlModule::init_actuators()
{
    ESP_LOGI(TAG, "Ініціалізація актуаторів");
    
    esp_err_t result = ESP_OK;
    
    // Отримуємо піни для актуаторів з HAL
    gpio_num_t compressor_pin = HAL::get_pin_for_component("compressor", HAL_COMPONENT_RELAY);
    gpio_num_t fan_pin = HAL::get_pin_for_component("fan", HAL_COMPONENT_RELAY);
    
    // Ініціалізуємо реле компресора
    if (compressor_pin != GPIO_NUM_NC) {
        compressor_relay_ = std::make_unique<Relay>(compressor_pin, "compressor");
        esp_err_t compressor_result = compressor_relay_->init();
        if (compressor_result != ESP_OK) {
            ESP_LOGE(TAG, "Помилка ініціалізації реле компресора: %s", esp_err_to_name(compressor_result));
            result = compressor_result;
        } else {
            ESP_LOGI(TAG, "Реле компресора ініціалізовано на піні %d", compressor_pin);
            
            // Встановлюємо мінімальний час вимкнення для захисту компресора
            compressor_relay_->set_delay(min_compressor_off_time_sec_ * 1000);
        }
    } else {
        ESP_LOGW(TAG, "Не знайдено пін для реле компресора");
    }
    
    // Ініціалізуємо реле вентилятора
    if (fan_pin != GPIO_NUM_NC) {
        fan_relay_ = std::make_unique<Relay>(fan_pin, "fan");
        esp_err_t fan_result = fan_relay_->init();
        if (fan_result != ESP_OK) {
            ESP_LOGE(TAG, "Помилка ініціалізації реле вентилятора: %s", esp_err_to_name(fan_result));
            if (result == ESP_OK) {
                result = fan_result;
            }
        } else {
            ESP_LOGI(TAG, "Реле вентилятора ініціалізовано на піні %d", fan_pin);
        }
    } else {
        ESP_LOGW(TAG, "Не знайдено пін для реле вентилятора");
    }
    
    return result;
}

// Ініціалізація датчиків
esp_err_t CoolingControlModule::init_sensors()
{
    ESP_LOGI(TAG, "Ініціалізація датчиків");
    
    esp_err_t result = ESP_OK;
    
    // Отримуємо піни для датчиків з HAL
    gpio_num_t chamber_temp_pin = HAL::get_pin_for_component("chamber_temp", HAL_COMPONENT_TEMP_SENSOR);
    
    // Ініціалізуємо датчик температури камери
    if (chamber_temp_pin != GPIO_NUM_NC) {
        chamber_temp_sensor_ = std::make_unique<DS18B20Sensor>(chamber_temp_pin, "chamber_temp");
        esp_err_t chamber_result = chamber_temp_sensor_->init();
        if (chamber_result != ESP_OK) {
            ESP_LOGE(TAG, "Помилка ініціалізації датчика температури камери: %s", esp_err_to_name(chamber_result));
            result = chamber_result;
        } else {
            ESP_LOGI(TAG, "Датчик температури камери ініціалізовано на піні %d", chamber_temp_pin);
        }
    } else {
        ESP_LOGW(TAG, "Не знайдено пін для датчика температури камери");
    }
    
    return result;
}

// Перевірка мінімального часу простою компресора
bool CoolingControlModule::is_min_compressor_off_time_elapsed() const
{
    // Отримуємо поточний час
    uint32_t current_time = static_cast<uint32_t>(time(nullptr));
    
    // Якщо компресор ще не зупинявся, вважаємо що час простою дотримано
    if (last_compressor_stop_time_ == 0) {
        return true;
    }
    
    // Перевіряємо, чи минув мінімальний час простою
    return (current_time - last_compressor_stop_time_) >= min_compressor_off_time_sec_;
}

// Оновлення статистики роботи компресора
void CoolingControlModule::update_compressor_statistics()
{
    // Якщо компресор працює, оновлюємо час роботи
    if (compressor_running_ && compressor_start_time_ > 0) {
        uint32_t current_time = static_cast<uint32_t>(time(nullptr));
        uint32_t current_runtime = compressor_on_time_ + (current_time - compressor_start_time_);
        
        // Оновлюємо статистику в SharedState кожні 60 секунд
        if (current_time % 60 == 0) {
            SharedState::set<uint32_t>(cooling_state::KEY_STATS_COMPRESSOR_RUNTIME, current_runtime);
            SharedState::set<uint32_t>(cooling_state::KEY_STATS_COMPRESSOR_CYCLES, compressor_cycles_);
        }
    }
}
