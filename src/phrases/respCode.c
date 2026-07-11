#include "phrases.h"
#include <stdbool.h>
#include "common.h"
#include "utility/utility.h"

typedef struct {
    int       code;
    Phrases_t id;
} ResponseCode;

static const ResponseCode responseCodes[] = {
    {0, RESP_0},   {1, RESP_1},   {2, RESP_2},   {3, RESP_3},   {4, RESP_4},
    {5, RESP_5},   {6, RESP_6},   {9, RESP_9},   {12, RESP_12}, {13, RESP_13},
    {14, RESP_14}, {15, RESP_15}, {19, RESP_19}, {23, RESP_23}, {25, RESP_25},
    {30, RESP_30}, {31, RESP_31}, {33, RESP_33}, {34, RESP_34}, {36, RESP_36},
    {38, RESP_38}, {39, RESP_39}, {40, RESP_40}, {41, RESP_41}, {42, RESP_42},
    {43, RESP_43}, {48, RESP_48}, {51, RESP_51}, {54, RESP_54}, {55, RESP_55},
    {57, RESP_57}, {58, RESP_58}, {59, RESP_59}, {61, RESP_61}, {63, RESP_63},
    {65, RESP_65}, {67, RESP_67}, {68, RESP_68}, {75, RESP_75}, {80, RESP_80},
    {83, RESP_83}, {84, RESP_84}, {86, RESP_86}, {89, RESP_89}, {90, RESP_90},
    {91, RESP_91}, {93, RESP_93}, {94, RESP_94}, {95, RESP_95}, {96, RESP_96},
};

void getResponseCode(int code, char* message, size_t size) {
    const char* result = "خطاي نامشخص";

    for (size_t i = 0; i < sizeof(responseCodes) / sizeof(responseCodes[0]);
         i++) {
        if (responseCodes[i].code == code) {
            result = phraseGetDef(responseCodes[i].id);
            break;
        }
    }

    snprintf(message, size, "%s", result);
}