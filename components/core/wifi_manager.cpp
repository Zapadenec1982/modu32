#include "wifi_manager.h"
#include "config.h" // Потрібно для доступу до ConfigLoader
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_system.h" // Для esp_restart()
#include "esp_random.h" // Для генерації випадкових чисел
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "lwip/err.h" // Для типів помилок LwIP
#include "lwip/sys.h"
#include "lwip/dns.h" // Для DNS резолвера
#include <string.h> // Для strncpy, strlen
#include <string>   // Для std::string
#include <vector>   // Для списків
#include <sstream>  // Для string streams
#include <algorithm> // Для std::transform
#include <cctype>   // Для tolower

static const char* TAG = "WiFiManager";

// --- Налаштування ---
#define WIFI_PROVISIONING_TIMEOUT_SEC 30 // Час очікування підключення STA перед переходом в AP
#define WIFI_MAX_STA_CONN_RETRIES 5      // Кількість спроб перепідключення STA
#define WIFI_PROV_AP_CHANNEL 1           // Канал точки доступу
#define WIFI_PROV_AP_MAX_CONN 4          // Макс. клієнтів точки доступу
#define WIFI_SCAN_MAX_NETWORKS 20        // Максимально мереж при скануванні

// --- Глобальні змінні компонента (в анонімному неймспейсі) ---
namespace {
    static bool wifi_connected = false;
    static int s_retry_num = 0;
    static bool provisioning_active = false;

    // Генерація унікального ідентифікатора для AP SSID
    static std::string generate_ap_ssid() {
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        char suffix[8];
        snprintf(suffix, sizeof(suffix), "%02X%02X%02X", mac[3], mac[4], mac[5]);
        return std::string("ModuChill-") + suffix;
    }

    // Статичне визначення SSID (з унікальним суфіксом) і пароля AP
    static const std::string WIFI_PROV_AP_SSID = generate_ap_ssid();
    static const std::string WIFI_PROV_AP_PASS = "setup123"; // Стандартний пароль, може бути змінений

    // Event group для сигналізації про стан підключення
    static EventGroupHandle_t s_wifi_event_group = nullptr;
    const static int WIFI_CONNECTED_BIT = BIT0;
    const static int WIFI_FAIL_BIT = BIT1;
    const static int WIFI_SCAN_DONE_BIT = BIT2;

    // Хендл веб-сервера для налаштування
    static httpd_handle_t s_prov_server_handle = nullptr;
    // Хендли мережевих інтерфейсів
    static esp_netif_t *s_sta_netif = nullptr;
    static esp_netif_t *s_ap_netif = nullptr;

    // Результати сканування
    static std::vector<WiFiNetwork> scan_results;
    static bool scan_in_progress = false;

    // URL декодування (для веб-інтерфейсу)
    std::string url_decode(const std::string& encoded) {
        std::string decoded;
        for (size_t i = 0; i < encoded.length(); ++i) {
            if (encoded[i] == '%' && i + 2 < encoded.length()) {
                int value;
                std::stringstream ss;
                ss << std::hex << encoded.substr(i+1, 2);
                ss >> value;
                decoded += static_cast<char>(value);
                i += 2;
            } else if (encoded[i] == '+') {
                decoded += ' ';
            } else {
                decoded += encoded[i];
            }
        }
        return decoded;
    }

    // Парсинг параметрів з URL-кодованих даних
    std::map<std::string, std::string> parse_url_encoded(const std::string& data) {
        std::map<std::string, std::string> result;
        std::string::size_type start = 0;
        std::string::size_type end;
        
        while ((end = data.find('&', start)) != std::string::npos) {
            std::string keyval = data.substr(start, end - start);
            std::string::size_type mid = keyval.find('=');
            if (mid != std::string::npos) {
                std::string key = keyval.substr(0, mid);
                std::string val = keyval.substr(mid + 1);
                result[key] = url_decode(val);
            }
            start = end + 1;
        }
        
        // Останній параметр
        std::string keyval = data.substr(start);
        std::string::size_type mid = keyval.find('=');
        if (mid != std::string::npos) {
            std::string key = keyval.substr(0, mid);
            std::string val = keyval.substr(mid + 1);
            result[key] = url_decode(val);
        }
        
        return result;
    }

    // Обробник подій Wi-Fi та IP
    static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                   int32_t event_id, void* event_data) {
        if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
            ESP_LOGI(TAG, "WIFI_EVENT_STA_START: Спроба підключення...");
            esp_wifi_connect();
        } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
            wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
            wifi_connected = false;
            
            // Логування причини відключення
            const char* reason_str = "невідома";
            switch(event->reason) {
                case WIFI_REASON_UNSPECIFIED: reason_str = "UNSPECIFIED"; break;
                case WIFI_REASON_AUTH_EXPIRE: reason_str = "AUTH_EXPIRE"; break;
                case WIFI_REASON_AUTH_LEAVE: reason_str = "AUTH_LEAVE"; break;
                case WIFI_REASON_ASSOC_EXPIRE: reason_str = "ASSOC_EXPIRE"; break;
                case WIFI_REASON_ASSOC_TOOMANY: reason_str = "ASSOC_TOOMANY"; break;
                case WIFI_REASON_NOT_AUTHED: reason_str = "NOT_AUTHED"; break;
                case WIFI_REASON_NOT_ASSOCED: reason_str = "NOT_ASSOCED"; break;
                case WIFI_REASON_ASSOC_LEAVE: reason_str = "ASSOC_LEAVE"; break;
                case WIFI_REASON_ASSOC_NOT_AUTHED: reason_str = "ASSOC_NOT_AUTHED"; break;
                case WIFI_REASON_DISASSOC_PWRCAP_BAD: reason_str = "DISASSOC_PWRCAP_BAD"; break;
                case WIFI_REASON_DISASSOC_SUPCHAN_BAD: reason_str = "DISASSOC_SUPCHAN_BAD"; break;
                case WIFI_REASON_BSS_TRANSITION_DISASSOC: reason_str = "BSS_TRANSITION_DISASSOC"; break;
                case WIFI_REASON_IE_INVALID: reason_str = "IE_INVALID"; break;
                case WIFI_REASON_MIC_FAILURE: reason_str = "MIC_FAILURE"; break;
                case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT: reason_str = "4WAY_HANDSHAKE_TIMEOUT"; break;
                case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT: reason_str = "GROUP_KEY_UPDATE_TIMEOUT"; break;
                case WIFI_REASON_IE_IN_4WAY_DIFFERS: reason_str = "IE_IN_4WAY_DIFFERS"; break;
                case WIFI_REASON_GROUP_CIPHER_INVALID: reason_str = "GROUP_CIPHER_INVALID"; break;
                case WIFI_REASON_PAIRWISE_CIPHER_INVALID: reason_str = "PAIRWISE_CIPHER_INVALID"; break;
                case WIFI_REASON_AKMP_INVALID: reason_str = "AKMP_INVALID"; break;
                case WIFI_REASON_UNSUPP_RSN_IE_VERSION: reason_str = "UNSUPP_RSN_IE_VERSION"; break;
                case WIFI_REASON_INVALID_RSN_IE_CAP: reason_str = "INVALID_RSN_IE_CAP"; break;
                case WIFI_REASON_802_1X_AUTH_FAILED: reason_str = "802_1X_AUTH_FAILED"; break;
                case WIFI_REASON_CIPHER_SUITE_REJECTED: reason_str = "CIPHER_SUITE_REJECTED"; break;
                case WIFI_REASON_BEACON_TIMEOUT: reason_str = "BEACON_TIMEOUT"; break;
                case WIFI_REASON_NO_AP_FOUND: reason_str = "NO_AP_FOUND"; break;
                case WIFI_REASON_AUTH_FAIL: reason_str = "AUTH_FAIL"; break;
                case WIFI_REASON_ASSOC_FAIL: reason_str = "ASSOC_FAIL"; break;
                case WIFI_REASON_HANDSHAKE_TIMEOUT: reason_str = "HANDSHAKE_TIMEOUT"; break;
                case WIFI_REASON_CONNECTION_FAIL: reason_str = "CONNECTION_FAIL"; break;
            }
            
            ESP_LOGW(TAG, "WIFI_EVENT_STA_DISCONNECTED: SSID='%s', BSSID=" MACSTR ", причина=%d (%s)",
                    event->ssid, MAC2STR(event->bssid), event->reason, reason_str);
            
            if (s_retry_num < WIFI_MAX_STA_CONN_RETRIES) {
                s_retry_num++;
                ESP_LOGI(TAG, "Повторна спроба підключення (%d/%d)...", s_retry_num, WIFI_MAX_STA_CONN_RETRIES);
                esp_wifi_connect();
            } else {
                ESP_LOGW(TAG, "Не вдалося підключитися після %d спроб.", WIFI_MAX_STA_CONN_RETRIES);
                if (s_wifi_event_group) {
                    xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                }
            }
        } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP: Отримано IP:" IPSTR, IP2STR(&event->ip_info.ip));
            s_retry_num = 0;
            wifi_connected = true;
            if (s_wifi_event_group) {
                xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            }
        } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
            wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
            ESP_LOGI(TAG, "Клієнт " MACSTR " підключився до AP, AID=%d", MAC2STR(event->mac), event->aid);
        } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
            wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
            ESP_LOGI(TAG, "Клієнт " MACSTR " відключився від AP, AID=%d", MAC2STR(event->mac), event->aid);
        } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
            if (!scan_in_progress) return; // Ігноруємо, якщо ми не запускали сканування
            
            ESP_LOGI(TAG, "WIFI_EVENT_SCAN_DONE: Сканування завершено");
            
            // Отримуємо результати сканування
            uint16_t ap_count = 0;
            esp_wifi_scan_get_ap_num(&ap_count);
            
            if (ap_count > 0) {
                uint16_t max_aps = std::min(ap_count, (uint16_t)WIFI_SCAN_MAX_NETWORKS);
                wifi_ap_record_t* ap_records = new wifi_ap_record_t[max_aps];
                
                if (ap_records != nullptr) {
                    esp_err_t ret = esp_wifi_scan_get_ap_records(&max_aps, ap_records);
                    
                    if (ret == ESP_OK) {
                        scan_results.clear();
                        for (int i = 0; i < max_aps; i++) {
                            WiFiNetwork network;
                            network.ssid = (char*)ap_records[i].ssid;
                            network.rssi = ap_records[i].rssi;
                            network.auth_mode = ap_records[i].authmode;
                            scan_results.push_back(network);
                        }
                        // Сортуємо за силою сигналу (RSSI) - від найсильнішого до найслабшого
                        std::sort(scan_results.begin(), scan_results.end(), 
                                 [](const WiFiNetwork& a, const WiFiNetwork& b) {
                                     return a.rssi > b.rssi;
                                 });
                    } else {
                        ESP_LOGE(TAG, "Помилка отримання результатів сканування: %s", esp_err_to_name(ret));
                    }
                    
                    delete[] ap_records;
                } else {
                    ESP_LOGE(TAG, "Помилка виділення пам'яті для результатів сканування");
                }
            }
            
            // Повідомляємо про завершення сканування
            scan_in_progress = false;
            if (s_wifi_event_group) {
                xEventGroupSetBits(s_wifi_event_group, WIFI_SCAN_DONE_BIT);
            }
        }
    }

    // --- Логіка веб-сервера для налаштування ---

    // HTML Шаблони для веб-інтерфейсу
    const char* provisioning_html_template = R"rawliteral(
<!DOCTYPE html>
<html lang="uk">
<head>
    <meta charset="UTF-8">
    <title>ModuChill WiFi Setup</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; margin: 0; padding: 20px; color: #333; max-width: 600px; margin: 0 auto; }
        h1 { color: #0066cc; }
        .container { background: #f5f5f5; padding: 20px; border-radius: 5px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }
        label { display: block; margin-top: 10px; font-weight: bold; }
        input[type="text"], input[type="password"], select { width: 100%%; padding: 8px; margin: 5px 0 15px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }
        button { background: #0066cc; color: white; border: none; padding: 10px 15px; border-radius: 4px; cursor: pointer; }
        button:hover { background: #0055bb; }
        .network-list { margin-bottom: 15px; }
        .network { padding: 8px; margin: 5px 0; border: 1px solid #ddd; border-radius: 4px; cursor: pointer; }
        .network:hover { background: #e9e9e9; }
        .signal { display: inline-block; width: 20px; }
        .loading { display: none; text-align: center; padding: 10px; }
        .footer { margin-top: 20px; text-align: center; font-size: 0.8em; color: #777; }
    </style>
</head>
<body>
    <div class="container">
        <h1>ModuChill WiFi Setup</h1>
        <p>Налаштування підключення до Wi-Fi мережі</p>
        
        <div id="wifi-form">
            <form action="/save" method="post" id="config-form">
                <label for="ssid">Доступні мережі:</label>
                <div class="network-list" id="networks">
                    %NETWORK_LIST%
                </div>
                
                <button type="button" id="scan-btn">Сканувати знову</button>
                
                <div class="loading" id="loading">Сканування...</div>
                
                <label for="ssid">SSID (назва мережі):</label>
                <input type="text" id="ssid" name="ssid" required>
                
                <label for="pass">Пароль:</label>
                <input type="password" id="pass" name="pass">
                
                <button type="submit">Зберегти і перезавантажити</button>
            </form>
        </div>
    </div>
    
    <div class="footer">
        ModuChill &copy; 2025
    </div>
    
    <script>
        // JavaScript для вибору мережі зі списку
        document.addEventListener('DOMContentLoaded', function() {
            const networks = document.querySelectorAll('.network');
            const ssidInput = document.getElementById('ssid');
            
            networks.forEach(network => {
                network.addEventListener('click', function() {
                    ssidInput.value = this.getAttribute('data-ssid');
                });
            });
            
            // Кнопка сканування
            const scanBtn = document.getElementById('scan-btn');
            const loading = document.getElementById('loading');
            
            scanBtn.addEventListener('click', function() {
                loading.style.display = 'block';
                fetch('/scan')
                    .then(response => response.json())
                    .then(data => {
                        if (data.success) {
                            // Перезавантажуємо сторінку, щоб отримати новий список
                            window.location.reload();
                        } else {
                            alert('Помилка сканування: ' + data.message);
                            loading.style.display = 'none';
                        }
                    })
                    .catch(error => {
                        alert('Помилка з\'єднання');
                        loading.style.display = 'none';
                    });
            });
        });
    </script>
</body>
</html>
)rawliteral";

    const char* success_html = R"rawliteral(
<!DOCTYPE html>
<html lang="uk">
<head>
    <meta charset="UTF-8">
    <title>ModuChill WiFi Setup</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; margin: 0; padding: 20px; color: #333; max-width: 600px; margin: 0 auto; }
        h1 { color: #0066cc; }
        .container { background: #f5f5f5; padding: 20px; border-radius: 5px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }
        .success { color: #4CAF50; }
        .progress { margin-top: 20px; height: 10px; background-color: #f3f3f3; border-radius: 5px; overflow: hidden; }
        .progress-bar { height: 100%; width: 0; background-color: #4CAF50; transition: width 10s ease; }
    </style>
</head>
<body>
    <div class="container">
        <h1 class="success">Налаштування збережено!</h1>
        <p>Нові налаштування Wi-Fi успішно збережені. Пристрій перезавантажиться через кілька секунд і спробує підключитися до вказаної мережі.</p>
        
        <div class="progress"><div class="progress-bar" id="progressBar"></div></div>
    </div>
    
    <script>
        document.addEventListener('DOMContentLoaded', function() {
            document.getElementById('progressBar').style.width = '100%';
        });
    </script>
</body>
</html>
)rawliteral";

    // Генерація HTML кодy списку мереж
    std::string generate_network_list_html() {
        std::stringstream html;
        
        if (scan_results.empty()) {
            html << "<p>Мережі не знайдено. Натисніть 'Сканувати знову'.</p>";
            return html.str();
        }
        
        for (const auto& network : scan_results) {
            // Визначення рівня сигналу (простий індикатор)
            std::string signal_icon;
            if (network.rssi > -50) {
                signal_icon = "▮▮▮▮"; // Дуже сильний сигнал
            } else if (network.rssi > -65) {
                signal_icon = "▮▮▮";   // Сильний сигнал
            } else if (network.rssi > -75) {
                signal_icon = "▮▮";    // Середній сигнал
            } else {
                signal_icon = "▮";     // Слабкий сигнал
            }
            
            // Тип шифрування
            std::string security;
            switch (network.auth_mode) {
                case WIFI_AUTH_OPEN: security = ""; break;
                case WIFI_AUTH_WEP: security = "WEP"; break;
                case WIFI_AUTH_WPA_PSK: security = "WPA"; break;
                case WIFI_AUTH_WPA2_PSK: security = "WPA2"; break;
                case WIFI_AUTH_WPA_WPA2_PSK: security = "WPA/WPA2"; break;
                case WIFI_AUTH_WPA2_ENTERPRISE: security = "WPA2-Enterprise"; break;
                case WIFI_AUTH_WPA3_PSK: security = "WPA3"; break;
                case WIFI_AUTH_WPA2_WPA3_PSK: security = "WPA2/WPA3"; break;
                default: security = "?"; break;
            }
            
            html << "<div class='network' data-ssid='" << network.ssid << "'>";
            html << "<span class='signal'>" << signal_icon << "</span> ";
            html << network.ssid;
            if (!security.empty()) {
                html << " <small>(" << security << ")</small>";
            }
            html << "</div>";
        }
        
        return html.str();
    }

    // Підготовка HTML сторінки з актуальними даними
    std::string prepare_provisioning_html() {
        std::string html = provisioning_html_template;
        std::string network_list = generate_network_list_html();
        
        // Заміна плейсхолдера списком мереж
        size_t pos = html.find("%NETWORK_LIST%");
        if (pos != std::string::npos) {
            html.replace(pos, 14, network_list);
        }
        
        return html;
    }

    // Обробник GET запиту на '/'
    esp_err_t http_get_handler(httpd_req_t *req) {
        ESP_LOGI(TAG, "Віддаю сторінку налаштування...");
        
        // Перевіряємо наявність результатів сканування
        if (scan_results.empty()) {
            // Запускаємо сканування, якщо ще не було результатів
            WiFiManager::scanNetworks();
        }
        
        std::string html = prepare_provisioning_html();
        
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, html.c_str(), html.length());
        return ESP_OK;
    }

    // Обробник GET запиту для сканування
    esp_err_t http_get_scan_handler(httpd_req_t *req) {
        ESP_LOGI(TAG, "Запит на сканування Wi-Fi мереж...");
        
        std::vector<WiFiNetwork> networks = WiFiManager::scanNetworks();
        
        // Формуємо JSON відповідь
        std::string json = "{\"success\":true,\"count\":" + std::to_string(networks.size()) + "}";
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json.c_str(), json.length());
        return ESP_OK;
    }

    // Обробник POST запиту на '/save'
    esp_err_t http_post_save_handler(httpd_req_t *req) {
        ESP_LOGI(TAG, "Отримано POST запит на /save (len: %d)", req->content_len);
        
        // Обмеження розміру запиту
        const size_t max_len = 1024;
        if (req->content_len >= max_len) {
            ESP_LOGE(TAG, "POST дані занадто великі (%d B)", req->content_len);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Data too long");
            return ESP_FAIL;
        }
        
        // Буфер для POST даних
        char *buf = new char[max_len];
        if (!buf) {
            ESP_LOGE(TAG, "Не вдалося виділити пам'ять для POST даних");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
            return ESP_FAIL;
        }
        
        // Читаємо тіло запиту
        int ret = httpd_req_recv(req, buf, req->content_len);
        if (ret <= 0) {
            delete[] buf;
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                httpd_resp_send_408(req);
            }
            ESP_LOGE(TAG, "Помилка читання POST даних (%d)", ret);
            return ESP_FAIL;
        }
        
        // Null-terminate і логування для відлагодження
        buf[ret] = '\0';
        ESP_LOGD(TAG, "Отримано POST дані: %s", buf);
        
        // Парсимо дані форми з використанням поліпшеного парсеру
        std::map<std::string, std::string> params = parse_url_encoded(buf);
        
        // Більше не потрібен
        delete[] buf;
        
        std::string ssid = params["ssid"];
        std::string pass = params["pass"];
        
        ESP_LOGI(TAG, "Парсинг: SSID='%s', Pass='%s' (довжина: %d)", 
                ssid.c_str(), 
                pass.empty() ? "" : "********", 
                (int)pass.length());
        
        // Валідація (мінімальна)
        if (ssid.empty()) {
            ESP_LOGE(TAG, "SSID не може бути порожнім");
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID cannot be empty");
            return ESP_FAIL;
        }
        
        // Спроба зберегти налаштування
        esp_err_t result = WiFiManager::setCredentials(ssid, pass, false);
        
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "Помилка збереження конфігурації: %s", esp_err_to_name(result));
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save configuration");
            return ESP_FAIL;
        }
        
        ESP_LOGI(TAG, "Облікові дані Wi-Fi збережено успішно");

        // Відправляємо сторінку успіху
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, success_html, HTTPD_RESP_USE_STRLEN);

        // Невелике очікування, щоб браузер встиг отримати відповідь
        vTaskDelay(pdMS_TO_TICKS(2000));

        // Створюємо окрему задачу для перезавантаження, щоб відповідь відправилась
        xTaskCreate(
            [](void *) {
                vTaskDelay(pdMS_TO_TICKS(1000)); // Додаткова затримка перед перезавантаженням
                ESP_LOGI(TAG, "Перезавантаження системи...");
                esp_restart();
                vTaskDelete(NULL);
            },
            "restart_task", 2048, NULL, 5, NULL);

        return ESP_OK;
    }

    // Функція для старту веб-сервера налаштування
    esp_err_t start_provisioning_server() {
        if (s_prov_server_handle) {
            ESP_LOGW(TAG, "Сервер налаштування вже запущено.");
            return ESP_OK;
        }

        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.max_uri_handlers = 3; // Для '/', '/save' та '/scan'
        config.lru_purge_enable = true; // Дозволити очистку кешу
        config.core_id = 0; // Запускати на ядрі 0 (для мультиядерних ESP32)
        config.stack_size = 8192; // Збільшуємо розмір стеку

        ESP_LOGI(TAG, "Запуск сервера налаштування на порту %d", config.server_port);
        esp_err_t ret = httpd_start(&s_prov_server_handle, &config);
        if (ret == ESP_OK) {
            // Реєстрація обробників URI
            httpd_uri_t get_uri = {
                .uri       = "/",
                .method    = HTTP_GET,
                .handler   = http_get_handler,
                .user_ctx  = nullptr
            };
            httpd_register_uri_handler(s_prov_server_handle, &get_uri);

            httpd_uri_t scan_uri = {
                .uri       = "/scan",
                .method    = HTTP_GET,
                .handler   = http_get_scan_handler,
                .user_ctx  = nullptr
            };
            httpd_register_uri_handler(s_prov_server_handle, &scan_uri);

            httpd_uri_t post_uri = {
                .uri       = "/save",
                .method    = HTTP_POST,
                .handler   = http_post_save_handler,
                .user_ctx  = nullptr
            };
            httpd_register_uri_handler(s_prov_server_handle, &post_uri);
            
            ESP_LOGI(TAG, "Обробники URI зареєстровано.");
        } else {
            ESP_LOGE(TAG, "Помилка запуску сервера налаштування: %s", esp_err_to_name(ret));
            s_prov_server_handle = nullptr;
            return ret;
        }
        
        return ESP_OK;
    }

    // Функція для зупинки веб-сервера налаштування
    void stop_provisioning_server() {
        if (s_prov_server_handle) {
            ESP_LOGI(TAG, "Зупинка сервера налаштування...");
            httpd_stop(s_prov_server_handle);
            s_prov_server_handle = nullptr;
        }
    }

} // Анонімний namespace


// --- Реалізація публічних методів класу ---

esp_err_t WiFiManager::init() {
    ESP_LOGI(TAG, "Ініціалізація WiFiManager...");

    if (!s_wifi_event_group) {
        s_wifi_event_group = xEventGroupCreate();
        if (!s_wifi_event_group) {
            ESP_LOGE(TAG, "Не вдалося створити Event Group!");
            return ESP_FAIL;
        }
    } else {
        // Очищуємо біти з попередньої спроби, якщо була
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT | WIFI_SCAN_DONE_BIT);
    }

    // Мережевий інтерфейс створюється в CoreApp::init() до нас
    // Але Wi-Fi інтерфейси створюємо тут

    // Створюємо інтерфейс станції
    if (!s_sta_netif) {
        s_sta_netif = esp_netif_create_default_wifi_sta();
        if (!s_sta_netif) {
            ESP_LOGE(TAG, "Не вдалося створити STA netif!");
            return ESP_FAIL;
        }
    }

    // Створюємо інтерфейс точки доступу (може знадобитися)
    if (!s_ap_netif) {
        s_ap_netif = esp_netif_create_default_wifi_ap();
        if (!s_ap_netif) {
            ESP_LOGE(TAG, "Не вдалося створити AP netif!");
            return ESP_FAIL;
        }
    }

    // Ініціалізація Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Помилка esp_wifi_init: %s", esp_err_to_name(ret));
        return ret;
    }

    // Реєстрація обробників подій
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ret = esp_event_handler_instance_register(WIFI_EVENT,
                                             ESP_EVENT_ANY_ID,
                                             &wifi_event_handler,
                                             nullptr,
                                             &instance_any_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Помилка реєстрації обробника WIFI_EVENT: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = esp_event_handler_instance_register(IP_EVENT,
                                             IP_EVENT_STA_GOT_IP,
                                             &wifi_event_handler,
                                             nullptr,
                                             &instance_got_ip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Помилка реєстрації обробника IP_EVENT: %s", esp_err_to_name(ret));
        // Розреєструємо попередній обробник
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id);
        return ret;
    }

    // Завантаження конфігурації
    std::string ssid_str = ConfigLoader::get<std::string>("/wifi/ssid", "");
    std::string pass_str = ConfigLoader::get<std::string>("/wifi/pass", "");

    if (!ssid_str.empty()) {
        ESP_LOGI(TAG, "Знайдено конфігурацію Wi-Fi: SSID='%s'", ssid_str.c_str());
        
        // Спробуємо підключитися з наявними налаштуваннями
        return connect();
    } else {
        ESP_LOGW(TAG, "Не знайдено конфігурацію Wi-Fi. Запуск режиму налаштування.");
        startProvisioning();
        return ESP_OK; // Повертаємо OK, працюємо в режимі налаштування
    }
}

// Метод для підключення до WiFi
esp_err_t WiFiManager::connect() {
    // Завантаження конфігурації
    std::string ssid_str = ConfigLoader::get<std::string>("/wifi/ssid", "");
    std::string pass_str = ConfigLoader::get<std::string>("/wifi/pass", "");
    
    if (ssid_str.empty()) {
        ESP_LOGE(TAG, "Не знайдено SSID для підключення!");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Очищуємо біти з попередньої спроби
    if (s_wifi_event_group) {
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    }
    
    // Налаштування параметрів підключення
    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.sta.ssid, ssid_str.c_str(), sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, pass_str.c_str(), sizeof(wifi_config.sta.password) - 1);
    
    // Визначаємо режим автентифікації
    wifi_config.sta.threshold.authmode = pass_str.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    
    // Налаштування PMF (Protected Management Frames)
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;
    
    // Зупиняємо WiFi, якщо він вже запущений
    esp_wifi_stop();
    
    // Налаштовуємо та запускаємо WiFi в режимі станції
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start()); // Запускає WIFI_EVENT_STA_START
    
    ESP_LOGI(TAG, "Спроба підключення до Wi-Fi (до %d сек)...", WIFI_PROVISIONING_TIMEOUT_SEC);
    
    // Очікуємо на біт підключення або помилки
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                        pdFALSE, // Не очищати біти після виходу
                                        pdFALSE, // Чекати будь-який біт
                                        pdMS_TO_TICKS(WIFI_PROVISIONING_TIMEOUT_SEC * 1000));
    
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Підключено до точки доступу!");
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGW(TAG, "Не вдалося підключитися до точки доступу після %d спроб.", WIFI_MAX_STA_CONN_RETRIES);
        return ESP_FAIL;
    } else {
        ESP_LOGW(TAG, "Таймаут підключення до точки доступу (%d сек).", WIFI_PROVISIONING_TIMEOUT_SEC);
        return ESP_ERR_TIMEOUT;
    }
}

// Метод для відключення від WiFi
esp_err_t WiFiManager::disconnect() {
    if (!wifi_connected) {
        ESP_LOGW(TAG, "WiFi вже відключено.");
        return ESP_OK;
    }
    
    esp_err_t ret = esp_wifi_disconnect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Помилка відключення від WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Відключено від WiFi.");
    wifi_connected = false;
    
    return ESP_OK;
}

// Метод перевірки стану підключення
bool WiFiManager::isConnected() {
    // Перевіряємо і прапорець, і чи активний STA інтерфейс
    if (!s_sta_netif) return false;
    return wifi_connected && esp_netif_is_netif_up(s_sta_netif);
}

// Метод для запуску режиму налаштування (AP + веб-портал)
void WiFiManager::startProvisioning() {
    if (provisioning_active) {
        ESP_LOGW(TAG, "Режим налаштування вже активний");
        return;
    }
    
    ESP_LOGI(TAG, "Запуск режиму налаштування (AP + портал)...");
    stop_provisioning_server(); // Зупиняємо старий сервер, якщо він був

    // Зупиняємо поточний режим (STA, якщо є)
    esp_wifi_stop();

    // Налаштування точки доступу
    wifi_config_t wifi_config_ap = {};
    strncpy((char*)wifi_config_ap.ap.ssid, WIFI_PROV_AP_SSID.c_str(), sizeof(wifi_config_ap.ap.ssid) - 1);
    wifi_config_ap.ap.ssid_len = WIFI_PROV_AP_SSID.length();
    wifi_config_ap.ap.channel = WIFI_PROV_AP_CHANNEL;
    wifi_config_ap.ap.max_connection = WIFI_PROV_AP_MAX_CONN;
    
    if (WIFI_PROV_AP_PASS.empty()) {
        wifi_config_ap.ap.authmode = WIFI_AUTH_OPEN;
        memset(wifi_config_ap.ap.password, 0, sizeof(wifi_config_ap.ap.password));
    } else {
        wifi_config_ap.ap.authmode = WIFI_AUTH_WPA2_PSK;
        strncpy((char*)wifi_config_ap.ap.password, WIFI_PROV_AP_PASS.c_str(), sizeof(wifi_config_ap.ap.password) - 1);
    }

    // Запуск WiFi в режимі точки доступу
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config_ap));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Точку доступу запущено: SSID='%s', Канал=%d", WIFI_PROV_AP_SSID.c_str(), WIFI_PROV_AP_CHANNEL);

    // Сканування доступних мереж для показу в інтерфейсі
    scanNetworks();

    // Запуск веб-сервера налаштування
    esp_err_t ret = start_provisioning_server();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Не вдалося запустити веб-сервер налаштування!");
        // Повторна спроба через деякий час?
    }
    
    provisioning_active = true;
}

// Метод для сканування доступних WiFi мереж
std::vector<WiFiNetwork> WiFiManager::scanNetworks(uint32_t timeout_ms) {
    ESP_LOGI(TAG, "Запуск сканування WiFi мереж...");
    
    if (scan_in_progress) {
        ESP_LOGW(TAG, "Сканування вже виконується");
        return scan_results; // Повертаємо попередні результати
    }
    
    scan_in_progress = true;
    
    // Очищуємо біти сканування
    if (s_wifi_event_group) {
        xEventGroupClearBits(s_wifi_event_group, WIFI_SCAN_DONE_BIT);
    }
    
    // Збережемо поточний режим
    wifi_mode_t current_mode;
    esp_wifi_get_mode(&current_mode);
    
    // Встановлюємо режим, який підтримує сканування
    if (current_mode == WIFI_MODE_NULL) {
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_start();
    } else if (current_mode == WIFI_MODE_AP) {
        esp_wifi_set_mode(WIFI_MODE_APSTA);
    }
    
    // Параметри сканування
    wifi_scan_config_t scan_config = {
        .ssid = nullptr,
        .bssid = nullptr,
        .channel = 0,  // Всі канали
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = {
            .active = {
                .min = 100,
                .max = 300
            }
        }
    };
    
    // Запуск асинхронного сканування
    esp_err_t ret = esp_wifi_scan_start(&scan_config, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Помилка запуску сканування: %s", esp_err_to_name(ret));
        scan_in_progress = false;
        return scan_results; // Повертаємо попередні результати
    }
    
    // Очікуємо завершення сканування
    if (s_wifi_event_group) {
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                            WIFI_SCAN_DONE_BIT,
                                            pdFALSE,
                                            pdFALSE,
                                            pdMS_TO_TICKS(timeout_ms));
        
        if (!(bits & WIFI_SCAN_DONE_BIT)) {
            ESP_LOGW(TAG, "Таймаут сканування!");
            esp_wifi_scan_stop(); // Зупиняємо сканування
            scan_in_progress = false;
        }
    } else {
        // Просто чекаємо, якщо event group не створено
        vTaskDelay(pdMS_TO_TICKS(timeout_ms));
        scan_in_progress = false;
    }
    
    // Повертаємо поточний режим, якщо змінювали
    if (current_mode == WIFI_MODE_NULL) {
        esp_wifi_stop();
    } else if (current_mode == WIFI_MODE_AP) {
        esp_wifi_set_mode(WIFI_MODE_AP);
    }
    
    return scan_results;
}

// Метод для отримання детальної інформації про підключення
WiFiStatus WiFiManager::getStatus() {
    WiFiStatus status;
    status.connected = wifi_connected;
    
    if (wifi_connected && s_sta_netif) {
        // Отримання IP-адреси
        esp_netif_ip_info_t ip_info;
        esp_netif_get_ip_info(s_sta_netif, &ip_info);
        char ip_str[16];
        snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
        status.ip_address = ip_str;
        
        // Отримання SSID поточної мережі
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            status.ssid = (char*)ap_info.ssid;
            status.rssi = ap_info.rssi;
        }
        
        // Отримання MAC-адреси
        uint8_t mac[6];
        if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK) {
            char mac_str[18];
            snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X", 
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            status.mac_address = mac_str;
        }
    }
    
    return status;
}

// Метод для встановлення облікових даних WiFi без перезавантаження
esp_err_t WiFiManager::setCredentials(const std::string& ssid, const std::string& password, bool connect_now) {
    if (ssid.empty()) {
        ESP_LOGE(TAG, "SSID не може бути порожнім");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Зберігаємо налаштування
    bool saved_ssid = ConfigLoader::set<const char*>("/wifi/ssid", ssid.c_str());
    bool saved_pass = ConfigLoader::set<const char*>("/wifi/pass", password.c_str());
    
    if (!saved_ssid || !saved_pass) {
        ESP_LOGE(TAG, "Помилка збереження конфігурації!");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Облікові дані Wi-Fi збережено: SSID='%s'", ssid.c_str());
    
    // Підключаємося відразу, якщо потрібно
    if (connect_now) {
        // Зупиняємо режим налаштування, якщо активний
        if (provisioning_active) {
            stop_provisioning_server();
            provisioning_active = false;
        }
        
        return connect();
    }
    
    return ESP_OK;
}