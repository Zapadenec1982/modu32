#ifndef CORE_MODULE_MANAGER_H
#define CORE_MODULE_MANAGER_H

#include <vector>
#include "base_module.h"
#include "config.h"

class ModuleManager {
public:
    static void init();
    static void register_module(BaseModule* module);
    static void init_modules(ConfigLoader& config);
    static void init_modules(); // Перевантажена версія без ConfigLoader
    static void tick_all();
    static void stop_all();
    static const std::vector<BaseModule*>& getActiveModules();
    static const std::vector<BaseModule*>& get_all_modules(); // Додана функція
};

#endif // CORE_MODULE_MANAGER_H