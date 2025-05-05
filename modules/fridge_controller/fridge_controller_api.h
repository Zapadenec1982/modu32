/**
 * @file fridge_controller_api.h
 * @brief Публічний API модуля контролера холодильника
 * 
 * Цей файл містить оголошення публічних функцій, структур даних
 * та констант, які можуть використовуватися іншими модулями.
 */

#ifndef MODULES_FRIDGE_CONTROLLER_API_H
#define MODULES_FRIDGE_CONTROLLER_API_H

#include "esp_err.h"
#include "fridge_controller_state.h"
#include <string>

namespace fridge_api {

/**
 * @brief Режими роботи холодильника
 */
enum class OperationMode {
    AUTO,           ///< Автоматичний режим (терморегуляція)
    MANUAL,         ///< Ручний режим (керування в обхід термостата)
    DEFROST,        ///< Режим розморожування
    OFF             ///< Вимкнено
};

/**
 * @brief Коди помилок холодильника
 */
enum class FridgeErrorCode {
    NONE = 0,                      ///< Немає помилок
    TEMPERATURE_SENSOR_FAILURE,    ///< Відмова датчика температури
    COMPRESSOR_FAILURE,            ///< Відмова компресора
    FAN_FAILURE,                   ///< Відмова вентилятора
    DEFROST_FAILURE,               ///< Відмова розморожування
    DOOR_OPEN_TOO_LONG,            ///< Двері відкриті занадто довго
    HIGH_TEMPERATURE,              ///< Висока температура
    LOW_TEMPERATURE,               ///< Низька температура
    SYSTEM_ERROR                   ///< Системна помилка
};

// === Публічні API функції ===

/**
 * @brief Отримує поточний стан холодильника
 * 
 * @param status Вказівник на структуру, яка буде заповнена поточним станом
 * @return ESP_OK при успішному виконанні, інакше код помилки
 */
esp_err_t get_status(fridge_state::FridgeStatus* status);

/**
 * @brief Встановлює цільову температуру
 * 
 * @param temperature Нова цільова температура в градусах Цельсія
 * @return ESP_OK при успішному встановленні, інакше код помилки
 */
esp_err_t set_target_temperature(float temperature);

/**
 * @brief Встановлює гістерезис
 * 
 * @param hysteresis Новий гістерезис в градусах Цельсія
 * @return ESP_OK при успішному встановленні, інакше код помилки
 */
esp_err_t set_hysteresis(float hysteresis);

/**
 * @brief Встановлює режим роботи
 * 
 * @param mode Новий режим роботи
 * @return ESP_OK при успішному встановленні, інакше код помилки
 */
esp_err_t set_operation_mode(OperationMode mode);

/**
 * @brief Керує освітленням
 * 
 * @param state Новий стан освітлення (true = увімкнено)
 * @return ESP_OK при успішному встановленні, інакше код помилки
 */
esp_err_t set_light(bool state);

/**
 * @brief Запускає процес розморожування
 * 
 * @param duration_minutes Тривалість розморожування в хвилинах (0 = використовувати значення за замовчуванням)
 * @return ESP_OK при успішному запуску, інакше код помилки
 */
esp_err_t start_defrost(uint32_t duration_minutes = 0);

/**
 * @brief Зупиняє процес розморожування
 * 
 * @return ESP_OK при успішній зупинці, інакше код помилки
 */
esp_err_t stop_defrost();

/**
 * @brief Отримує історію температури
 * 
 * @param data Вказівник на масив, який буде заповнено даними
 * @param count Максимальна кількість елементів в масиві
 * @param actual_count Вказівник на змінну, яка буде містити фактичну кількість елементів
 * @return ESP_OK при успішному отриманні, інакше код помилки
 */
esp_err_t get_temperature_history(float* data, size_t count, size_t* actual_count);

/**
 * @brief Скидає статистику холодильника
 * 
 * @return ESP_OK при успішному скиданні, інакше код помилки
 */
esp_err_t reset_statistics();

} // namespace fridge_api

#endif // MODULES_FRIDGE_CONTROLLER_API_H