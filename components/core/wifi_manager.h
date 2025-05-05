#ifndef CORE_WIFI_MANAGER_H
#define CORE_WIFI_MANAGER_H

#include "esp_err.h"
#include <string>
#include <vector>
#include <map>

// Структура для зберігання інформації про WiFi мережу
struct WiFiNetwork {
    std::string ssid;
    int8_t rssi;
    uint8_t auth_mode;
    // Можна додати більше полів за потребою
};

// Структура для зберігання статусу WiFi
struct WiFiStatus {
    bool connected;
    std::string ip_address;
    std::string ssid;
    int8_t rssi;
    std::string mac_address;
};

class WiFiManager {
public:
    // Ініціалізація WiFi системи
    static esp_err_t init();
    
    // Перевірка стану підключення
    static bool isConnected();
    
    // Запуск режиму налаштування (AP + веб-портал)
    static void startProvisioning();
    
    // Новий метод: Явне підключення до WiFi
    static esp_err_t connect();
    
    // Новий метод: Явне відключення від WiFi
    static esp_err_t disconnect();
    
    // Новий метод: Сканування доступних мереж
    static std::vector<WiFiNetwork> scanNetworks(uint32_t timeout_ms = 10000);
    
    // Новий метод: Отримання детальної інформації про підключення
    static WiFiStatus getStatus();
    
    // Новий метод: Встановлення облікових даних без перезавантаження
    static esp_err_t setCredentials(const std::string& ssid, const std::string& password, bool connect_now = true);
};

#endif // CORE_WIFI_MANAGER_H