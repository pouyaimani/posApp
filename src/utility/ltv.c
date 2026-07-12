#include "ltv.h"
#include "common.h"
#include "utility.h"

static void pubBytesToHexString(unsigned char* bytes, size_t buflen,
                                char* retval) {
    int i = 0;
    for (i = 0; i < buflen; i++) {
        sprintf(retval + i * 2, "%2.2X", bytes[i]);
    }
}

void setCommonLtv(char* deviceSerial, const char* version, int iLang,
                  char* buff) {
    int  len         = 0;
    char temp[128]   = {0};
    char pack[128]   = {0};
    char buffer[128] = {0};
    char model[64]   = {0};

    len = strlen(deviceSerial);
    pubBytesToHexString((unsigned char*)deviceSerial, len, temp);
    len += 1;
    sprintf(pack, "%02d01%s", len, temp);
    strcpy(buffer, pack);

    memset(temp, 0x00, sizeof(temp));
    memset(pack, 0x00, sizeof(pack));
    len = strlen(version);
    pubBytesToHexString((unsigned char*)version, len, temp);
    len += 1;
    sprintf(pack, "%02d02%s", len, temp);
    strcat(buffer, pack);

    memset(pack, 0x00, sizeof(pack));
    sprintf(pack, "0203%s", (iLang == 1) ? "31" : "30");
    strcat(buffer, pack);

    memset(model, 0x00, sizeof(model));
    memset(temp, 0x00, sizeof(temp));
    memset(pack, 0x00, sizeof(pack));
    strcpy(model, "T32G");
    len = strlen(model);
    pubBytesToHexString((unsigned char*)model, len, temp);
    len += 1;
    sprintf(pack, "%02d04%s", len, temp);
    strcat(buffer, pack);

    sprintf(buff, "%s", buffer);
}

int unpackLtv(char* buffer, LtvStructInfo ltvStructInfo[]) {
    int c = 0;
    int i = 0;
    DEFINE_STRING(lenc, 3);
    DEFINE_STRING(temp, 512);

    while (true) {
        RESET_STRING(lenc);
        memcpy(lenc, buffer + c, 2);
        ltvStructInfo[i].len = libAtoi(lenc);
        c += 2;
        if (ltvStructInfo[i].len == 0)
            break;
        RESET_STRING(temp);
        memcpy(temp, buffer + c, ltvStructInfo[i].len * 2);
        memcpy(ltvStructInfo[i].tag, temp, 2);
        memcpy(ltvStructInfo[i].data, temp + 2, (ltvStructInfo[i].len * 2) - 2);
        c += (ltvStructInfo[i].len * 2);
        i++;
    }
    return i;
}