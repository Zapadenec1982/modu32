/**
 * @file cooling_control_api.h
 * @brief Публічний API модуля керування охолодженням
 */

#ifndef MODULES_COOLING_CONTROL_API_H
#define MODULES_COOLING_CONTROL_API_H

#include "esp_err.h"

namespace cooling_api {

/**
 * @brief Режими роботи охолодження
 */
enum class OperationMode {
    AUTO,           ///< Автоматичний режим (терморегуляція)
    MANUAL,         ///< Ручний режим (керування в обхід термостата)
    OFF             ///< Вимкнено
};

/**
 * @brief Встановлює цільову температуру
 * 
 * @param temperature Температура в градусах Цельсія
 * @return ESP_OK при успішному встановленні, інакше код помилки
 */
esp_err_t set_target_temperature(float temperature);

/**
 * @brief Отримує поточну цільову температуру
 * 
 * @param temperature Вказівник на змінну для запису температури
 * @return ESP_OK при успішному отриманні, інакше код помилки
 */
esp_err_t get_target_temperature(float* temperature);

/**
 * @brief Встановлює гістерезис
 * 
 * @param hysteresis Гістерезис в градусах Цельсія
 * @return ESP_OK при успішному встановленні, інакше код помилки
 */
esp_err_t set_hysteresis(float hysteresis);

/**
 * @brief Отримує поточний гістерезис
 * 
 * @param hysteresis Вказівник на змінну для запису гістерезису
 * @return ESP_OK при успішному отриманні, інакше код помилки
 */
esp_err_t get_hysteresis(float* hysteresis);

/**
 * @brief Встановлює режим роботи
 * 
 * @param mode Новий режим роботи
 * @return ESP_OK при успішному встановленні, інакше код помилки
 */
esp_err_t set_mode(OperationMode mode);

/**
 * @brief Отримує поточний режим роботи
 * 
 * @param mode Вказівник на змінну для запису режиму
 * @return ESP_OK при успішному отриманні, інакше код помилки
 */
esp_err_t get_mode(OperationMode* mode);

/**
 * @brief Встановлює стан компресора (тільки в ручному режимі)
 * 
 * @param state Стан компресора (true - увімкнено)
 * @return ESP_OK при успішному встановленні, інакше код помилки
 */
esp_err_t set_compressor_state(bool state);

/**
 * @brief Отримує поточний стан компресора
 * 
 * @param state Вказівник на змінну для запису стану
 * @return ESP_OK при успішному отриманні, інакше код помилки
 */
esp_err_t get_compressor_state(bool* state);

/**
 * @brief Встановлює стан вентилятора (тільки в ручному режимі)
 * 
 * @param state Стан вентилятора (true - увімкнено)
 * @return ESP_OK при успішному встановленні, інакше код помилки
 */
esp_err_t set_fan_state(bool state);

/**
 * @brief Отримує поточний стан вентилятора
 * 
 * @param state Вказівник на змінну для запису стану
 * @return ESP_OK при успішному отриманні, інакше код помилки
 */
esp_err_t get_fan_state(bool* state);

/**
 * @brief Отримує поточну температуру камери
 * 
 * @param temperature Вказівник на змінну для запису температури
 * @return ESP_OK при успішному отриманні, інакше код помилки
 */
esp_err_t get_chamber_temperature(float* temperature);

/**
 * @brief Отримує статистику роботи компресора
 * 
 * @param total_runtime Вказівник на змінну для запису загального часу роботи (сек)
 * @param cycles Вказівник на змінну для запису кількості циклів
 * @return ESP_OK при успішному отриманні, інакше код помилки
 */
esp_err_t get_compressor_statistics(uint32_t* total_runtime, uint32_t* cycles);

} // namespace cooling_api

#endif // MODULES_COOLING_CONTROL_API_H