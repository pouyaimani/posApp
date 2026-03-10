#ifndef STORAGE_H_
#define STORAGE_H_

#include "oop.h"
#include <stdbool.h>
#include <stdint.h>
#include "dcfg.h"
#include "event.h"

#define MAX_SHIFT_CNT 100

OOP_CLASS(TerminalSettings) {
    bool mHasLoginOnline;
    bool mIsKeyVolumeOpen;
    bool mHasMargined;
    bool mHasDownloadTmk;
    bool mHasVersionForceUpdate;
    bool mTouchEnable;

    uint8_t mSupportAutoLogin;
    bool mSupportReverse;
    bool mSupportEsBeforeOnline;

    uint8_t netRoute;
    uint8_t devVolume;
    uint8_t mSleepTimeout;
    uint8_t brightness;
    char mLoginOperator;
    char mLoginDate[6 + 1];

    uint8_t mOperatePwd[4 + 1];
    uint8_t m99Pwd[8 + 1];
    uint8_t m00Pwd[6 + 1];
    char mMerchantNo[15 + 1];
    char mTerminalNo[15 + 1];
    char mMerchantName[40 + 1];
    char mBatchNO[6 + 1];
    char mVoucherNO[6 + 1];
    char mSafePwd[8 + 1];
    char mTransmitCreateLinkAttr[32];
    char mWifiSSID[32];                             
    char mWifiMac[32];                              
    uint32_t mWifiEnc;                                   
    char mWifiPwd[63+1];
    char mTmsCheckDate[4 + 1];                      

    uint32_t mTimeoutShowErr;                            
    uint32_t mTimeoutShowTip;                            
    uint32_t mTransmitCreatePPPTimeout;                  
    uint32_t mTransmitErrTipsTimeout;                    
    uint32_t mRecvDataTimeout;                           
    uint32_t mAppID;                                     
    uint8_t mLanguage;
    uint8_t mPrintGreyScale;
    uint8_t mUpdateFlag;
    uint8_t mConnectMode;
    bool maxAmntEnable;
    char maxAmnt[12 + 1];
    bool directSaleEn;
    bool serviceEn[MAX_SERVICE_NUM];
};

OOP_CLASS(ServerSettings) {
    char mMainServerIp[64];         
    char mMainServerPort[6];        
    char mBackupServerBackupIp[64]; 
    char mBackupServerBackupPort[6];
    bool mUseBackupAddressFirst;    
    char mTmsIp[64];                
    char mTmsPort[6];               
    char mTmsBackupIp[64];          
    char mTmsBackupPort[6];         
};

OOP_CLASS(TxnSettings) {
    bool mIsSupportQPS;                 
    bool mIsSupportNoSign;              
    bool mIsQPSBINA;                    
    bool mIsQPSBINB;                    
    bool mIsQPSCDCVM;                   
    bool mIsSupportMag12Pwd;            
    bool mIsSupportEmptyPwd;            
    bool mIsMk3Des;                     
    bool mIsAutoLogout;                 
    bool mIsRFFirst;                    
    bool mIsSupportGM;                  
    bool mIsVerifyMacField;             
    uint8_t mIccAidCount;                    
    uint8_t mIccCAPKCount;                   
    uint8_t mTMkIndex;                       
    uint8_t mWKIndex;                        
    bool mIsPrintDetail;                
    uint8_t mPrintTimes;                     
    uint8_t mPrintTimesQrcode;               
    uint8_t mReversalTimes;                  

    char mTPDU[11];                       
    char mQPSNoPinAmount[12 + 1];         
    char mQPSNoSignAmount[12 + 1];        
    bool mIsSupportTdk;                 
};

OOP_CLASS(Shifts) {
    struct ShiftsData{
        uint32_t startDate;
        uint32_t startTime;
        uint32_t endDate;
        uint32_t endTime;
    } data[MAX_SHIFT_CNT];
    uint16_t latest;
    uint8_t isEnable;
    uint8_t isActive;
};

OOP_CLASS(DevSettings) {
    TerminalSettings terminal;
    ServerSettings server;
    TxnSettings txn;
    Shifts shift;
};

OOP_DECLARE_CLASS(Storage)

OOP_VTABLE(Storage) {
    OOP_IMETHOD(void, Storage, init);
    OOP_IMETHOD(void, Storage, reloadSettings);
    OOP_IMETHOD(void, Storage, applySettings);
};

OOP_CLASS(Storage) {
    OOP_IMPLEMENTS(Storage);
    DevSettings *settings;
};

OOP_CTOR(Storage);

Storage *getStorage(void);

#endif