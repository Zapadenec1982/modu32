#ifndef CORE_UI_SCHEMA_H
#define CORE_UI_SCHEMA_H

#include "cJSON.h"

class UISchemaBuilder {
public:
    static cJSON* getSchemaJson();
};

#endif // CORE_UI_SCHEMA_H 