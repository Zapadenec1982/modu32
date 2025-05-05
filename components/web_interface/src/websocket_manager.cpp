#include "websocket_manager.h"
#include "event_bus.h" // Для підписки на події системи
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "mongoose.h"
#include "cJSON.h"
#include <set>     // Використовуємо set для автоматичного уникнення дублікатів та швидкого пошуку/видалення
#include <mutex>   // Використаємо std::mutex для простоти в цьому модулі
#include <vector>  // Для копіювання списку клієнтів перед розсилкою

static const char *TAG = "WebSocketManager";

namespace {
    // Використовуємо std::set для зберігання унікальних вказівників на з'єднання
    static std::set<struct mg_connection *> s_clients;
    // М'ютекс для захисту доступу до s_clients
    static std::mutex s_clients_mutex;

    // Обробник подій від EventBus
    static void websocket_event_handler(const std::string& event_name, void* event_data) {
        ESP_LOGD(TAG, "Отримано подію '%s' від EventBus для WebSocket", event_name.c_str());

        // Формуємо JSON повідомлення на основі події
        // Це приклад, реальна структура залежить від ваших подій
        cJSON *payload = cJSON_CreateObject();
        if (!payload) return;

        cJSON_AddStringToObject(payload, "event", event_name.c_str());

        // --- Додавання даних залежно від типу події ---
        // Приклад для події оновлення температури
        if (event_name == "temperature_update" && event_data) {
            // Припускаємо, що event_data це вказівник на структуру
            // typedef struct { const char* id; float value; } TempData;
            // TempData* data = static_cast<TempData*>(event_data);
            cJSON* data_obj = cJSON_CreateObject();
            // cJSON_AddStringToObject(data_obj, "sensorId", data->id);
            // cJSON_AddNumberToObject(data_obj, "value", data->value);
             cJSON_AddStringToObject(data_obj, "detail", "Приклад даних події"); // Заглушка
            cJSON_AddItemToObject(payload, "data", data_obj);
        }
        // Приклад для події без даних
        else if (event_name == "relay_toggled") {
             cJSON_AddNullToObject(payload, "data");
        }
        // ... інші обробники подій ...
        else {
             // Подія невідома або не має даних для WS
             cJSON_Delete(payload);
             return;
        }
        // --- Кінець додавання даних ---


        // Надсилаємо JSON всім підключеним клієнтам
        websocket_broadcast_json(payload); // Ця функція НЕ видаляє payload

        cJSON_Delete(payload); // Видаляємо створений JSON
    }

} // Анонімний namespace


// --- Реалізація публічних функцій ---

esp_err_t websocket_manager_init() {
    ESP_LOGI(TAG, "Ініціалізація WebSocket Manager...");
    {
        // Очищаємо список клієнтів при старті
        std::lock_guard<std::mutex> lock(s_clients_mutex);
        s_clients.clear();
    }

    // Підписуємось на системні події, які хочемо транслювати
    // TODO: Замініть "some_event", "temperature_update" на реальні імена ваших подій
    EventBus::subscribe("some_event", websocket_event_handler);
    EventBus::subscribe("temperature_update", websocket_event_handler);
    EventBus::subscribe("relay_toggled", websocket_event_handler);
     EventBus::subscribe("SystemStarted", websocket_event_handler); // Наприклад

    ESP_LOGI(TAG, "Підписано на події EventBus для трансляції WebSocket.");

    return ESP_OK;
}

void websocket_on_open(struct mg_connection *c) {
    if (!c) return;
    {
        std::lock_guard<std::mutex> lock(s_clients_mutex);
        s_clients.insert(c); // Додаємо нового клієнта до списку
    }
    ESP_LOGI(TAG, "WebSocket client connected (ID: %lu, Total: %d)", c->id, s_clients.size());
    // Можна надіслати вітальне повідомлення новому клієнту
    // mg_ws_send(c, "{\"event\":\"welcome\"}", strlen("{\"event\":\"welcome\"}"), WEBSOCKET_OP_TEXT);
}

void websocket_on_close(struct mg_connection *c) {
    if (!c) return;
    bool removed = false;
    {
        std::lock_guard<std::mutex> lock(s_clients_mutex);
        removed = s_clients.erase(c); // Видаляємо клієнта зі списку
    }
     if (removed) {
        ESP_LOGI(TAG, "WebSocket client disconnected (ID: %lu, Total: %d)", c->id, s_clients.size());
     } else {
         ESP_LOGW(TAG, "WebSocket client disconnect event for unknown connection (ID: %lu)", c->id);
     }
}

void websocket_on_message(struct mg_connection *c, const char* message, size_t len) {
    if (!c || !message) return;
    ESP_LOGD(TAG, "WebSocket message from client (ID: %lu, len: %d): %.*s", c->id, len, (int)len, message);

    // TODO: Обробка повідомлень від клієнта
    // Наприклад, парсинг JSON команд
    cJSON* req_json = cJSON_ParseWithLength(message, len);
    if (req_json) {
        // Обробити команду...
        // Наприклад, надіслати відповідь
        // cJSON* resp_json = cJSON_CreateObject();
        // cJSON_AddStringToObject(resp_json, "status", "ok");
        // char* resp_str = cJSON_PrintUnformatted(resp_json);
        // if(resp_str) {
        //    mg_ws_send(c, resp_str, strlen(resp_str), WEBSOCKET_OP_TEXT);
        //    free(resp_str);
        // }
        // cJSON_Delete(resp_json);
        cJSON_Delete(req_json); // Видаляємо розпарсений запит
    } else {
         ESP_LOGW(TAG, "Не вдалося розпарсити JSON від WS клієнта #%lu", c->id);
         // Надіслати помилку клієнту
         const char* err_msg = "{\"error\":\"Invalid JSON\"}";
         mg_ws_send(c, err_msg, strlen(err_msg), WEBSOCKET_OP_TEXT);
    }
}

void websocket_on_error(struct mg_connection *c) {
    // Mongoose зазвичай закриває з'єднання після помилки,
    // тому подія on_close має викликатись пізніше.
    if (c) {
        ESP_LOGE(TAG, "WebSocket error on connection (ID: %lu)", c->id);
        // Примусово видаляємо зі списку про всяк випадок
        std::lock_guard<std::mutex> lock(s_clients_mutex);
        s_clients.erase(c);
    } else {
         ESP_LOGE(TAG, "WebSocket general error");
    }
}

// Розсилка бінарного/текстового повідомлення всім клієнтам
void websocket_broadcast(const char* message, size_t len) {
    if (!message || len == 0) return;

    // Створюємо копію списку клієнтів, щоб не блокувати м'ютекс надовго
    std::vector<struct mg_connection *> clients_copy;
    {
        std::lock_guard<std::mutex> lock(s_clients_mutex);
        // Перевірка на порожній список перед копіюванням
        if (s_clients.empty()) return;
        clients_copy.assign(s_clients.begin(), s_clients.end());
    }

    ESP_LOGD(TAG, "Broadcasting message (len %d) to %d clients", len, clients_copy.size());
    for (struct mg_connection *client : clients_copy) {
        mg_ws_send(client, message, len, WEBSOCKET_OP_TEXT); // Або WEBSOCKET_OP_BINARY?
    }
}

// Розсилка JSON повідомлення всім клієнтам
void websocket_broadcast_json(const cJSON* json) {
    if (!json) return;

    char *json_str = cJSON_PrintUnformatted(json); // Серіалізуємо JSON
    if (json_str) {
        websocket_broadcast(json_str, strlen(json_str)); // Викликаємо базовий broadcast
        cJSON_free(json_str); // Звільняємо рядок
    } else {
        ESP_LOGE(TAG, "Не вдалося серіалізувати JSON для broadcast");
    }
    // ВАЖЛИВО: Ми не видаляємо вхідний об'єкт 'json' тут!
    // Його має видалити той, хто викликав цю функцію (напр., websocket_event_handler).
}