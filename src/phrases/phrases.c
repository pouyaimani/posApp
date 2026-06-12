#include "phrases.h"
#include "utility/cJSON.h"
#include <stdbool.h>
#include "file/file.h"
#include "sys/sys.h"
#include "logger.h"
#include "common.h"
#include "settings/settings.h"

// static bool findInJson(const char *phrase, char *out, size_t size, Language_t lang) {
//     char *jsonData = MEM_ALLOC(4096);
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
//     MEM_FREE(jsonData);
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

typedef struct {
    int code;
    const char *message;
} ResponseCode;

static const ResponseCode responseCodes[] = {
    { 0,  "عملیات موفق" },
    { 1,  "لغوشده توسط صادرکننده" },
    { 2,  "از قبل اصلاح خورده است" },
    { 3,  "پذیرنده کارت نامعتبر" },
    { 4,  "کارت مسدود شده است" },
    { 5,  "تراکنش لغو شده" },
    { 6,  "خطايي رخ داده" },
    { 9,  "مشغول بودن سیستم" },
    { 12, "تراکنش نامعتبر" },
    { 13, "مبلغ نادرست" },
    { 14, "شماره کارت ناشناخته" },
    { 15, "صادرکننده نامعتبر" },
    { 19, "ورود مجدد تراکنش" },
    { 23, "تراکنش نامعتبر پذيرنده" },
    { 25, "داده ای یافت نشد" },
    { 30, "قالب پیام نامعتبر" },
    { 31, "پذیرنده کارت نامعتبر" },
    { 33, "انقضای کارت" },
    { 34, "عدم تایید" },
    { 36, "کارت محدود شده" },
    { 38, "ورود رمز از حد مجاز گذشته" },
    { 39, "حساب کارت نامشخص" },
    { 40, "سرويس نامعتبر" },
    { 41, "کارت مفقود یا مسدود موقت" },
    { 42, "حسابي به کارت متصل نيست" },
    { 43, "کارت مسدود است" },
    { 48, "قبض تکراری" },
    { 51, "عدم موجودی کافی" },
    { 54, "کارت نامعتبر است" },
    { 55, "رمز نامعتبر است" },
    { 57, "تراکنش غیر مجاز کارت" },
    { 58, "تراکنش غیر مجاز ترمینال" },
    { 59, "کارت مظنون به تقلب" },
    { 61, "مبلغ بیش از حد مجاز" },
    { 63, "تمهیدات امنیتی نقض گردیده" },
    { 65, "محدودیت در تعداد برداشت" },
    { 67, "کارت ضبط شد" },
    { 68, "جواب دریافتی با تاخیر آمده" },
    { 75, "تعداد رمز غلط بیش از حدمجاز" },
    { 80, "اشکال در پردازش تراکنش" },
    { 83, "سرويس دهنده يا شاپرک غيرفعال" },
    { 84, "سوییچ صادرکننده غیرفعال" },
    { 86, "بانک صادرکننده غيرفعال" },
    { 89, "داده ها نامعتبر" },
    { 90, "درحال پردازش پایان روز" },
    { 91, "عدم دریافت پاسخ" },
    { 93, "تراکنش کامل نشده" },
    { 94, "تراکنش تکراری" },
    { 95, "رمز قبلی غلط" },
    { 96, "اشکال درعملکرد سيستم" },
};

void getResponseCode(int code, char *message, size_t size) {
    const char *result = "خطاي نامشخص";

    for (size_t i = 0; i < sizeof(responseCodes) / sizeof(responseCodes[0]); i++) {
        if (responseCodes[i].code == code) {
            result = responseCodes[i].message;
            break;
        }
    }

    snprintf(message, size, "%s", result);
}