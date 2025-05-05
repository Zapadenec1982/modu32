/**
 * @file fridge_controller_state.h
 * @brief Структури даних для стану модуля контролера холодильника
 * 
 * Цей файл містить усі структури даних, які модуль використовує
 * для обміну станом через SharedState.
 */

#ifndef MODULES_FRIDGE_CONTROLLER_STATE_H
#define MODULES_FRIDGE_CONTROLLER_STATE_H

#include <string>

namespace fridge_state {

// === Ключі для SharedState ===
// Формат: fridge/{група}/{ключ}

// Температура
static const char* const KEY_TEMP_CHAMBER = "fridge/temperature/chamber";  // Поточна температура камери
static const char* const KEY_TEMP_EVAPORATOR = "fridge/temperature/evaporator";  // Поточна температура випарника
static const char* const KEY_TEMP_TARGET = "fridge/temperature/target";  // Цільова температура
static const char* const KEY_TEMP_HYSTERESIS = "fridge/temperature/hysteresis";  // Гістерезис

// Стан актуаторів
static const char* const KEY_COMPRESSOR_STATE = "fridge/actuator/compressor";  // Стан компресора
static const char* const KEY_FAN_STATE = "fridge/actuator/fan";  // Стан вентилятора
static const char* const KEY_DEFROST_STATE = "fridge/actuator/defrost";  // Стан нагрівача розморожування
static const char* const KEY_LIGHT_STATE = "fridge/actuator/light";  // Стан освітлення

// Режим роботи
static const char* const KEY_OPERATION_MODE = "fridge/mode";  // Поточний режим роботи

// Стан розморожування
static const char* const KEY_DEFROST_ACTIVE = "fridge/defrost/active";  // Чи активне розморожування
static const char* const KEY_DEFROST_PROGRESS = "fridge/defrost/progress";  // Прогрес розморожування (%)
static const char* const KEY_LAST_DEFROST_TIME = "fridge/defrost/last_time";  // Час останнього розморожування
static const char* const KEY_NEXT_DEFROST_TIME = "fridge/defrost/next_time";  // Час наступного розморожування

// Стан дверей
static const char* const KEY_DOOR_STATE = "fridge/door/state";  // Стан дверей (відкриті/закриті)
static const char* const KEY_DOOR_OPEN_TIME = "fridge/door/open_time";  // Час відкриття дверей

// Статистика
static const char* const KEY_STATS_COMPRESSOR_CYCLES = "fridge/stats/compressor_cycles";  // Кількість циклів компресора
static const char* const KEY_STATS_COMPRESSOR_RUNTIME = "fridge/stats/compressor_runtime";  // Загальний час роботи компресора
static const char* const KEY_STATS_DEFROST_COUNT = "fridge/stats/defrost_count";  // Кількість розморожувань
static const char* const KEY_STATS_AVG_TEMPERATURE = "fridge/stats/avg_temperature";  // Середня температура

// Помилки
static const char* const KEY_ERROR_CODE = "fridge/error/code";  // Код останньої помилки
static const char* const KEY_ERROR_DESCRIPTION = "fridge/error/description";  // Опис останньої помилки

// === Структури даних для SharedState ===

/**
 * @brief Повний стан холодильника (для API)
 */
struct FridgeStatus {
    // Температура
    float chamber_temp;         ///< Температура камери (°C)
    float evaporator_temp;      ///< Температура випарника (°C)
    float target_temp;          ///< Цільова температура (°C)
    float hysteresis;           ///< Гістерезис (°C)
    
    // Стан актуаторів
    bool compressor_running;    ///< Компресор працює
    bool fan_running;           ///< Вентилятор працює
    bool defrost_active;        ///< Розморожування активне
    bool light_on;              ///< Освітлення увімкнено
    
    // Режим роботи
    int operation_mode;         ///< Режим роботи (з enum OperationMode)
    
    // Розморожування
    uint32_t last_defrost_time; ///< Час останнього розморожування (UNIX timestamp)
    uint32_t next_defrost_time; ///< Час наступного розморожування (UNIX timestamp)
    uint8_t defrost_progress;   ///< Прогрес розморожування (%)
    
    // Двері
    bool door_open;             ///< Двері відчинені
    uint32_t door_open_time;    ///< Час відкриття дверей (секунди)
    
    // Статистика
    uint32_t compressor_cycles; ///< Кількість циклів компресора
    uint32_t compressor_runtime; ///< Загальний час роботи компресора (секунди)
    uint32_t defrost_count;     ///< Кількість розморожувань
    float avg_temperature;      ///< Середня температура (°C)
    
    // Помилки
    int error_code;             ///< Код останньої помилки (0 = немає помилок)
    std::string error_description; ///< Опис останньої помилки
};

} // namespace fridge_state

#endif // MODULES_FRIDGE_CONTROLLER_STATE_H