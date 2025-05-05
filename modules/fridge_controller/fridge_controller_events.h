/**
 * @file fridge_controller_events.h
 * @brief Визначення подій модуля контролера холодильника
 * 
 * Цей файл містить усі визначення подій, які модуль публікує
 * або на які він підписується через EventBus.
 */

#ifndef MODULES_FRIDGE_CONTROLLER_EVENTS_H
#define MODULES_FRIDGE_CONTROLLER_EVENTS_H

#include <string>

namespace fridge_events {

// === Константи імен подій ===

// Події, які публікує модуль
static const char* const EVENT_TEMPERATURE_CHANGED = "fridge.temperature_changed";
static const char* const EVENT_COMPRESSOR_STATE_CHANGED = "fridge.compressor_changed";
static const char* const EVENT_FAN_STATE_CHANGED = "fridge.fan_changed";
static const char* const EVENT_DEFROST_STARTED = "fridge.defrost_started";
static const char* const EVENT_DEFROST_COMPLETED = "fridge.defrost_completed";
static const char* const EVENT_DOOR_STATE_CHANGED = "fridge.door_changed";
static const char* const EVENT_MODE_CHANGED = "fridge.mode_changed";
static const char* const EVENT_ERROR_DETECTED = "fridge.error";

// Події, на які підписується модуль
static const char* const EVENT_SYSTEM_STARTED = "SystemStarted";
static const char* const EVENT_SYSTEM_SHUTDOWN = "SystemShutdown";
static const char* const EVENT_WIFI_CONNECTED = "wifi.connected";
static const char* const EVENT_WIFI_DISCONNECTED = "wifi.disconnected";

// === Структури даних подій ===

/**
 * @brief Подія зміни температури
 */
struct TemperatureChangedEvent {
    std::string sensor_id;    ///< Ідентифікатор датчика
    float temperature;        ///< Нове значення температури (°C)
    uint64_t timestamp;       ///< Часова мітка (мс)
};

/**
 * @brief Подія зміни стану компресора
 */
struct CompressorStateChangedEvent {
    bool is_running;          ///< Новий стан компресора (true = працює)
    uint64_t timestamp;       ///< Часова мітка (мс)
    uint32_t runtime_sec;     ///< Час роботи з моменту запуску (секунди)
};

/**
 * @brief Подія зміни стану вентилятора
 */
struct FanStateChangedEvent {
    bool is_running;          ///< Новий стан вентилятора (true = працює)
    uint64_t timestamp;       ///< Часова мітка (мс)
};

/**
 * @brief Подія початку розморожування
 */
struct DefrostStartedEvent {
    uint32_t planned_duration_sec; ///< Планована тривалість (секунди)
    uint64_t timestamp;           ///< Часова мітка (мс)
    bool is_manual;               ///< Чи запущено вручну (true) чи за розкладом (false)
};

/**
 * @brief Подія завершення розморожування
 */
struct DefrostCompletedEvent {
    uint32_t actual_duration_sec;  ///< Фактична тривалість (секунди)
    uint64_t timestamp;           ///< Часова мітка (мс)
    bool is_completed;            ///< Чи завершено успішно (true) чи перервано (false)
    float final_temperature;      ///< Кінцева температура випарника (°C)
};

/**
 * @brief Подія зміни стану дверей
 */
struct DoorStateChangedEvent {
    bool is_open;             ///< Новий стан дверей (true = відчинено)
    uint64_t timestamp;       ///< Часова мітка (мс)
    uint32_t open_duration_sec; ///< Тривалість відкритого стану (секунди, 0 якщо закрито)
};

/**
 * @brief Подія зміни режиму роботи
 */
struct ModeChangedEvent {
    int new_mode;             ///< Новий режим роботи (з enum OperationMode)
    int previous_mode;        ///< Попередній режим
    uint64_t timestamp;       ///< Часова мітка (мс)
    bool is_manual;           ///< Чи змінено вручну (true) чи автоматично (false)
};

/**
 * @brief Подія виявлення помилки
 */
struct ErrorEvent {
    int error_code;           ///< Код помилки
    std::string description;  ///< Опис помилки
    uint64_t timestamp;       ///< Часова мітка (мс)
    bool is_critical;         ///< Чи є критичною (потребує негайної реакції)
};

} // namespace fridge_events

#endif // MODULES_FRIDGE_CONTROLLER_EVENTS_H