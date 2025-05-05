/**
 * @file board_config.h
 * @brief Конфігурація пінів для тестової плати ModuChill
 * 
 * Цей файл містить визначення пінів для конкретної апаратної конфігурації.
 * Різні версії плат будуть мати різні файли board_config.h.
 */

#ifndef HAL_BOARD_CONFIG_H
#define HAL_BOARD_CONFIG_H

#include "driver/gpio.h"

/**
 * @brief Конфігурація пінів для ESP32
 * 
 * Ця структура визначає фізичні піни GPIO для різних функцій.
 * Модифікуйте цей файл для різних апаратних конфігурацій.
 */
typedef struct {
    /** @name Реле
     * Піни для керування реле
     * @{
     */
    gpio_num_t relay1_pin;      ///< Реле 1
    gpio_num_t relay2_pin;      ///< Реле 2
    gpio_num_t relay3_pin;      ///< Реле 3
    gpio_num_t relay4_pin;      ///< Реле 4
    /** @} */

    /** @name Кнопки
     * Піни для підключення кнопок керування
     * @{
     */
    gpio_num_t button1_pin;     ///< Кнопка 1
    gpio_num_t button2_pin;     ///< Кнопка 2
    gpio_num_t button3_pin;     ///< Кнопка 3
    gpio_num_t button4_pin;     ///< Кнопка 4
    gpio_num_t button5_pin;     ///< Кнопка 5
    /** @} */

    /** @name Дисплей OLED
     * Піни для підключення OLED дисплею по I2C
     * @{
     */
    gpio_num_t oled_scl_pin;    ///< SCL для OLED дисплею (I2C)
    gpio_num_t oled_sda_pin;    ///< SDA для OLED дисплею (I2C)
    /** @} */

    /** @name Датчики температури
     * Піни для підключення датчиків температури
     * @{
     */
    gpio_num_t ds18b20_pin1;    ///< DS18B20 датчик 1 (OneWire)
    gpio_num_t ds18b20_pin2;    ///< DS18B20 датчик 2 (OneWire)
    /** @} */
} board_pins_config_t;

/**
 * @brief Конфігурація пінів для тестової плати
 * 
 * Конкретні значення для тестової плати
 */
static const board_pins_config_t BOARD_PINS_CONFIG = {
    // Реле
    .relay1_pin = GPIO_NUM_1,
    .relay2_pin = GPIO_NUM_2,
    .relay3_pin = GPIO_NUM_3,
    .relay4_pin = GPIO_NUM_4,
    
    // Кнопки
    .button1_pin = GPIO_NUM_9,
    .button2_pin = GPIO_NUM_10,
    .button3_pin = GPIO_NUM_12,
    .button4_pin = GPIO_NUM_13,
    .button5_pin = GPIO_NUM_11,
    
    // OLED дисплей
    .oled_scl_pin = GPIO_NUM_15,
    .oled_sda_pin = GPIO_NUM_16,
    
    // Датчики температури
    .ds18b20_pin1 = GPIO_NUM_8,
    .ds18b20_pin2 = GPIO_NUM_7
};

#endif // HAL_BOARD_CONFIG_H
