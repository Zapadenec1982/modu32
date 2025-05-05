#ifndef CORE_CONFIG_H
#define CORE_CONFIG_H

#include "esp_err.h"
#include "cJSON.h"
#include <string> // Додано для std::string
#include <vector> // Додано для split_path та інших
#include <sstream> // Додано для split_path
#include "esp_log.h" // Для логування в get/set
#include "freertos/FreeRTOS.h" // Для доступу до примітивів синхронізації
#include "freertos/semphr.h" // Для доступу до примітивів синхронізації

// Оголошення допоміжних функцій та змінних з .cpp, які потрібні шаблонам
// Або перенесення їх у приватну секцію класу, якщо робимо НЕ статичний клас
// Для статичного класу - залишити в анонімному неймспейсі в .cpp або
// зробити їх статичними приватними членами класу ConfigLoader.
// Щоб зберегти статичність, зробимо їх статичними приватними.

class ConfigLoader {
public:
    // Ініціалізація залишається статичною
    static esp_err_t init(const char* default_config_json);

    // --- Шаблонні Getters ---
    template <typename T>
    static T get(const char* path, T default_value) {
        if (xSemaphoreTake(get_mutex(), portMAX_DELAY) != pdTRUE) {
            ESP_LOGE(TAG, "Не вдалося захопити м'ютекс для get(%s)", path ? path : "NULL");
            return default_value;
        }

        T result = default_value;
        cJSON* root_node = get_config_root(); // Отримуємо доступ до root JSON
        if (root_node) {
            std::vector<std::string> parts = split_path(path);
            cJSON* node = find_node_by_path(root_node, parts);

            // Спеціалізація логіки для різних типів T
            if (node) {
                if constexpr (std::is_same_v<T, int>) {
                    if (cJSON_IsNumber(node)) result = (int)node->valuedouble;
                } else if constexpr (std::is_same_v<T, float>) {
                    if (cJSON_IsNumber(node)) result = (float)node->valuedouble;
                } else if constexpr (std::is_same_v<T, double>) {
                    if (cJSON_IsNumber(node)) result = node->valuedouble;
                } else if constexpr (std::is_same_v<T, bool>) {
                    if (cJSON_IsBool(node)) result = cJSON_IsTrue(node);
                } else if constexpr (std::is_same_v<T, std::string>) {
                    if (cJSON_IsString(node) && node->valuestring) result = node->valuestring;
                }
                // Додайте інші типи за потреби
            }
        } else {
             ESP_LOGE(TAG, "Конфігурація не ініціалізована для get(%s)", path ? path : "NULL");
        }

        xSemaphoreGive(get_mutex());
        return result;
    }

     // Окремий get для const char*, щоб уникнути проблем з управлінням пам'яттю
     // Повертає тимчасовий вказівник, що небезпечно, або копію (краще string)
     // Залишимо повернення std::string як основний варіант для рядків.


    // --- Шаблонні Setters ---
     template <typename T>
     static bool set(const char* path, T value) {
        if (xSemaphoreTake(get_mutex(), portMAX_DELAY) != pdTRUE) {
             ESP_LOGE(TAG, "Не вдалося захопити м'ютекс для set(%s)", path ? path : "NULL");
             return false;
        }

        bool success = false;
        cJSON* root_node = get_config_root();
        std::vector<std::string> parts = split_path(path);

        if (!parts.empty() && root_node) {
            std::string leaf_name = parts.back();
            parts.pop_back(); // Шлях до батьківського вузла

            cJSON* parent = parts.empty() ? root_node : find_or_create_node_by_path(root_node, parts);

            if (parent && cJSON_IsObject(parent)) {
                 cJSON* new_item = nullptr;
                 // Спеціалізація створення cJSON для різних типів T
                 if constexpr (std::is_same_v<T, int> || std::is_same_v<T, float> || std::is_same_v<T, double>) {
                     new_item = cJSON_CreateNumber(static_cast<double>(value));
                 } else if constexpr (std::is_same_v<T, bool>) {
                     new_item = cJSON_CreateBool(value);
                 } else if constexpr (std::is_same_v<T, std::string>) {
                     new_item = cJSON_CreateString(value.c_str());
                 } else if constexpr (std::is_same_v<T, const char*>) {
                      new_item = cJSON_CreateString(value);
                 }
                 // Додайте інші типи за потреби

                if (new_item) {
                    if (cJSON_HasObjectItem(parent, leaf_name.c_str())) {
                        cJSON_ReplaceItemInObjectCaseSensitive(parent, leaf_name.c_str(), new_item);
                    } else {
                        cJSON_AddItemToObject(parent, leaf_name.c_str(), new_item);
                    }
                    // Збереження у файл
                    if (save_config_to_file() == ESP_OK) {
                        success = true;
                    } else {
                        ESP_LOGE(TAG, "Помилка збереження конфігурації після set для %s", path);
                        // TODO: Відкат зміни в пам'яті?
                    }
                } else {
                    ESP_LOGE(TAG, "Не вдалося створити cJSON елемент для %s", path);
                }
            } else {
                ESP_LOGE(TAG, "Не вдалося знайти/створити батьківський вузол для %s", path);
            }
        } else {
            ESP_LOGE(TAG, "Некоректний шлях або конфігурація не ініціалізована для %s", path);
        }

        xSemaphoreGive(get_mutex());
        return success;
     }


    // Отримання копії всього JSON
    static cJSON* getConfigJson();

private:
    // Приватні статичні члени для зберігання стану та синхронізації
    static cJSON* config_json_root;
    static SemaphoreHandle_t config_mutex_handle;
    static const char* TAG; // Тег для логування

    // Приватні статичні допоміжні методи
    static esp_err_t save_config_to_file();
    static char* read_file_to_string(const char* path);
    static std::vector<std::string> split_path(const char* path);
    static cJSON* find_or_create_node_by_path(cJSON* root, const std::vector<std::string>& parts);
    static cJSON* find_node_by_path(cJSON* root, const std::vector<std::string>& parts);

    // Доступ до приватних членів (безпечно, оскільки методи статичні)
    static SemaphoreHandle_t get_mutex() { return config_mutex_handle; }
    static cJSON* get_config_root() { return config_json_root; }
    static void set_config_root(cJSON* root) { config_json_root = root; }

    // Забороняємо створення екземплярів
    ConfigLoader() = delete;
    ~ConfigLoader() = delete;
    ConfigLoader(const ConfigLoader&) = delete;
    ConfigLoader& operator=(const ConfigLoader&) = delete;

};

#endif // CORE_CONFIG_H