#ifndef RPC_API_H
#define RPC_API_H

#include "esp_err.h"
#include "cJSON.h" // Потрібен для типів у сигнатурах функцій

/**
 * @brief Тип функції-обробника для RPC методу.
 *
 * @param params Вказівник на cJSON об'єкт або масив з параметрами запиту (може бути NULL).
 * НЕ ПОТРІБНО видаляти цей об'єкт всередині обробника.
 * @return cJSON* Вказівник на cJSON об'єкт з результатом. Викликаючий код (rpc_api_handle_request_str)
 * стає власником цього об'єкта і має його видалити (через cJSON_Delete).
 * Поверніть NULL, якщо сталася внутрішня помилка обробки (це призведе
 * до формування JSON-RPC помилки -32603 Internal error).
 */
typedef cJSON* (*rpc_handler_func_t)(const cJSON* params);

/**
 * @brief Ініціалізує систему обробки RPC API.
 *
 * Має викликатись один раз при старті web_interface.
 *
 * @return esp_err_t ESP_OK при успіху.
 */
esp_err_t rpc_api_init();

/**
 * @brief Реєструє функцію-обробник для конкретного RPC методу.
 *
 * @param method_name Назва RPC методу (наприклад, "System.GetInfo", "Relay.SetState").
 * @param handler Вказівник на функцію типу rpc_handler_func_t.
 * @return esp_err_t ESP_OK при успіху, ESP_ERR_INVALID_ARG якщо параметри невірні.
 */
esp_err_t rpc_api_register_handler(const char* method_name, rpc_handler_func_t handler);

/**
 * @brief Обробляє вхідний рядок JSON-RPC запиту.
 *
 * Парсить запит, знаходить відповідний обробник, викликає його,
 * формує рядок JSON-RPC відповіді.
 *
 * @param request_body Вказівник на буфер з тілом запиту.
 * @param body_len Довжина тіла запиту.
 * @return char* Вказівник на рядок з JSON-RPC відповіддю. Викликаюча сторона
 * (server_mongoose.cpp) ВІДПОВІДАЄ за звільнення пам'яті
 * цього рядка за допомогою free()! Повертає NULL у випадку помилки
 * форматування відповіді або якщо запит є notification.
 */
char* rpc_api_handle_request_str(const char* request_body, size_t body_len);

#endif // RPC_API_H