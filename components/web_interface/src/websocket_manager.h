#ifndef WEBSOCKET_MANAGER_H
#define WEBSOCKET_MANAGER_H

#include "esp_err.h"
#include "cJSON.h"
#include <stddef.h>
#include "mongoose.h" // <<<--- Додано для struct mg_connection

esp_err_t websocket_manager_init();

// Функції, що викликаються з обробника подій сервера Mongoose
// Додано параметр 'struct mg_connection* c'
void websocket_on_open(struct mg_connection *c);
void websocket_on_close(struct mg_connection *c);
void websocket_on_message(struct mg_connection *c, const char* message, size_t len);
void websocket_on_error(struct mg_connection *c); // 'c' може бути NULL, якщо помилка не пов'язана з клієнтом

// Функції для надсилання повідомлень
void websocket_broadcast(const char* message, size_t len);
void websocket_broadcast_json(const cJSON* json); // Змінено на const*, менеджер не видаляє json

// Функція для надсилання конкретному клієнту (опціонально)
// void websocket_send_json(struct mg_connection *c, const cJSON* json);

#endif // WEBSOCKET_MANAGER_H