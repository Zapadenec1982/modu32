#include "server_mongoose.h"
#include "rpc_api.h"          // Для обробки RPC
#include "websocket_manager.h" // Для обробки WebSocket
#include "mongoose.h"         // Основна бібліотека Mongoose
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include <string> // Для перетворення mg_str в std::string при потребі
#include "web_interface.h" //  Для schema_handler

static const char *TAG = "ServerMongoose";

// --- Вбудовані файли UI ---
// Посилання на вбудовані бінарні ресурси (HTML, CSS, JS)
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t style_css_start[] asm("_binary_style_css_start");
extern const uint8_t style_css_end[] asm("_binary_style_css_end");
extern const uint8_t script_js_start[] asm("_binary_script_js_start");
extern const uint8_t script_js_end[] asm("_binary_script_js_end");

// --- Налаштування сервера ---
#define MONGOOSE_LISTEN_ADDR "http://0.0.0.0:80" // Слухати на всіх інтерфейсах, порт 80
#define MONGOOSE_POLL_MS 500                     // Інтервал опитування менеджера подій
#define MONGOOSE_TASK_STACK_SIZE 8192            // Розмір стеку для задачі сервера
#define MONGOOSE_TASK_PRIORITY 5                 // Пріоритет задачі сервера
#define WEBUI_FS_ROOT "/littlefs"                // Корінь файлової системи
#define WEBUI_DOC_ROOT "/www"                    // Директорія з UI файлами відносно кореня ФС
#define RPC_ENDPOINT "/api/rpc"                  // Шлях для RPC запитів
#define SCHEMA_ENDPOINT "/api/schema"            // Шлях для запитів схеми
#define USE_LITTLEFS false                       // Флаг для вибору джерела UI файлів

namespace {
    static struct mg_mgr s_mongoose_mgr;       // Менеджер подій Mongoose
    static TaskHandle_t s_server_task_handle = nullptr; // Хендл задачі сервера
    static bool s_is_initialized = false;
    static bool s_is_running = false;
    static std::string s_full_doc_root = ""; // Повний шлях до статичних файлів

    // Функція для обробки запитів на вбудовані файли
    bool serve_embedded_file(struct mg_connection *c, const char *path) {
        const uint8_t *data_start = nullptr;
        const uint8_t *data_end = nullptr;
        const char *content_type = "text/plain";

        // Визначаємо який файл запитується
        if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
            data_start = index_html_start;
            data_end = index_html_end;
            content_type = "text/html";
        } else if (strcmp(path, "/style.css") == 0) {
            data_start = style_css_start;
            data_end = style_css_end;
            content_type = "text/css";
        } else if (strcmp(path, "/script.js") == 0) {
            data_start = script_js_start;
            data_end = script_js_end;
            content_type = "application/javascript";
        } else {
            return false; // Файл не знайдено
        }

        // Відправляємо заголовок і дані файлу
        size_t length = data_end - data_start;
        mg_printf(c, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n",
                 content_type, length);
        mg_send(c, data_start, length);
        c->is_resp = 1; // Позначаємо, що відповідь відправлено
        
        ESP_LOGI(TAG, "Відправлено вбудований файл: %s (%d байт)", path, length);
        return true;
    }

    // --- Головний обробник подій Mongoose ---
    static void main_event_handler(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
        if (!s_is_running) return; // Ігноруємо події, якщо сервер зупиняється

        switch (ev) {
            case MG_EV_ERROR: {
                ESP_LOGE(TAG, "Mongoose error: %s", ev_data ? (char*)ev_data : "Unknown");
                // Передаємо помилку в WS менеджер, якщо це WS з'єднання
                 if (c->is_websocket) {
                     websocket_on_error(/* передати контекст c, якщо потрібно */);
                 }
                break;
            }
            case MG_EV_OPEN:
                // ESP_LOGD(TAG, "Connection opened"); // Занадто багато логів
                break;
            case MG_EV_CLOSE:
                // ESP_LOGD(TAG, "Connection closed"); // Занадто багато логів
                 if (c->is_websocket) {
                     websocket_on_close(/* передати контекст c */);
                 }
                break;
            case MG_EV_WS_OPEN:
                ESP_LOGI(TAG, "WebSocket connection opened (conn_id: %lu)", c->id);
                 websocket_on_open(/* передати контекст c */);
                break;
            case MG_EV_WS_MSG: {
                struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;
                ESP_LOGD(TAG, "WebSocket message received (conn_id: %lu, len: %d)", c->id, wm->data.len);
                 websocket_on_message((const char*)wm->data.ptr, wm->data.len /*, передати контекст c */);
                 // Очищуємо буфер повідомлення Mongoose
                 mg_iobuf_delete(&c->recv, wm->data.len);
                break;
            }
             case MG_EV_WS_CTL: // Control frames (e.g., close, ping, pong)
                 // ESP_LOGD(TAG, "WebSocket control frame");
                 // Mongoose автоматично обробляє PING/PONG/CLOSE
                 break;

            case MG_EV_HTTP_MSG: {
                 struct mg_http_message *hm = (struct mg_http_message *) ev_data;
                 ESP_LOGD(TAG, "HTTP request: %.*s %.*s", (int)hm->method.len, hm->method.ptr, (int)hm->uri.len, hm->uri.ptr);

                 // 1. Перевірка на WebSocket Upgrade
                 if (mg_http_match_uri(hm, "/ws") && hm->method.len == 3 && memcmp(hm->method.ptr, "GET", 3) == 0) {
                      ESP_LOGI(TAG, "WebSocket upgrade request for /ws");
                      // Оновлюємо з'єднання до WebSocket
                      mg_ws_upgrade(c, hm, NULL); // Mongoose обробить подальші WS події
                      // fn_data не використовуємо тут
                      return; // Більше нічого не робимо з цим запитом
                 }

                 // 2. Перевірка на RPC запит
                 if (mg_http_match_uri(hm, RPC_ENDPOINT) && hm->method.len == 4 && memcmp(hm->method.ptr, "POST", 4) == 0) {
                     ESP_LOGI(TAG, "RPC request received");
                     char *response_json_str = rpc_api_handle_request_str(hm->body.ptr, hm->body.len);
                     if (response_json_str) {
                         mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s", response_json_str);
                         free(response_json_str); // Важливо звільнити пам'ять!
                     } else {
                         // rpc_api_handle_request_str мав обробити помилку,
                         // але якщо він повернув NULL без відправки відповіді:
                         mg_http_reply(c, 500, "", "{\"error\":\"RPC processing error\"}");
                         ESP_LOGE(TAG, "RPC handler returned NULL response string");
                     }
                     // Позначаємо, що запит оброблено
                      c->is_resp = 1; // Mongoose v7+ (раніше c->flags |= MG_F_SEND_AND_CLOSE);
                     return;
                 }

                 // 3. Обробка запиту на схему
                 if (mg_http_match_uri(hm, SCHEMA_ENDPOINT) && hm->method.len == 3 && memcmp(hm->method.ptr, "GET", 3) == 0) {
                     ESP_LOGI(TAG, "Schema request received");
                     schema_handler(c, ev, ev_data, fn_data);
                     c->is_resp = 1;
                     return;
                 }

                 // 4. Обробка вбудованих статичних файлів
                 if (!USE_LITTLEFS) {
                     // Конвертуємо URI в C-рядок для порівняння
                     char uri[100] = {0};
                     mg_snprintf(uri, sizeof(uri), "%.*s", (int)hm->uri.len, hm->uri.ptr);
                     
                     // Спробуємо обробити вбудований файл
                     if (serve_embedded_file(c, uri)) {
                         return; // Файл успішно відправлено
                     }
                     
                     // Якщо файл не знайдено - відправляємо 404
                     mg_http_reply(c, 404, "", "File not found (embedded): %s\n", uri);
                     c->is_resp = 1;
                     return;
                 }

                 // 5. Обробка статичних файлів з LittleFS
                  if (USE_LITTLEFS && !s_full_doc_root.empty()) {
                     struct mg_http_serve_opts opts = {};
                     opts.root_dir = s_full_doc_root.c_str();
                     opts.ssi_pattern = NULL; // Вимикаємо Server-Side Includes
                     // Можна додати кешування: opts.extra_headers = "Cache-Control: max-age=300\r\n";
                     mg_http_serve_dir(c, hm, &opts);
                     ESP_LOGD(TAG, "Attempted to serve static file: %.*s", (int)hm->uri.len, hm->uri.ptr);
                 } else {
                      ESP_LOGW(TAG, "Document root не встановлено, статичні файли не обслуговуються.");
                      mg_http_reply(c, 404, "", "Not Found (Static files disabled)\n");
                 }
                  // Позначаємо, що запит оброблено (або не знайдено) функцією serve_dir
                  c->is_resp = 1;
                 break; // MG_EV_HTTP_MSG
            }
            default:
                // Ігноруємо інші події
                break;
        }
        (void) fn_data; // Уникаємо попередження про невикористаний параметр
    }

    // --- Задача для Mongoose Poll ---
    static void server_task(void *pvParameters) {
        struct mg_mgr *mgr = (struct mg_mgr *)pvParameters;
        ESP_LOGI(TAG, "Mongoose task started.");
        while (s_is_running) {
            mg_mgr_poll(mgr, MONGOOSE_POLL_MS);
            // Невелике затримання для передачі управління іншим задачам
            vTaskDelay(pdMS_TO_TICKS(5));
        }
        ESP_LOGI(TAG, "Mongoose task stopping.");
        s_server_task_handle = nullptr; // Очищаємо хендл після виходу
        vTaskDelete(NULL); // Видаляємо саму себе
    }

} // Анонімний namespace


// --- Реалізація публічних методів ---

esp_err_t server_mongoose_init() {
    if (s_is_initialized) {
        ESP_LOGW(TAG, "Mongoose server вже ініціалізовано.");
        return ESP_OK;
    }
    ESP_LOGI(TAG, "Ініціалізація Mongoose server...");
    mg_mgr_init(&s_mongoose_mgr); // Ініціалізуємо менеджер подій
    
    if (USE_LITTLEFS) {
        s_full_doc_root = std::string(WEBUI_FS_ROOT) + WEBUI_DOC_ROOT;
        ESP_LOGI(TAG, "Document root: '%s'", s_full_doc_root.c_str());
    } else {
        ESP_LOGI(TAG, "Використовуються вбудовані файли інтерфейсу");
    }
    
    s_is_initialized = true;
    s_is_running = false;
    ESP_LOGI(TAG, "Mongoose manager ініціалізовано.");
    return ESP_OK;
}

esp_err_t server_mongoose_start() {
     if (!s_is_initialized) {
        ESP_LOGE(TAG, "Сервер не ініціалізовано перед стартом!");
        return ESP_FAIL;
     }
     if (s_is_running) {
         ESP_LOGW(TAG, "Сервер вже запущено.");
         return ESP_OK;
     }

    ESP_LOGI(TAG, "Старт Mongoose server на %s", MONGOOSE_LISTEN_ADDR);

    // Створюємо слухаюче з'єднання
    struct mg_connection *listener = mg_http_listen(&s_mongoose_mgr, MONGOOSE_LISTEN_ADDR, main_event_handler, NULL);
    if (listener == NULL) {
        ESP_LOGE(TAG, "Не вдалося створити слухаюче з'єднання Mongoose!");
        mg_mgr_free(&s_mongoose_mgr); // Очистка менеджера
        s_is_initialized = false;
        return ESP_FAIL;
    }

    // Створюємо задачу для обробки подій
    BaseType_t task_created = xTaskCreate(server_task,
                                           "mongoose_task",
                                           MONGOOSE_TASK_STACK_SIZE,
                                           &s_mongoose_mgr,
                                           MONGOOSE_TASK_PRIORITY,
                                           &s_server_task_handle);

    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Не вдалося створити задачу Mongoose!");
        mg_mgr_free(&s_mongoose_mgr); // Очистка менеджера
        s_is_initialized = false;
        return ESP_FAIL;
    }

    s_is_running = true;
    ESP_LOGI(TAG, "Mongoose сервер успішно запущено.");
    return ESP_OK;
}

esp_err_t server_mongoose_stop() {
     if (!s_is_initialized || !s_is_running) {
        ESP_LOGW(TAG, "Сервер не запущено або не ініціалізовано.");
        return ESP_OK;
     }

    ESP_LOGI(TAG, "Зупинка Mongoose server...");
    s_is_running = false; // Сигналізуємо задачі про зупинку

    // Чекаємо недовго, щоб задача могла завершитись штатно
    // Або можна використати інший механізм сигналізації та очікування
    vTaskDelay(pdMS_TO_TICKS(MONGOOSE_POLL_MS * 2));

    // Якщо задача ще існує, видаляємо її примусово
    if (s_server_task_handle != nullptr) {
        ESP_LOGW(TAG,"Примусова зупинка задачі Mongoose...");
        vTaskDelete(s_server_task_handle);
        s_server_task_handle = nullptr;
    }

    mg_mgr_free(&s_mongoose_mgr); // Звільняємо ресурси менеджера
    s_is_initialized = false;
    ESP_LOGI(TAG, "Mongoose сервер зупинено.");
    return ESP_OK;
}