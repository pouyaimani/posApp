#ifndef STORAGE_H_
#define STORAGE_H_

#include "oop.h"
#include <stdbool.h>
#include <stdint.h>
#include "dcfg.h"
#include "event.h"

#define MAX_SHIFT_CNT 100

OOP_CLASS(TerminalSettings)
{
    bool touchEnable;

    uint8_t netRoute;
    uint8_t devVolume;
    uint8_t sleepTimeout;
    uint8_t brightness;
    char loginOperator;
    char loginDate[6 + 1];

    uint8_t merchantPin[4 + 1];
    char merchantNo[15 + 1];
    char terminalNo[15 + 1];
    char merchantName[40 + 1];
    char mSafePwd[8 + 1];
    char wfiSSID[32];
    char wifiMac[32];
    uint32_t wifiEnc;
    char wifiPwd[63 + 1];

    uint32_t appID;
    uint8_t language;
    uint8_t printGreyScale;
    uint8_t updateFlag;
    uint8_t connectMode;
    bool maxAmntEnable;
    char maxAmnt[12 + 1];
    bool directSaleEn;
    bool serviceEn[MAX_SERVICE_NUM];
    int fixedAmountItem;
    char amountList[12 + 1][MAX_AMOUNT_LIST];
    int amountListCnt;
    int fixedAmountCoef;
};

OOP_CLASS(ServerSettings)
{
    char mainServerIp[12 + 1];
    uint16_t mainServerPort;
    uint16_t mainServerId;
    char backupServerIp[12 + 1];
    uint16_t backupServerPort;
    bool useBackupAddressFirst;
    char tmsIp[12 + 1];
    uint16_t tmsPort;
    uint16_t tmsId;
    char tmsBackupIp[12 + 1];
    uint16_t tmsBackupPort;
    bool sslEn;
};

OOP_CLASS(TxnSettings){};

OOP_CLASS(Shifts)
{
    struct ShiftsData
    {
        uint32_t startDate;
        uint32_t startTime;
        uint32_t endDate;
        uint32_t endTime;
    } data[MAX_SHIFT_CNT];
    uint16_t latest;
    uint8_t isEnable;
    uint8_t isActive;
};

OOP_CLASS(DevSettings)
{
    TerminalSettings terminal;
    ServerSettings server;
    TxnSettings txn;
    Shifts shift;
};

OOP_DECLARE_CLASS(Storage)

OOP_VTABLE(Storage)
{
    OOP_IMETHOD(void, Storage, init);
    OOP_IMETHOD(void, Storage, reloadSettings);
    OOP_IMETHOD(void, Storage, applySettings);
    OOP_IMETHOD(void, Storage, resetSettings);
};

OOP_CLASS(Storage)
{
    OOP_IMPLEMENTS(Storage);
    DevSettings *settings;
};

OOP_CTOR(Storage);

Storage *getStorage(void);

#define SAVE_SETTINGS() OOP_CALL(getStorage(), applySettings)
#define RESET_SETTINGS() OOP_CALL(getStorage(), resetSettings)

#endif