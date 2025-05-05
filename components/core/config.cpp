#include "config.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include "esp_vfs.h"
#include "esp_littlefs.h"

// Визначення статичних членів класу
const char* ConfigLoader::TAG = "ConfigLoader";
cJSON* ConfigLoader::config_json_root = nullptr;
SemaphoreHandle_t ConfigLoader::config_mutex_handle = nullptr;

#define USER_CONFIG_PATH "/littlefs/user_config.json"


// --- Реалізація статичних методів ---

esp_err_t ConfigLoader::init(const char* default_config_json_str) {
    if (config_mutex_handle == nullptr) {
        config_mutex_handle = xSemaphoreCreateMutex();
        if (config_mutex_handle == nullptr) {
            ESP_LOGE(TAG, "Не вдалося створити м'ютекс!");
            return ESP_FAIL;
        }
    }

    if (xSemaphoreTake(config_mutex_handle, portMAX_DELAY) != pdTRUE) {
         ESP_LOGE(TAG, "Не вдалося захопити м'ютекс для init!");
         return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Ініціалізація конфігурації...");

    // Спочатку видаляємо стару конфігурацію, якщо вона є
    if (config_json_root) {
        cJSON_Delete(config_json_root);
        config_json_root = nullptr;
    }

    // 1. Парсимо дефолтну конфігурацію
    cJSON* default_json = nullptr;
    if (default_config_json_str && strlen(default_config_json_str) > 0) {
        default_json = cJSON_Parse(default_config_json_str);
        if (!default_json) {
            ESP_LOGE(TAG, "Помилка парсингу default_config_json: %s", cJSON_GetErrorPtr());
             xSemaphoreGive(config_mutex_handle);
            return ESP_FAIL;
        }
    } else {
        ESP_LOGI(TAG, "Дефолтна конфігурація не надана, створюємо порожній об'єкт.");
        default_json = cJSON_CreateObject(); // Створюємо порожній об'єкт, якщо дефолтної немає
    }
     if (!cJSON_IsObject(default_json)) {
         ESP_LOGE(TAG, "Дефолтна конфігурація не є JSON об'єктом!");
         cJSON_Delete(default_json);
         xSemaphoreGive(config_mutex_handle);
         return ESP_FAIL;
     }


     // 2. Читаємо користувацьку конфігурацію з файлу
    char* user_config_str = read_file_to_string(USER_CONFIG_PATH); // Використовуємо приватний статичний метод
    cJSON* user_json = nullptr;
    if (user_config_str) {
        user_json = cJSON_Parse(user_config_str);
        free(user_config_str); // Звільняємо буфер файлу
        if (!user_json) {
            ESP_LOGW(TAG, "Помилка парсингу user_config.json: %s. Буде використана дефолтна.", cJSON_GetErrorPtr());
        } else if (!cJSON_IsObject(user_json)) {
             ESP_LOGW(TAG, "user_config.json не є JSON об'єктом. Буде використана дефолтна.");
             cJSON_Delete(user_json);
             user_json = nullptr; // Ігноруємо невалідний JSON
        } else {
             ESP_LOGI(TAG, "Зчитано user_config.json");
        }
    }

    // 3. Мерджимо конфігурації (якщо є валідна користувацька)
    if (user_json) {
        ESP_LOGI(TAG, "Мерджимо користувацьку конфігурацію поверх дефолтної...");
        // cJSON_MergePatch перезаписує default_json!
        if (!cJSON_MergePatch(default_json, user_json)) {
             ESP_LOGE(TAG, "Помилка злиття конфігурацій!");
             // Продовжуємо з тим, що є в default_json
        }
        cJSON_Delete(user_json); // Видаляємо тимчасовий об'єкт
        set_config_root(default_json); // Тепер default_json містить змерджену версію
    } else {
        // Якщо користувацького JSON немає або він невалідний, просто використовуємо дефолтний
        set_config_root(default_json);
    }

    xSemaphoreGive(config_mutex_handle);

    if (!config_json_root) {
        ESP_LOGE(TAG, "Фінальна конфігурація порожня після ініціалізації!");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "ConfigLoader ініціалізовано успішно.");
    return ESP_OK;
}

cJSON* ConfigLoader::getConfigJson() {
    // М'ютекс береться всередині get_mutex(), якщо реалізовано так,
    // але для безпеки краще брати його явно тут.
    if (xSemaphoreTake(config_mutex_handle, portMAX_DELAY) != pdTRUE) return nullptr;
    cJSON* root = get_config_root();
    cJSON* duplicate = nullptr;
    if (root) {
         duplicate = cJSON_Duplicate(root, true /* recurse */);
    }
    xSemaphoreGive(config_mutex_handle);
    return duplicate; // Викликаюча сторона має викликати cJSON_Delete()
}


// --- Реалізація приватних статичних допоміжних методів ---

esp_err_t ConfigLoader::save_config_to_file() {
    // Примітка: цей метод викликається з set<T>, де м'ютекс вже захоплено
    cJSON* root = get_config_root();
    if (!root) {
        ESP_LOGE(TAG, "Конфігурація не ініціалізована для збереження.");
        return ESP_FAIL;
    }

    char* json_str = cJSON_PrintUnformatted(root);
    if (!json_str) {
        ESP_LOGE(TAG, "Помилка серіалізації JSON.");
        return ESP_FAIL;
    }

    FILE* f = fopen(USER_CONFIG_PATH, "w");
    if (f == nullptr) {
        ESP_LOGE(TAG, "Не вдалося відкрити %s для запису", USER_CONFIG_PATH);
        cJSON_free(json_str);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Збереження конфігурації у %s", USER_CONFIG_PATH); // Змінено на Debug рівень
    size_t len = strlen(json_str);
    size_t written = fwrite(json_str, 1, len, f);
    fclose(f);
    cJSON_free(json_str);

    if (written != len) {
         ESP_LOGE(TAG, "Помилка запису у файл конфігурації (записано %d з %d)", written, len);
         return ESP_FAIL;
    }

    return ESP_OK;
}

char* ConfigLoader::read_file_to_string(const char* path) {
    // Цей метод може викликатись лише з init, де м'ютекс вже захоплено
    FILE* f = fopen(path, "r");
    if (f == nullptr) {
        ESP_LOGI(TAG, "Файл %s не знайдено.", path);
        return nullptr;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
         fclose(f);
         ESP_LOGW(TAG, "Файл %s порожній або помилка розміру.", path);
         return nullptr;
    }
     // Додамо перевірку на занадто великий файл
    if (size > 100 * 1024) { // Обмеження 100 KB для конфігу
         fclose(f);
         ESP_LOGE(TAG, "Файл %s занадто великий (%ld bytes)", path, size);
         return nullptr;
    }


    char* buffer = (char*)malloc(size + 1);
    if (!buffer) {
        fclose(f);
        ESP_LOGE(TAG, "Не вдалося виділити пам'ять (%ld байт) для читання %s", size + 1, path);
        return nullptr;
    }

    size_t bytes_read = fread(buffer, 1, size, f);
    fclose(f);

    if(bytes_read != (size_t)size){
        ESP_LOGE(TAG, "Помилка читання файлу %s (прочитано %d з %ld)", path, bytes_read, size);
        free(buffer);
        return nullptr;
    }

    buffer[size] = '\0'; // Null-terminate
    return buffer;
}

std::vector<std::string> ConfigLoader::split_path(const char* path) {
    // Може викликатись з get/set, де м'ютекс захоплено
    std::vector<std::string> parts;
    if (!path || path[0] != '/') return parts; // Повинен починатися з '/'
    if (strlen(path) == 1) return parts; // Тільки "/"
    std::stringstream ss(path + 1); // Пропускаємо перший '/'
    std::string part;
    while (std::getline(ss, part, '/')) {
        if (!part.empty()) {
            parts.push_back(part);
        }
    }
    return parts;
}

cJSON* ConfigLoader::find_or_create_node_by_path(cJSON* root, const std::vector<std::string>& parts) {
     // Може викликатись з set, де м'ютекс захоплено
    cJSON* current = root;
    for (const auto& part : parts) {
        if (!current) return nullptr; // Помилка на попередньому кроці
        cJSON* next = cJSON_GetObjectItemCaseSensitive(current, part.c_str());
        if (!next) {
            // Створюємо проміжний об'єкт
            next = cJSON_AddObjectToObject(current, part.c_str());
            if (!next) {
                ESP_LOGE(TAG, "Не вдалося створити проміжний об'єкт %s", part.c_str());
                return nullptr; // Помилка створення
            }
        } else if (!cJSON_IsObject(next)) {
             ESP_LOGE(TAG, "Елемент шляху %s не є об'єктом, неможливо створити вкладений елемент", part.c_str());
             return nullptr; // Проміжний елемент не є об'єктом
        }
        current = next;
    }
    return current; // Повертає батьківський вузол для останнього елемента шляху
}

cJSON* ConfigLoader::find_node_by_path(cJSON* root, const std::vector<std::string>& parts) {
     // Може викликатись з get, де м'ютекс захоплено
    cJSON* current = root;
    for (const auto& part : parts) {
        if (!current || !cJSON_IsObject(current)) return nullptr; // Перевірка перед пошуком
        current = cJSON_GetObjectItemCaseSensitive(current, part.c_str());
        if (!current) return nullptr; // Не знайдено
    }
    return current;
}