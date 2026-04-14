#include "bankName.h"
#include "cJSON.h"
#include "file/file.h"
#include "dev/dev.h"
#include "logger.h"

#define BANK_NAME_JSON ""

static BankName *__bankName;
static cJSON *nameEn;
static cJSON *nameFa;

typedef enum {
    EN = 0,
    FA
} Language_t;

static bool findInJson(const char *iin, char *out, size_t size, Language_t lang) {
    char *jsonData = GET_MEM(4096);
    OOP_CALL(file(), read, BANK_NAME_JSON, jsonData, 0, 4096);

    cJSON *root = cJSON_Parse(jsonData);
    if (!root) {
        LOG_ERROR("Json parse error, file = %s", BANK_NAME_JSON);
        return false;
    }

    for (int i = 0; i < cJSON_GetArraySize(root); i++) {
        cJSON *bank = cJSON_GetArrayItem(root, i);

        cJSON *code = cJSON_GetObjectItem(bank, "code");

        if (strcmp(code->valuestring, iin) == 0) {

            if (lang == EN) {
                cJSON *name = cJSON_GetObjectItem(bank, "name_en");
                snprintf(out, size, "s", name->valuestring);
            } else if (lang == FA) {
                cJSON *name = cJSON_GetObjectItem(bank, "name_fa");
                snprintf(out, size, "s", name->valuestring);
            }
            break;
        }
    }
    cJSON_Delete(root);
    FREE_MEM(jsonData);
    return true;
}

void getNameFa(const char * iin, char *out, size_t size) {
    findInJson(iin, out, size, FA);
}

void getNameEn(const char * iin, char *out, size_t size) {
    findInJson(iin, out, size, EN);
}

OOP_CTOR(BankName) {
    self->getNameEn = getNameEn;
    self->getNameFa = getNameFa;
};

BankName* bankName() {
    CALL_ONCE(
        OOP_CALL_CTOR(BankName, __bankName);
    );
    return __bankName;
}