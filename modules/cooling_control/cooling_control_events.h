/**
 * @file cooling_control_events.h
 * @brief Визначення подій модуля керування охолодженням
 */

#ifndef MODULES_COOLING_CONTROL_EVENTS_H
#define MODULES_COOLING_CONTROL_EVENTS_H

#include <string>

namespace cooling_events {

// Константи імен подій
static const char* const EVENT_TEMPERATURE_CHANGED = "cooling.temperature_changed";
static const char* const EVENT_COMPRESSOR_STATE_CHANGED = "cooling.compressor_changed";
static const char* const EVENT_FAN_STATE_CHANGED = "cooling.fan_changed";
static const char* const EVENT_TARGET_TEMPERATURE_CHANGED = "cooling.target_temp_changed";
static const char* const EVENT_MODE_CHANGED = "cooling.mode_changed";

/**
 * @brief Подія зміни температури камери
 */
struct TemperatureChangedEvent {
    float temperature;   ///< Нове значення температури (°C)
    uint64_t timestamp;  ///< Часова мітка (мс)
};

/**
 * @brief Подія зміни стану компресора
 */
struct CompressorStateChangedEvent {
    bool is_running;        ///< Новий стан компресора (true = працює)
    uint64_t timestamp;     ///< Часова мітка (мс)
    uint32_t runtime_sec;   ///< Час роботи з моменту запуску (секунди)
};

/**
 * @brief Подія зміни стану вентилятора
 */
struct FanStateChangedEvent {
    bool is_running;        ///< Новий стан вентилятора (true = працює)
    uint64_t timestamp;     ///< Часова мітка (мс)
};

/**
 * @brief Подія зміни цільової температури
 */
struct TargetTemperatureChangedEvent {
    float old_temperature;  ///< Попереднє значення (°C)
    float new_temperature;  ///< Нове значення (°C)
    uint64_t timestamp;     ///< Часова мітка (мс)
    bool is_manual;         ///< Чи змінено вручну (true) чи автоматично (false)
};

/**
 * @brief Подія зміни режиму роботи
 */
struct ModeChangedEvent {
    int old_mode;           ///< Попередній режим (з enum OperationMode)
    int new_mode;           ///< Новий режим (з enum OperationMode)
    uint64_t timestamp;     ///< Часова мітка (мс)
    bool is_manual;         ///< Чи змінено вручну (true) чи автоматично (false)
};

/**
 * @brief Подія досягнення цільової температури
 */
struct TargetTemperatureReachedEvent {
    float temperature;      ///< Досягнута температура (°C)
    uint64_t timestamp;     ///< Часова мітка (мс)
    uint32_t time_to_reach; ///< Час досягнення від моменту запуску (секунди)
};

} // namespace cooling_events

#endif // MODULES_COOLING_CONTROL_EVENTS_H