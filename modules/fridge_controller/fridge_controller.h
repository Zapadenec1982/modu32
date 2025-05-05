/**
 * @file fridge_controller.h
 * @brief Модуль контролера холодильника для ModuChill
 */

#ifndef MODULES_FRIDGE_CONTROLLER_H
#define MODULES_FRIDGE_CONTROLLER_H

#include "base_module.h"
#include "hal.h"
#include "ds18b20.h"
#include "relay.h"
#include <vector>
#include <memory>
#include <string>
#include <cJSON.h>

/**
 * @brief Модуль для керування холодильною камерою
 * 
 * Цей модуль реалізує термостатичну логіку та керування компресором,
 * вентилятором, розморожуванням та освітленням холодильника.
 */
class FridgeControllerModule : public BaseModule {
public:
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
     * @brief Конструктор
     */
    FridgeControllerModule();
    
    /**
     * @brief Деструктор
     */
    virtual ~FridgeControllerModule();
    
    /**
     * @brief Отримує ім'я модуля
     * 
     * @return Рядок "fridge_controller"
     */
    const char* getName() const override;
    
    /**
     * @brief Ініціалізує модуль
     * 
     * @return ESP_OK при успішній ініціалізації, інакше код помилки
     */
    esp_err_t init() override;
    
    /**
     * @brief Періодичне оновлення модуля
     * 
     * Ця функція викликається з головного циклу програми
     * для оновлення стану модуля та виконання логіки керування.
     */
    void tick() override;
    
    /**
     * @brief Зупиняє модуль
     * 
     * Ця функція викликається при зупинці модуля
     * для звільнення ресурсів та безпечного вимкнення.
     */
    void stop() override;
    
    /**
     * @brief Генерує схему UI для модуля
     * 
     * @param module_schema_parent Вказівник на батьківський cJSON об'єкт
     * @return ESP_OK при успішній генерації, інакше код помилки
     */
    esp_err_t get_ui_schema(cJSON* module_schema_parent) override;
    
    /**
     * @brief Встановлює цільову температуру
     * 
     * @param temp_c Температура в градусах Цельсія
     * @return ESP_OK при успішному встановленні, інакше код помилки
     */
    esp_err_t set_target_temperature(float temp_c);
    
    /**
     * @brief Отримує поточну цільову температуру
     * 
     * @return Температура в градусах Цельсія
     */
    float get_target_temperature() const;
    
    /**
     * @brief Встановлює гістерезис
     * 
     * @param hysteresis_c Гістерезис в градусах Цельсія
     * @return ESP_OK при успішному встановленні, інакше код помилки
     */
    esp_err_t set_hysteresis(float hysteresis_c);
    
    /**
     * @brief Отримує поточний гістерезис
     * 
     * @return Гістерезис в градусах Цельсія
     */
    float get_hysteresis() const;
    
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
     * @return Поточний режим роботи
     */
    OperationMode get_mode() const;
    
    /**
     * @brief Встановлює стан освітлення
     * 
     * @param state Новий стан (true = увімкнено)
     * @return ESP_OK при успішному встановленні, інакше код помилки
     */
    esp_err_t set_light(bool state);
    
    /**
     * @brief Отримує поточний стан освітлення
     * 
     * @return Поточний стан (true = увімкнено)
     */
    bool get_light() const;
    
    /**
     * @brief Запускає процес розморожування
     * 
     * @param duration_minutes Тривалість розморожування в хвилинах
     * @return ESP_OK при успішному запуску, інакше код помилки
     */
    esp_err_t start_defrost(uint32_t duration_minutes = 0);
    
    /**
     * @brief Зупиняє процес розморожування
     * 
     * @return ESP_OK при успішній зупинці, інакше код помилки
     */
    esp_err_t stop_defrost();
    
private:
    // Датчики температури
    std::unique_ptr<DS18B20Sensor> chamber_temp_sensor_;   ///< Датчик температури камери
    std::unique_ptr<DS18B20Sensor> evaporator_temp_sensor_; ///< Датчик температури випарника
    
    // Актуатори
    std::unique_ptr<Relay> compressor_relay_; ///< Реле компресора
    std::unique_ptr<Relay> fan_relay_;       ///< Реле вентилятора
    std::unique_ptr<Relay> defrost_relay_;   ///< Реле розморожування
    std::unique_ptr<Relay> light_relay_;     ///< Реле освітлення
    
    // Параметри керування
    float target_temp_c_;        ///< Цільова температура в °C
    float hysteresis_c_;         ///< Гістерезис в °C
    OperationMode mode_;         ///< Поточний режим роботи
    uint32_t min_compressor_off_time_sec_; ///< Мінімальний час вимкнення компресора в секундах
    
    // Змінні стану
    float current_chamber_temp_c_;    ///< Поточна температура камери
    float current_evaporator_temp_c_; ///< Поточна температура випарника
    bool compressor_running_;         ///< Флаг роботи компресора
    bool fan_running_;                ///< Флаг роботи вентилятора
    bool defrost_active_;             ///< Флаг активного розморожування
    bool light_on_;                   ///< Флаг увімкненого освітлення
    uint32_t last_compressor_stop_time_; ///< Час останньої зупинки компресора
    uint32_t last_temp_read_time_;    ///< Час останнього зчитування температури
    uint32_t last_defrost_time_;      ///< Час останнього розморожування
    uint32_t defrost_duration_sec_;   ///< Тривалість розморожування в секундах
    uint32_t defrost_start_time_;     ///< Час початку розморожування
    
    /**
     * @brief Зчитує значення температури з датчиків
     * 
     * @return ESP_OK при успішному зчитуванні, інакше код помилки
     */
    esp_err_t read_temperatures();
    
    /**
     * @brief Виконує термостатичну логіку
     * 
     * @return ESP_OK при успішному керуванні, інакше код помилки
     */
    esp_err_t run_thermostat_logic();
    
    /**
     * @brief Ініціалізує актуатори
     * 
     * @return ESP_OK при успішній ініціалізації, інакше код помилки
     */
    esp_err_t init_actuators();
    
    /**
     * @brief Ініціалізує датчики
     * 
     * @return ESP_OK при успішній ініціалізації, інакше код помилки
     */
    esp_err_t init_sensors();
    
    /**
     * @brief Перевіряє, чи дотримано мінімальний час простою компресора
     * 
     * @return true якщо дотримано, інакше false
     */
    bool is_min_compressor_off_time_elapsed() const;
    
    /**
     * @brief Обробляє логіку розморожування
     * 
     * @return ESP_OK при успішній обробці, інакше код помилки
     */
    esp_err_t process_defrost();
};

#endif // MODULES_FRIDGE_CONTROLLER_H
