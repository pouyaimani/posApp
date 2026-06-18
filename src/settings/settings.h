#ifndef SETTINGS_H_
#define SETTINGS_H_

#include "oop.h"
#include "common.h"
#include <stdint.h>
#include <stdbool.h>

#define MAX_SHIFT_CNT 100

typedef struct __attribute__((packed)) {
    bool touchEnable;

    uint8_t netRoute;
    uint8_t devVolume;
    uint8_t sleepTimeout;
    uint8_t brightness;
    char    loginOperator;
    char    loginDate[6 + 1];

    char     merchantPin[4 + 1];
    char     merchantNo[15 + 1];
    char     merchantAddress[64];
    char     merchantPostalCode[16];
    char     merchantUniqueId[32];
    char     merchantName[40 + 1];
    char     merchantPhone[32];
    char     terminalNo[15 + 1];
    char     mSafePwd[8 + 1];
    char     wfiSSID[32];
    char     wifiMac[32];
    uint32_t wifiEnc;
    char     wifiPwd[63 + 1];
    char     acquirerIIN[12];

    uint32_t appID;
    uint8_t  language;
    uint8_t  printGreyScale;
    uint8_t  updateFlag;
    uint8_t  connectMode;
    bool     maxAmntEnable;
    char     maxAmnt[12 + 1];
    bool     directSaleEn;
    bool     serviceEn[MAX_SERVICE_NUM];
    int      fixedAmountItem;
    char     amountList[12 + 1][MAX_AMOUNT_LIST];
    int      amountListCnt;
    int      fixedAmountCoef;
    uint8_t  shiftEnable;
    uint8_t  shiftActive;

} TerminalSettings;

typedef struct __attribute__((packed)) {
    char     mainServerIp[12 + 1];
    uint16_t mainServerPort;
    uint16_t mainServerNii;
    char     backupServerIp[12 + 1];
    uint16_t backupServerPort;
    bool     useBackupAddressFirst;
    char     tmsIp[12 + 1];
    uint16_t tmsPort;
    uint16_t tmsId;
    char     tmsBackupIp[12 + 1];
    uint16_t tmsBackupPort;
    bool     sslEn;
    uint8_t  forceTMS;
} ServerSettings;

OOP_CLASS(TxnSettings){

};

typedef struct __attribute__((packed)) {
    OOP_METHOD(int, save);
    OOP_METHOD(int, load);
    OOP_METHOD(int, reset);
    TerminalSettings terminal;
    ServerSettings   server;
    TxnSettings      txn;
} DevSettings;

DevSettings* settings();

typedef struct __attribute__((packed)) {
    OOP_METHOD(int, inc);
    OOP_METHOD(int, load);
    uint32_t stan;
    uint32_t batch;
} TxnTraceInfo;

TxnTraceInfo* txnTraceInfo();

#endif