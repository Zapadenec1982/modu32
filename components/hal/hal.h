/**
 * @file hal.h
 * @brief Інтерфейс апаратного рівня абстракції для ModuChill
 * 
 * Цей файл визначає інтерфейс HAL (Hardware Abstraction Layer),
 * який абстрагує специфіку апаратного забезпечення від
 * прикладного рівня програмного забезпечення.
 */

#ifndef HAL_H
#define HAL_H

#include "esp_err.h"
#include "driver/gpio.h"
#include "board_config.h"
#include <string>
#include <map>

/**
 * @brief Типи апаратних компонентів
 */
typedef enum {
    HAL_COMPONENT_RELAY,       ///< Реле
    HAL_COMPONENT_BUTTON,      ///< Кнопка
    HAL_COMPONENT_TEMP_SENSOR, ///< Датчик температури
    HAL_COMPONENT_DISPLAY,     ///< Дисплей
    HAL_COMPONENT_LED,         ///< Світлодіод
    HAL_COMPONENT_OTHER        ///< Інші компоненти
} hal_component_type_t;

/**
 * @brief Клас для керування апаратними компонентами
 */
class HAL {
public:
    /**
     * @brief Ініціалізує HAL
     * 
     * Налаштовує всі апаратні компоненти згідно з конфігурацією
     * 
     * @return ESP_OK при успішній ініціалізації, інакше код помилки
     */
    static esp_err_t init();

    /**
     * @brief Отримує фізичний пін для логічного імені компонента
     * 
     * @param logical_name Логічне ім'я компонента (напр. "компресор", "датчик_темп_камери")
     * @param component_type Тип компонента (реле, датчик, тощо)
     * @return gpio_num_t Номер пін GPIO або -1, якщо не знайдено
     */
    static gpio_num_t get_pin_for_component(const std::string& logical_name, hal_component_type_t component_type);

    /**
     * @brief Зіставляє логічне ім'я компонента з фізичним піном
     * 
     * @param logical_name Логічне ім'я компонента (напр. "компресор", "датчик_темп_камери")
     * @param component_type Тип компонента (реле, датчик, тощо)
     * @param pin_index Індекс піна в конфігурації (напр. 1 для relay1_pin)
     * @return ESP_OK при успішному зіставленні, інакше код помилки
     */
    static esp_err_t map_component_to_pin(const std::string& logical_name, hal_component_type_t component_type, int pin_index);

private:
    // Карта зіставлень логічних імен з фізичними пінами
    static std::map<std::string, gpio_num_t> component_to_pin_map;
    
    // Карта зіставлень типів компонентів з їх логічними іменами
    static std::map<hal_component_type_t, std::vector<std::string>> component_type_map;
    
    // Флаг ініціалізації
    static bool initialized;
};

// Інтерфейси для конкретних компонентів

/**
 * @brief Абстрактний клас датчика
 */
class SensorInterface {
public:
    virtual ~SensorInterface() = default;
    
    /**
     * @brief Ініціалізує датчик
     * 
     * @return ESP_OK при успішній ініціалізації, інакше код помилки
     */
    virtual esp_err_t init() = 0;
    
    /**
     * @brief Зчитує дані з датчика
     * 
     * @param value Вказівник на змінну для збереження зчитаного значення
     * @return ESP_OK при успішному зчитуванні, інакше код помилки
     */
    virtual esp_err_t read(float* value) = 0;
    
    /**
     * @brief Отримує тип датчика
     * 
     * @return Рядок з типом датчика
     */
    virtual std::string get_type() const = 0;
};

/**
 * @brief Абстрактний клас актуатора
 */
class ActuatorInterface {
public:
    virtual ~ActuatorInterface() = default;
    
    /**
     * @brief Ініціалізує актуатор
     * 
     * @return ESP_OK при успішній ініціалізації, інакше код помилки
     */
    virtual esp_err_t init() = 0;
    
    /**
     * @brief Встановлює стан актуатора
     * 
     * @param state Новий стан (true = увімкнено, false = вимкнено)
     * @return ESP_OK при успішному встановленні стану, інакше код помилки
     */
    virtual esp_err_t set_state(bool state) = 0;
    
    /**
     * @brief Отримує поточний стан актуатора
     * 
     * @return Поточний стан (true = увімкнено, false = вимкнено)
     */
    virtual bool get_state() const = 0;
    
    /**
     * @brief Перемикає стан актуатора
     * 
     * @return ESP_OK при успішному перемиканні, інакше код помилки
     */
    virtual esp_err_t toggle() = 0;
    
    /**
     * @brief Отримує тип актуатора
     * 
     * @return Рядок з типом актуатора
     */
    virtual std::string get_type() const = 0;
};

#endif // HAL_H
