/**
 * @file ds18b20.h
 * @brief Інтерфейс для роботи з датчиками температури DS18B20
 */

#ifndef HAL_DS18B20_H
#define HAL_DS18B20_H

#include "hal.h"

/**
 * @brief Клас для роботи з датчиком температури DS18B20
 * 
 * Цей клас реалізує інтерфейс SensorInterface для датчика DS18B20,
 * використовуючи протокол 1-Wire для комунікації з датчиком.
 */
class DS18B20Sensor : public SensorInterface {
public:
    /**
     * @brief Конструктор
     * 
     * @param pin Пін GPIO, до якого підключено датчик
     * @param name Логічне ім'я датчика (для ідентифікації)
     */
    DS18B20Sensor(gpio_num_t pin, const std::string& name);
    
    /**
     * @brief Деструктор
     */
    virtual ~DS18B20Sensor();
    
    /**
     * @brief Ініціалізує датчик
     * 
     * @return ESP_OK при успішній ініціалізації, інакше код помилки
     */
    esp_err_t init() override;
    
    /**
     * @brief Зчитує температуру з датчика
     * 
     * @param value Вказівник на змінну для збереження температури
     * @return ESP_OK при успішному зчитуванні, інакше код помилки
     */
    esp_err_t read(float* value) override;
    
    /**
     * @brief Отримує тип датчика
     * 
     * @return Рядок "DS18B20"
     */
    std::string get_type() const override;
    
    /**
     * @brief Отримує ім'я датчика
     * 
     * @return Логічне ім'я датчика
     */
    std::string get_name() const;
    
    /**
     * @brief Встановлює роздільну здатність датчика
     * 
     * @param resolution Роздільна здатність (9-12 біт)
     * @return ESP_OK при успішному встановленні, інакше код помилки
     */
    esp_err_t set_resolution(uint8_t resolution);
    
private:
    gpio_num_t pin_;         ///< Пін GPIO, до якого підключено датчик
    std::string name_;       ///< Логічне ім'я датчика
    uint64_t rom_code_;      ///< ROM-код датчика (унікальний ідентифікатор)
    uint8_t resolution_;     ///< Роздільна здатність (9-12 біт)
    bool initialized_;       ///< Флаг ініціалізації
    
    // Команди для роботи з DS18B20
    static const uint8_t CMD_CONVERT_T = 0x44;         ///< Команда запуску перетворення температури
    static const uint8_t CMD_READ_SCRATCHPAD = 0xBE;   ///< Команда зчитування внутрішньої пам'яті
    static const uint8_t CMD_WRITE_SCRATCHPAD = 0x4E;  ///< Команда запису у внутрішню пам'ять
    static const uint8_t CMD_COPY_SCRATCHPAD = 0x48;   ///< Команда копіювання налаштувань у EEPROM
    static const uint8_t CMD_RECALL_EEPROM = 0xB8;     ///< Команда зчитування налаштувань з EEPROM
    static const uint8_t CMD_READ_POWER_SUPPLY = 0xB4; ///< Команда перевірки типу живлення
    
    // Функції для роботи з протоколом 1-Wire
    bool reset();                  ///< Скидання шини 1-Wire
    void write_bit(uint8_t bit);   ///< Запис одного біта
    uint8_t read_bit();            ///< Зчитування одного біта
    void write_byte(uint8_t byte); ///< Запис одного байта
    uint8_t read_byte();           ///< Зчитування одного байта
    
    /**
     * @brief Знаходить пристрої на шині 1-Wire
     * 
     * @return true якщо знайдено хоча б один пристрій, інакше false
     */
    bool search_devices();
    
    /**
     * @brief Перевіряє CRC байтів з внутрішньої пам'яті
     * 
     * @param data Масив байтів
     * @param len Довжина масиву
     * @return true якщо CRC коректний, інакше false
     */
    bool check_crc(const uint8_t* data, uint8_t len);
};

#endif // HAL_DS18B20_H
