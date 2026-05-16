#include "phrases.h"
#include "utility/cJSON.h"
#include <stdbool.h>
#include "file/file.h"
#include "sys/sys.h"
#include "logger.h"
#include "common.h"
#include "settings/settings.h"

// static bool findInJson(const char *phrase, char *out, size_t size, Language_t lang) {
//     char *jsonData = GET_MEM(4096);
//     FileErr_t ferr = OOP_CALL(file(), read, PHRASES_JSON_ADDR, jsonData, 0, 4096);
//     if (ferr != FILE_ERR_OK) {
//         LOG_ERROR("Error in reading file: %s", PHRASES_JSON_ADDR);
//         return false;
//     }

//     cJSON *root = cJSON_Parse(jsonData);
//     if (!root) {
//         LOG_ERROR("Json parse error, file: %s", PHRASES_JSON_ADDR);
//         return false;
//     }

//     for (int i = 0; i < cJSON_GetArraySize(root); i++) {
//         cJSON *item = cJSON_GetArrayItem(root, i);

//         cJSON *phrs = cJSON_GetObjectItem(item, "phrase");

//         if (strcmp(phrs->valuestring, phrase) == 0) {

//             if (lang == EN) {
//                 cJSON *name = cJSON_GetObjectItem(phrs, "en");
//                 snprintf(out, size, "s", name->valuestring);
//             } else if (lang == FA) {
//                 cJSON *name = cJSON_GetObjectItem(phrs, "fa");
//                 snprintf(out, size, "s", name->valuestring);
//             }
//             break;
//         }
//     }
//     cJSON_Delete(root);
//     FREE_MEM(jsonData);
//     return true;
// }

// void getPhrase(const char *phrase, int lang, const char *out, size_t size) {
//     findInJson(phrase, lang, out, size);
// }

static const PhraseEntry gPhrases[] = {
#define X(id, en, fa) \
    [id] = { .text = { [LNG_EN] = en, [LNG_FA] = fa } },

#include "phrases.def"
#undef X
};

const char *phraseGet(Phrases_t id, Language_t lang) {
    if (id >= PHRASE_T_COUNT || lang >= LNG_COUNT)
        return "";

    return gPhrases[id].text[lang];
}

const char *phraseGetDef(Phrases_t id) {
    Language_t lng = (Language_t)settings()->terminal.language;
    return phraseGet(id, lng);
}