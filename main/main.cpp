/* ModuChill Firmware - Універсальний контролер холодильного обладнання
   
   (c) 2025 - Проект ModuChill
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "app.h"           // API ядра
#include "module_manager.h" // Для tick_all()
#include "event_bus.h"      // Для публікації подій
#include "web_interface.h"  // Для запуску веб-інтерфейсу
#include "hal.h"            // Hardware Abstraction Layer
#include "fridge_controller.h" // Модуль контролера холодильника

static const char* TAG = "AppMain";

// --- Реєстрація модулів ---
void register_all_modules() {
    ESP_LOGI(TAG, "Реєстрація модулів...");
    
    // Реєстрація модуля контролера холодильника
    ModuleManager::register_module(new FridgeControllerModule());
    
    // Тут можна додати інші модулі по мірі їх розробки
    // ModuleManager::register_module(new AnotherModule());
    
    ESP_LOGI(TAG, "Модулі зареєстровано");
}

// Головна функція програми
extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Запуск ModuChill Firmware...");

    // 1. Ініціалізація ядра
    esp_err_t init_result = CoreApp::init();

    if (init_result != ESP_OK) {
        ESP_LOGE(TAG, "Помилка ініціалізації ядра! Код: %d (%s)", init_result, esp_err_to_name(init_result));
        ESP_LOGE(TAG, "Система не може продовжити роботу. Перезавантаження через 10 сек...");
        vTaskDelay(pdMS_TO_TICKS(10000));
        esp_restart();
        return; // Не досягнемо сюди
    }

    ESP_LOGI(TAG, "Ядро ініціалізовано.");
    
    // 1.5. Ініціалізація HAL
    esp_err_t hal_result = HAL::init();
    if (hal_result != ESP_OK) {
        ESP_LOGE(TAG, "Помилка ініціалізації HAL! Код: %d (%s)", hal_result, esp_err_to_name(hal_result));
        // Продовжуємо, оскільки базова система може працювати
        ESP_LOGW(TAG, "Продовження запуску з обмеженою функціональністю...");
    } else {
        ESP_LOGI(TAG, "HAL ініціалізовано.");
    }

    // 2. Реєстрація модулів 
    register_all_modules(); 
    ModuleManager::init_modules();

    // 3. Запуск веб-інтерфейсу (після ініціалізації модулів)
    esp_err_t web_start_result = web_interface_start();
    if (web_start_result != ESP_OK) {
        ESP_LOGE(TAG, "Помилка запуску веб-інтерфейсу! Код: %d (%s)", web_start_result, esp_err_to_name(web_start_result));
        // Не критично? Продовжуємо роботу без веб-інтерфейсу?
    } else {
        ESP_LOGI(TAG, "Веб-інтерфейс запущено.");
    }

    // 4. Публікація події старту системи
    EventBus::publish("SystemStarted");

    ESP_LOGI(TAG, "Система готова до роботи. Вхід у головний цикл.");

    // 5. Головний цикл (виконання ModuleManager::tick_all)
    while (true) {
        ModuleManager::tick_all();

        // Затримка циклу, щоб дати час іншим задачам (сервер, події тощо)
        vTaskDelay(pdMS_TO_TICKS(10)); // 10 мс - налаштуйте за потребою
    }

    // Цей код ніколи не виконається
    ESP_LOGI(TAG, "Зупинка app_main.");
}