/**
 * @file cooling_control_state.h
 * @brief Структури даних для стану модуля керування охолодженням
 */

#ifndef MODULES_COOLING_CONTROL_STATE_H
#define MODULES_COOLING_CONTROL_STATE_H

#include <string>

namespace cooling_state {

// Ключі для SharedState
static const char* const KEY_TEMP_CHAMBER = "cooling/temperature/chamber";
static const char* const KEY_TEMP_TARGET = "cooling/temperature/target";
static const char* const KEY_TEMP_HYSTERESIS = "cooling/temperature/hysteresis";
static const char* const KEY_COMPRESSOR_STATE = "cooling/actuator/compressor";
static const char* const KEY_FAN_STATE = "cooling/actuator/fan";
static const char* const KEY_OPERATION_MODE = "cooling/mode";
static const char* const KEY_STATS_COMPRESSOR_CYCLES = "cooling/stats/compressor_cycles";
static const char* const KEY_STATS_COMPRESSOR_RUNTIME = "cooling/stats/compressor_runtime";
static const char* const KEY_STATS_AVG_TEMPERATURE = "cooling/stats/avg_temperature";

/**
 * @brief Повний стан холодильника для API
 */
struct CoolingStatus {
    // Температура
    float chamber_temp;      ///< Температура камери (°C)
    float target_temp;       ///< Цільова температура (°C)
    float hysteresis;        ///< Гістерезис (°C)
    
    // Стан актуаторів
    bool compressor_running; ///< Компресор працює
    bool fan_running;        ///< Вентилятор працює
    
    // Режим роботи
    int operation_mode;      ///< Режим роботи (з enum OperationMode)
    
    // Статистика
    uint32_t compressor_cycles;     ///< Кількість циклів компресора
    uint32_t compressor_runtime;    ///< Загальний час роботи компресора (секунди)
    float avg_temperature;          ///< Середня температура (°C)
};

} // namespace cooling_state

#endif // MODULES_COOLING_CONTROL_STATE_H