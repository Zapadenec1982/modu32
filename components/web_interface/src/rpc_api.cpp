// rpc_api.cpp

#include "rpc_api.h"
#include "esp_log.h"
#include "cJSON.h"
#include <map>
#include <string>
#include <mutex>
#include <memory>

// --- Додані залежності для обробників ---
#include "core/config.h"
#include "core/shared_state.h"
#include "core/wifi_manager.h"
#include "esp_system.h"       // Для esp_chip_info, esp_get_free_heap_size, esp_restart
#include "esp_chip_info.h"    // Для esp_chip_info
#include "esp_app_format.h"   // Для esp_app_get_description
#include "esp_timer.h"        // Для esp_timer_get_time
#include "esp_heap_caps.h"    // Для esp_get_free_heap_size

static const char *TAG = "RPC_API";

// ... (анонімний неймспейс з create_jsonrpc_error, create_jsonrpc_response, cJSONDeleter залишається без змін) ...
namespace {
    // ... (код з попередньої відповіді: s_rpc_handlers, s_handler_mutex, create_jsonrpc_error, create_jsonrpc_response, cJSONDeleter, cJSONUniquePtr) ...
     static std::map<std::string, rpc_handler_func_t> s_rpc_handlers;
     static std::mutex s_handler_mutex;

     cJSON* create_jsonrpc_error(int code, const char* message) {
         // ... (реалізація з попередньої відповіді) ...
         cJSON *error_obj = cJSON_CreateObject();
         if (!error_obj) return nullptr;
         cJSON_AddNumberToObject(error_obj, "code", code);
         cJSON_AddStringToObject(error_obj, "message", message);
         return error_obj;
     }
     cJSON* create_jsonrpc_response(cJSON* id, cJSON* result, cJSON* error) {
          // ... (реалізація з попередньої відповіді) ...
        cJSON *response = cJSON_CreateObject();
        if (!response) return nullptr;
        cJSON_AddStringToObject(response, "jsonrpc", "2.0");
        if (id) {
            cJSON_AddItemToObject(response, "id", cJSON_Duplicate(id, true));
        } else {
             cJSON_AddNullToObject(response, "id");
        }
        if (error) {
            cJSON_AddItemToObject(response, "error", error);
        } else if (result) {
            cJSON_AddItemToObject(response, "result", result);
        } else {
             if(id) cJSON_AddNullToObject(response, "result");
        }
        return response;
     }
     struct cJSONDeleter { void operator()(cJSON* ptr) const { if (ptr) cJSON_Delete(ptr); } };
     using cJSONUniquePtr = std::unique_ptr<cJSON, cJSONDeleter>;

    // --- Функції-обробники для RPC-методів ---

    /**
     * @brief Обробник для System.GetStatus
     */
    cJSON* handle_system_get_status(const cJSON* params) {
        ESP_LOGD(TAG, "Виклик handle_system_get_status");
        cJSON *result = cJSON_CreateObject();
        if (!result) return nullptr;

        // Статус Wi-Fi
        cJSON_AddBoolToObject(result, "wifiConnected", WiFiManager::isConnected());

        // Час роботи
        int64_t uptime_us = esp_timer_get_time();
        cJSON_AddNumberToObject(result, "uptimeSeconds", (double)(uptime_us / 1000000));

        // Вільна пам'ять (Heap)
        cJSON_AddNumberToObject(result, "freeHeap", esp_get_free_heap_size());
        cJSON_AddNumberToObject(result, "minFreeHeap", esp_get_minimum_free_heap_size());

        // Інформація про чіп
        esp_chip_info_t chip_info;
        esp_chip_info(&chip_info);
        cJSON* chip_info_json = cJSON_CreateObject();
        if (chip_info_json) {
            cJSON_AddStringToObject(chip_info_json, "model", CONFIG_IDF_TARGET); // ESP32, ESP32-S3 etc.
            cJSON_AddNumberToObject(chip_info_json, "cores", chip_info.cores);
            cJSON_AddNumberToObject(chip_info_json, "revision", chip_info.revision);
            cJSON_AddItemToObject(result, "chipInfo", chip_info_json);
        }

        // Інформація про прошивку
        const esp_app_desc_t *app_desc = esp_app_get_description();
        cJSON* app_info_json = cJSON_CreateObject();
         if (app_info_json && app_desc) {
             cJSON_AddStringToObject(app_info_json, "version", app_desc->version);
             cJSON_AddStringToObject(app_info_json, "appName", app_desc->project_name);
             cJSON_AddStringToObject(app_info_json, "compileDate", app_desc->date);
             cJSON_AddStringToObject(app_info_json, "compileTime", app_desc->time);
             cJSON_AddStringToObject(app_info_json, "idfVersion", app_desc->idf_ver);
             cJSON_AddItemToObject(result, "appInfo", app_info_json);
         }

        return result; // Власність передається rpc_api_handle_request_str
    }

    /**
     * @brief Обробник для Config.GetValue
     */
    cJSON* handle_config_get_value(const cJSON* params) {
         ESP_LOGD(TAG, "Виклик handle_config_get_value");
         // Перевірка параметрів
         if (!cJSON_IsObject(params)) return nullptr; // Помилка буде згенерована вище
         cJSON* path_item = cJSON_GetObjectItemCaseSensitive(params, "path");
         if (!cJSON_IsString(path_item) || !path_item->valuestring) return nullptr;

         const char* path = path_item->valuestring;
         ESP_LOGD(TAG,"Запит Config.GetValue для шляху: %s", path);

         // Обмеження: Наразі повертаємо всі значення як рядки для простоти.
         // TODO: Розширити для повернення правильного типу (int, bool, float...)
         // шляхом аналізу типу вузла cJSON або передачі типу в запиті.
         std::string value = ConfigLoader::get<std::string>(path, ""); // Поверне порожній рядок, якщо шлях не знайдено

         return cJSON_CreateString(value.c_str()); // Власність передається
    }

     /**
      * @brief Обробник для Config.SetValue
      */
     cJSON* handle_config_set_value(const cJSON* params) {
         ESP_LOGD(TAG, "Виклик handle_config_set_value");
          // Перевірка параметрів
         if (!cJSON_IsObject(params)) return nullptr;
         cJSON* path_item = cJSON_GetObjectItemCaseSensitive(params, "path");
         cJSON* value_item = cJSON_GetObjectItemCaseSensitive(params, "value"); // Значення будь-якого типу

         if (!cJSON_IsString(path_item) || !path_item->valuestring || !value_item) return nullptr;

         const char* path = path_item->valuestring;
         bool success = false;

         // Визначаємо тип значення і викликаємо відповідний ConfigLoader::set
         if (cJSON_IsString(value_item)) {
             success = ConfigLoader::set<const char*>(path, value_item->valuestring);
         } else if (cJSON_IsNumber(value_item)) {
             // Зберігаємо як double, ConfigLoader::get<int/float> обробить це
             success = ConfigLoader::set<double>(path, value_item->valuedouble);
         } else if (cJSON_IsBool(value_item)) {
              success = ConfigLoader::set<bool>(path, cJSON_IsTrue(value_item));
         } else if (cJSON_IsNull(value_item)) {
             // TODO: Як обробити null? Видалити ключ? Поки що ігноруємо або повертаємо помилку.
              ESP_LOGW(TAG,"Спроба встановити NULL для Config ключа '%s' не підтримується.", path);
              return nullptr; // Повертаємо помилку
         } else {
              ESP_LOGW(TAG,"Непідтримуваний тип значення для Config ключа '%s'.", path);
              return nullptr; // Повертаємо помилку
         }

          if (success) {
               ESP_LOGI(TAG,"Встановлено значення для Config ключа '%s'", path);
               // Повертаємо простий успіх
               return cJSON_CreateTrue(); // Власність передається
          } else {
               ESP_LOGE(TAG,"Помилка встановлення значення для Config ключа '%s'", path);
               // Повертаємо NULL, що призведе до Internal error
               return nullptr;
          }
     }

     /**
     * @brief Обробник для SharedState.GetValue
     */
    cJSON* handle_sharedstate_get_value(const cJSON* params) {
         ESP_LOGD(TAG, "Виклик handle_sharedstate_get_value");
         if (!cJSON_IsObject(params)) return nullptr;
         cJSON* key_item = cJSON_GetObjectItemCaseSensitive(params, "key");
         if (!cJSON_IsString(key_item) || !key_item->valuestring) return nullptr;

         const char* key = key_item->valuestring;
         ESP_LOGD(TAG,"Запит SharedState.GetValue для ключа: %s", key);

          // Обмеження: Також повертаємо як рядок для простоти.
          // TODO: Розширити для повернення правильного типу JSON на основі ValueType.
         std::string value = SharedState::get<std::string>(key, "");

         return cJSON_CreateString(value.c_str());
    }

    // --- Інші обробники (за потреби) ---
    // cJSON* handle_restart_device(const cJSON* params) {
    //      ESP_LOGW(TAG, "Отримано команду перезавантаження через RPC!");
    //      // Можливо, додати невелику затримку перед перезавантаженням
    //      vTaskDelay(pdMS_TO_TICKS(500));
    //      esp_restart();
    //      return cJSON_CreateTrue(); // Хоча до цього не дійде
    // }


} // Анонімний namespace

// --- Реалізація публічних функцій ---

esp_err_t rpc_api_init() {
    ESP_LOGI(TAG, "Ініціалізація RPC API та реєстрація обробників...");
    // Очищуємо мапу обробників при ініціалізації
    std::lock_guard<std::mutex> lock(s_handler_mutex);
    s_rpc_handlers.clear();

    // Реєструємо базові методи
    rpc_api_register_handler("System.GetStatus", handle_system_get_status);
    rpc_api_register_handler("Config.GetValue", handle_config_get_value);
    rpc_api_register_handler("Config.SetValue", handle_config_set_value);
    rpc_api_register_handler("SharedState.GetValue", handle_sharedstate_get_value);
    // rpc_api_register_handler("System.Restart", handle_restart_device);

    // TODO: Дозволити модулям реєструвати власні RPC-методи,
    // викликаючи rpc_api_register_handler() зі свого init()

    return ESP_OK;
}

// Реалізація rpc_api_register_handler та rpc_api_handle_request_str залишається такою ж,
// як у попередній відповіді, оскільки вона містить загальну логіку обробки.

// ... (код для rpc_api_register_handler та rpc_api_handle_request_str з попередньої відповіді) ...

esp_err_t rpc_api_register_handler(const char* method_name, rpc_handler_func_t handler) {
     // ... (реалізація з попередньої відповіді) ...
     if (!method_name || !handler) {
        return ESP_ERR_INVALID_ARG;
    }
    std::lock_guard<std::mutex> lock(s_handler_mutex);
    if (s_rpc_handlers.count(method_name)) {
        ESP_LOGW(TAG, "Перезапис обробника для RPC-методу: %s", method_name);
    } else {
        ESP_LOGI(TAG, "Реєстрація RPC-методу: %s", method_name);
    }
    s_rpc_handlers[method_name] = handler;
    return ESP_OK;
}


char* rpc_api_handle_request_str(const char* request_body, size_t body_len) {
     // ... (повна реалізація з попередньої відповіді, яка включає валідацію,
     // пошук обробника в s_rpc_handlers, виклик його, та форматування відповіді/помилки) ...
      ESP_LOGD(TAG, "Обробка RPC-запиту (len: %d): %.*s", body_len, (int)body_len, request_body);

    cJSONUniquePtr request_json(cJSON_ParseWithLength(request_body, body_len));
    cJSON *response_json = nullptr;
    cJSON *error_json = nullptr;
    cJSON *result_json = nullptr;
    cJSON *id_json = nullptr; // Не видаляти через Deleter! Це посилання.
    cJSON *params_json = nullptr; // Не видаляти через Deleter!

    // 1. Перевірка помилки парсингу
    if (!request_json) {
        ESP_LOGE(TAG, "Помилка парсингу JSON-RPC запиту: %s", cJSON_GetErrorPtr());
        error_json = create_jsonrpc_error(-32700, "Parse error");
        response_json = create_jsonrpc_response(nullptr, nullptr, error_json); // ID невідомий
        goto respond;
    }

    // 2. Валідація структури JSON-RPC 2.0
    if (!cJSON_IsObject(request_json.get())) {
        error_json = create_jsonrpc_error(-32600, "Invalid Request - Not an object");
        goto respond;
    }
    cJSON* version = cJSON_GetObjectItemCaseSensitive(request_json.get(), "jsonrpc");
    if (!cJSON_IsString(version) || strcmp(version->valuestring, "2.0") != 0) {
        error_json = create_jsonrpc_error(-32600, "Invalid Request - Invalid jsonrpc version");
        goto respond;
    }
    cJSON* method = cJSON_GetObjectItemCaseSensitive(request_json.get(), "method");
    if (!cJSON_IsString(method) || !method->valuestring || strlen(method->valuestring) == 0) {
        error_json = create_jsonrpc_error(-32600, "Invalid Request - Method missing or invalid");
        goto respond;
    }
    id_json = cJSON_GetObjectItemCaseSensitive(request_json.get(), "id");
    if (id_json && !cJSON_IsString(id_json) && !cJSON_IsNumber(id_json) && !cJSON_IsNull(id_json)) {
         error_json = create_jsonrpc_error(-32600, "Invalid Request - ID must be string, number or null");
         id_json = nullptr;
         goto respond;
    }
    params_json = cJSON_GetObjectItemCaseSensitive(request_json.get(), "params");
    if (params_json && !cJSON_IsObject(params_json) && !cJSON_IsArray(params_json)) {
         error_json = create_jsonrpc_error(-32600, "Invalid Request - Params must be object or array");
         goto respond;
    }

    // 3. Пошук та виклик обробника
    {
        std::lock_guard<std::mutex> lock(s_handler_mutex);
        auto it = s_rpc_handlers.find(method->valuestring);
        if (it != s_rpc_handlers.end()) {
            rpc_handler_func_t handler = it->second;
            ESP_LOGD(TAG, "Виклик обробника для методу '%s'", method->valuestring);
            result_json = handler(params_json); // Обробник повертає NULL при помилці
            if (!result_json) {
                ESP_LOGE(TAG, "Обробник для '%s' повернув NULL (внутрішня помилка)", method->valuestring);
                error_json = create_jsonrpc_error(-32603, "Internal error - Handler failed");
            }
        } else {
            ESP_LOGW(TAG, "Метод не знайдено: %s", method->valuestring);
            error_json = create_jsonrpc_error(-32601, "Method not found");
        }
    }

respond:
    // 4. Формування фінальної відповіді (якщо не notification)
    if (id_json != nullptr && !cJSON_IsNull(id_json)) {
        response_json = create_jsonrpc_response(id_json, result_json, error_json);
    } else {
         ESP_LOGD(TAG, "Запит був notification, відповідь не формується.");
         if(result_json) cJSON_Delete(result_json); // Треба видалити, якщо не використано
         if(error_json) cJSON_Delete(error_json);   // Треба видалити, якщо не використано
         response_json = nullptr;
    }

    // 5. Серіалізація відповіді в рядок
    char* response_str = nullptr;
    if (response_json) {
        response_str = cJSON_PrintUnformatted(response_json);
        cJSON_Delete(response_json);
    }

    return response_str;
}