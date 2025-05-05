#include "web_interface.h"
#include "server_mongoose.h"
#include "rpc_api.h"
#include "websocket_manager.h"
#include "esp_log.h"
#include "module_manager.h" // Для отримання списку модулів
#include "base_module.h"    // Для BaseModule
#include "shared_state.h"   // Для отримання стану системи
#include "cJSON.h"
#include "mongoose.h"       // Для структур Mongoose

static const char *TAG = "WebInterface";

esp_err_t web_interface_init(void) {
    ESP_LOGI(TAG, "Ініціалізація компонента WebInterface...");
    esp_err_t ret;

    ret = rpc_api_init();
    if (ret != ESP_OK) return ret;

    ret = websocket_manager_init();
    if (ret != ESP_OK) return ret;

    ret = server_mongoose_init();
    if (ret != ESP_OK) return ret;

    ESP_LOGI(TAG, "WebInterface ініціалізовано.");
    return ESP_OK;
}

esp_err_t web_interface_start(void) {
    ESP_LOGI(TAG, "Запуск WebInterface (сервера)...");
    return server_mongoose_start();
}

esp_err_t web_interface_stop(void) {
    ESP_LOGI(TAG, "Зупинка WebInterface (сервера)...");
    return server_mongoose_stop();
}

// Функція для генерації JSON-схеми
void schema_handler(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
    if (ev != MG_EV_HTTP_MSG) return;

    struct mg_http_message *hm = (struct mg_http_message*)ev_data;
    
    if (!mg_http_match_uri(hm, "/api/schema")) return;
    
    ESP_LOGI(TAG, "Отримано запит на /api/schema");

    cJSON *root = cJSON_CreateObject();
    
    // Додавання статусу системи
    cJSON *status_obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(status_obj, "wifi_connected", SharedState::get<bool>("wifi_connected", false));
    cJSON_AddNumberToObject(status_obj, "temperature", SharedState::get<float>("temperature", 0.0f));
    cJSON_AddBoolToObject(status_obj, "compressor_active", SharedState::get<bool>("compressor_active", false));
    cJSON_AddBoolToObject(status_obj, "fan_active", SharedState::get<bool>("fan_active", false));
    cJSON_AddStringToObject(status_obj, "mode", SharedState::get<std::string>("mode", "auto").c_str());
    cJSON_AddItemToObject(root, "status", status_obj);
    
    // Додавання конфігурації
    cJSON *config_obj = cJSON_CreateObject();
    cJSON *temp_control = cJSON_CreateObject();
    cJSON_AddNumberToObject(temp_control, "set_temp", SharedState::get<float>("set_temp", 5.0f));
    cJSON_AddNumberToObject(temp_control, "hysteresis", SharedState::get<float>("hysteresis", 1.0f));
    cJSON_AddNumberToObject(temp_control, "min_compressor_off_time", SharedState::get<int>("min_compressor_off_time", 300));
    cJSON_AddItemToObject(config_obj, "temp_control", temp_control);
    cJSON_AddItemToObject(root, "config", config_obj);
    
    // Додавання інформації про модулі
    cJSON *modules_array = cJSON_CreateArray();
    const std::vector<BaseModule*> modules = ModuleManager::get_all_modules();
    
    for (BaseModule *module : modules) {
        if (module) {
            cJSON *module_obj = cJSON_CreateObject();
            cJSON_AddStringToObject(module_obj, "name", module->getName());
            
            // Додавання UI схеми модуля
            cJSON *ui_schema = cJSON_CreateObject();
            module->get_ui_schema(ui_schema);
            cJSON_AddItemToObject(module_obj, "ui_schema", ui_schema);
            
            cJSON_AddItemToArray(modules_array, module_obj);
        }
    }
    
    cJSON_AddItemToObject(root, "modules", modules_array);
    
    // Додавання системної інформації
    cJSON *system_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(system_obj, "firmware_version", "0.1.0");
    cJSON_AddStringToObject(system_obj, "device_id", SharedState::get<std::string>("device_id", "MC-001").c_str());
    cJSON_AddItemToObject(root, "system", system_obj);

    // Перетворення в рядок і відправка
    char *json_str = cJSON_Print(root);
    mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n", "%s", json_str);
    
    // Звільнення пам'яті
    free(json_str);
    cJSON_Delete(root);
}