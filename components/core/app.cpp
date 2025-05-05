// core/app.cpp (Оновлена версія)

#include "app.h"
#include "config.h"           // OK
#include "wifi_manager.h"     // OK
#include "shared_state.h"     // OK
#include "module_manager.h"   // OK
#include "event_bus.h"        // Для подієвої шини
#include "esp_log.h"
#include "nvs_flash.h"        // OK
#include "esp_event.h"        // OK
#include "esp_netif.h"        // OK
#include "esp_littlefs.h"     // OK

static const char* TAG = "CoreApp";

// --- Вбудовування default_config.json ---
// Припускаємо, що файл config/default_config.json існує
// і в головному CMakeLists.txt проєкту додано:
// idf_component_get_property(main_comp main SRCS)
// target_add_binary_data(${main_comp} "config/default_config.json" TEXT)
//
// Це створить символи _binary_config_default_config_json_start та _binary_config_default_config_json_end
extern const char default_config_json_start[] asm("_binary_config_default_config_json_start");
// extern const char default_config_json_end[]   asm("_binary_config_default_config_json_end");
// Кінець не потрібен, бо рядок null-terminated при вбудовуванні як TEXT

namespace CoreApp {

esp_err_t init() {
    esp_err_t err;

    ESP_LOGI(TAG, "Ініціалізація NVS...");
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS потребує стирання, форматую...");
        ESP_ERROR_CHECK(nvs_flash_erase()); // Використовуйте ESP_ERROR_CHECK для критичних помилок
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err); // Перевіряємо фінальний результат

    ESP_LOGI(TAG, "Ініціалізація esp_netif...");
    ESP_ERROR_CHECK(esp_netif_init());

    ESP_LOGI(TAG, "Ініціалізація event loop...");
    // Дозволяємо повторну ініціалізацію, якщо цикл вже створено
    err = esp_event_loop_create_default();
     if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
         ESP_LOGE(TAG, "Помилка створення event loop: %s", esp_err_to_name(err));
         return err; // Повертаємо помилку, якщо не вдалося створити
     }


    ESP_LOGI(TAG, "Монтування LittleFS (розділ 'storage')...");
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/littlefs",
        .partition_label = "storage", // Переконайтесь, що мітка вірна!
        .format_if_mount_failed = true, // Форматувати, якщо ФС пошкоджена
        .dont_mount = false
    };
    err = esp_vfs_littlefs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Помилка монтування LittleFS: %s", esp_err_to_name(err));
        // Тут може бути критична помилка, якщо ФС потрібна для конфігурації
        return err;
    }

    ESP_LOGI(TAG, "Ініціалізація ConfigLoader...");
    // Передаємо вбудований JSON як рядок C-style
    err = ConfigLoader::init(default_config_json_start);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Помилка ініціалізації ConfigLoader: %s", esp_err_to_name(err));
        return err; // Критично, якщо конфігурація потрібна далі
    }

    ESP_LOGI(TAG, "Ініціалізація EventBus...");
    err = EventBus::init(); // Використовуємо дефолтні розміри черги/стеку
     if (err != ESP_OK) {
        ESP_LOGE(TAG, "Помилка ініціалізації EventBus: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Ініціалізація SharedState...");
    SharedState::init(); // Не повертає помилку

    ESP_LOGI(TAG, "Ініціалізація ModuleManager...");
    ModuleManager::init(); // Не повертає помилку
    
    ESP_LOGI(TAG, "Ініціалізація WiFiManager...");
    err = WiFiManager::init(); // Намагається підключитись або запускає AP
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Помилка ініціалізації WiFiManager: %s", esp_err_to_name(err));
        // Не критично? Можливо, пристрій продовжить роботу оффлайн? Залежить від логіки.
        // return err; // Розкоментувати, якщо Wi-Fi критичний
    }

    ESP_LOGI(TAG, "Ініціалізація ядра CoreApp завершена успішно.");
    return ESP_OK;
}

} // namespace CoreApp