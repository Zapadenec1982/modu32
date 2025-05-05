/**
 * @file relay.h
 * @brief Інтерфейс для роботи з реле
 */

#ifndef HAL_RELAY_H
#define HAL_RELAY_H

#include "hal.h"

/**
 * @brief Клас для роботи з реле
 * 
 * Цей клас реалізує інтерфейс ActuatorInterface для керування реле,
 * підключеним до GPIO піна.
 */
class Relay : public ActuatorInterface {
public:
    /**
     * @brief Конструктор
     * 
     * @param pin Пін GPIO, до якого підключено реле
     * @param name Логічне ім'я реле (для ідентифікації)
     * @param active_low Якщо true, то реле активується низьким рівнем (LOW)
     */
    Relay(gpio_num_t pin, const std::string& name, bool active_low = true);
    
    /**
     * @brief Деструктор
     */
    virtual ~Relay();
    
    /**
     * @brief Ініціалізує реле
     * 
     * @return ESP_OK при успішній ініціалізації, інакше код помилки
     */
    esp_err_t init() override;
    
    /**
     * @brief Встановлює стан реле
     * 
     * @param state Новий стан (true = увімкнено, false = вимкнено)
     * @return ESP_OK при успішному встановленні стану, інакше код помилки
     */
    esp_err_t set_state(bool state) override;
    
    /**
     * @brief Отримує поточний стан реле
     * 
     * @return Поточний стан (true = увімкнено, false = вимкнено)
     */
    bool get_state() const override;
    
    /**
     * @brief Перемикає стан реле
     * 
     * @return ESP_OK при успішному перемиканні, інакше код помилки
     */
    esp_err_t toggle() override;
    
    /**
     * @brief Отримує тип актуатора
     * 
     * @return Рядок "Relay"
     */
    std::string get_type() const override;
    
    /**
     * @brief Отримує ім'я реле
     * 
     * @return Логічне ім'я реле
     */
    std::string get_name() const;
    
    /**
     * @brief Встановлює затримку перед зміною стану
     * 
     * Ця функція дозволяє встановити затримку перед зміною стану реле,
     * що може бути корисно для захисту компресора від частих перемикань.
     * 
     * @param delay_ms Затримка в мілісекундах
     */
    void set_delay(uint32_t delay_ms);
    
private:
    gpio_num_t pin_;         ///< Пін GPIO, до якого підключено реле
    std::string name_;       ///< Логічне ім'я реле
    bool active_low_;        ///< Якщо true, то реле активується низьким рівнем (LOW)
    bool state_;             ///< Поточний логічний стан (true = увімкнено)
    uint32_t delay_ms_;      ///< Затримка перед зміною стану
    bool initialized_;       ///< Флаг ініціалізації
    
    /**
     * @brief Застосовує фізичний стан до реле
     * 
     * Ця функція перетворює логічний стан (увімк/вимк) у фізичний
     * (high/low) з урахуванням параметра active_low_.
     * 
     * @param logical_state Логічний стан (true = увімкнено)
     * @return ESP_OK при успішному встановленні, інакше код помилки
     */
    esp_err_t apply_state(bool logical_state);
};

#endif // HAL_RELAY_H
