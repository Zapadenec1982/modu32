#ifndef SERVER_MONGOOSE_H
#define SERVER_MONGOOSE_H

#include "esp_err.h"

/**
 * @brief Ініціалізує внутрішні структури сервера Mongoose.
 *
 * Має викликатись перед server_mongoose_start().
 *
 * @return esp_err_t ESP_OK при успіху.
 */
esp_err_t server_mongoose_init();

/**
 * @brief Запускає слухаючий сокет Mongoose та задачу обробки подій.
 *
 * Має викликатись після server_mongoose_init().
 *
 * @return esp_err_t ESP_OK при успіху, інакше код помилки.
 */
esp_err_t server_mongoose_start();

/**
 * @brief Зупиняє задачу обробки подій та звільняє ресурси Mongoose.
 *
 * @return esp_err_t ESP_OK при успіху.
 */
esp_err_t server_mongoose_stop();

#endif // SERVER_MONGOOSE_H