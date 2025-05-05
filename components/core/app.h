#ifndef CORE_APP_H
#define CORE_APP_H

#include "esp_err.h" // Потрібен для типу esp_err_t

namespace CoreApp {
    /**
     * @brief Ініціалізує всі основні компоненти ядра системи.
     *
     * Має викликатись один раз з app_main при старті програми.
     *
     * @return esp_err_t ESP_OK при успішній ініціалізації всіх компонентів,
     * інакше код помилки першого компонента, що не зміг
     * ініціалізуватися.
     */
    esp_err_t init();
}

#endif // CORE_APP_H