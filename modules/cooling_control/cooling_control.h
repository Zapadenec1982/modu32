/**
 * @file cooling_control.h
 * @brief Модуль керування охолодженням для ModuChill
 */

#ifndef MODULES_COOLING_CONTROL_H
#define MODULES_COOLING_CONTROL_H

#include "base_module.h"
#include "hal.h"
#include "ds18b20.h"
#include "relay.h"
#include <memory>
#include <string>
#include <cJSON.h>

/**
 * @brief Модуль керування охолодженням
 * 
 * Цей модуль відповідає за керування компресором та 
 * вентилятором для підтримки заданої температури в камері.
 */
class CoolingControlModule : public BaseModule {
public:
    /**
     * @brief Режими роботи охолодження
     */
    enum class OperationMode {
        AUTO,           ///< Автоматичний режим (терморегуляція)
        MANUAL,         ///< Ручний режим (керування в обхід термостата)
        OFF             ///< Вимкнено
    };
    
    /**
     * @brief Конструктор
     */
    CoolingControlModule();
    
    /**
     * @brief Деструктор
     */
    virtual ~CoolingControlModule();
    
    /**
     * @brief Отримує ім'я модуля
     * 
     * @return Рядок "cooling_control"
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
     * @brief Встановлює стан компресора (ручний режим)
     * 
     * @param state Стан компресора (true - увімкнено)
     * @return ESP_OK при успішному встановленні, інакше код помилки
     */
    esp_err_t set_compressor_state(bool state);
    
    /**
     * @brief Отримує поточний стан компресора
     * 
     * @return true якщо компресор працює, false інакше
     */
    bool is_compressor_running() const;
    
    /**
     * @brief Встановлює стан вентилятора (ручний режим)
     * 
     * @param state Стан вентилятора (true - увімкнено)
     * @return ESP_OK при успішному встановленні, інакше код помилки
     */
    esp_err_t set_fan_state(bool state);
    
    /**
     * @brief Отримує поточний стан вентилятора
     * 
     * @return true якщо вентилятор працює, false інакше
     */
    bool is_fan_running() const;
    
    /**
     * @brief Отримує поточну температуру камери
     * 
     * @return Температура в градусах Цельсія
     */
    float get_chamber_temperature() const;
    
private:
    // Датчики температури
    std::unique_ptr<DS18B20Sensor> chamber_temp_sensor_;   ///< Датчик температури камери
    
    // Актуатори
    std::unique_ptr<Relay> compressor_relay_; ///< Реле компресора
    std::unique_ptr<Relay> fan_relay_;       ///< Реле вентилятора
    
    // Параметри керування
    float target_temp_c_;        ///< Цільова температура в °C
    float hysteresis_c_;         ///< Гістерезис в °C
    OperationMode mode_;         ///< Поточний режим роботи
    uint32_t min_compressor_off_time_sec_; ///< Мінімальний час вимкнення компресора в секундах
    
    // Змінні стану
    float current_chamber_temp_c_;    ///< Поточна температура камери
    bool compressor_running_;         ///< Флаг роботи компресора
    bool fan_running_;                ///< Флаг роботи вентилятора
    uint32_t last_compressor_stop_time_; ///< Час останньої зупинки компресора
    uint32_t last_temp_read_time_;    ///< Час останнього зчитування температури
    uint32_t compressor_on_time_;     ///< Загальний час роботи компресора (с)
    uint32_t compressor_cycles_;      ///< Кількість циклів компресора
    uint32_t compressor_start_time_;  ///< Час запуску компресора (для підрахунку робочого часу)
    
    /**
     * @brief Зчитує значення температури з датчика камери
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
     * @brief Оновлює статистику роботи компресора
     */
    void update_compressor_statistics();
};

#endif // MODULES_COOLING_CONTROL_H