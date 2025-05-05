#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t web_interface_init(void);
esp_err_t web_interface_start(void);
esp_err_t web_interface_stop(void);

// Додана функція schema_handler для обробки запитів схеми
struct mg_connection;
void schema_handler(struct mg_connection *c, int ev, void *ev_data, void *fn_data);

#ifdef __cplusplus
}
#endif

#endif // WEB_INTERFACE_H