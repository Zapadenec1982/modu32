#include "module_manager.h"
#include "esp_log.h"
#include <vector>
#include <mutex>

static const char* TAG = "ModuleManager";

namespace {
    static std::vector<BaseModule*> registered_modules;
    static std::vector<BaseModule*> active_modules;
    static std::mutex modules_mutex;
}

void ModuleManager::init() {
    std::lock_guard<std::mutex> lock(modules_mutex);
    registered_modules.clear();
    active_modules.clear();
}

void ModuleManager::register_module(BaseModule* module) {
    std::lock_guard<std::mutex> lock(modules_mutex);
    if (module) {
        ESP_LOGI(TAG, "Реєстрація модуля: %s", module->getName());
        registered_modules.push_back(module);
    }
}

void ModuleManager::init_modules(ConfigLoader& config) {
    std::lock_guard<std::mutex> lock(modules_mutex);
    ESP_LOGI(TAG, "Ініціалізація %d зареєстрованих модулів", registered_modules.size());
    
    for (auto* module : registered_modules) {
        ESP_LOGI(TAG, "Ініціалізація модуля: %s", module->getName());
        esp_err_t ret = module->init();
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Модуль %s успішно ініціалізовано", module->getName());
            active_modules.push_back(module);
        } else {
            ESP_LOGE(TAG, "Не вдалося ініціалізувати модуль %s: %s", 
                    module->getName(), esp_err_to_name(ret));
        }
    }
    
    ESP_LOGI(TAG, "Активовано %d модулів", active_modules.size());
}

void ModuleManager::init_modules() {
    std::lock_guard<std::mutex> lock(modules_mutex);
    ESP_LOGI(TAG, "Ініціалізація %d зареєстрованих модулів (без конфігурації)", registered_modules.size());
    
    for (auto* module : registered_modules) {
        ESP_LOGI(TAG, "Ініціалізація модуля: %s", module->getName());
        esp_err_t ret = module->init();
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Модуль %s успішно ініціалізовано", module->getName());
            active_modules.push_back(module);
        } else {
            ESP_LOGE(TAG, "Не вдалося ініціалізувати модуль %s: %s", 
                    module->getName(), esp_err_to_name(ret));
        }
    }
    
    ESP_LOGI(TAG, "Активовано %d модулів", active_modules.size());
}

void ModuleManager::tick_all() {
    std::lock_guard<std::mutex> lock(modules_mutex);
    for (auto* m : active_modules) {
        m->tick();
    }
}

void ModuleManager::stop_all() {
    std::lock_guard<std::mutex> lock(modules_mutex);
    for (auto* m : active_modules) {
        m->stop();
    }
    active_modules.clear();
}

const std::vector<BaseModule*>& ModuleManager::getActiveModules() {
    return active_modules;
}

const std::vector<BaseModule*>& ModuleManager::get_all_modules() {
    return registered_modules.empty() ? active_modules : registered_modules;
}