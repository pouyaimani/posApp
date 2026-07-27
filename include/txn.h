#ifndef TXN_H_
#define TXN_H_

#include <stdint.h>
#include "oop.h"
#include "len.h"

#define STAN_SIZE   6
#define PRCODE_SIZE 6
#define MTI_SIZE    4

/**********************************************************/
//                         MTI
/**********************************************************/

typedef enum {
    /*=====================================================
     * AUTHORIZATION (01xx)
     *=====================================================*/
    MTI_AUTH_REQ         = 100,
    MTI_AUTH_RESP        = 110,
    MTI_AUTH_ADVICE      = 120,
    MTI_AUTH_ADVICE_RESP = 130,

    /*=====================================================
     * FINANCIAL (02xx)
     *=====================================================*/
    MTI_FIN_REQ          = 200,
    MTI_FIN_RESP         = 210,
    MTI_FIN_ADVICE       = 220,
    MTI_FIN_ADVICE_RESP  = 230,
    MTI_FIN_NOTIFICATION = 240,

    /*=====================================================
     * FILE ACTIONS (03xx)
     *=====================================================*/
    MTI_FILE_REQ         = 300,
    MTI_FILE_RESP        = 310,
    MTI_FILE_ADVICE      = 320,
    MTI_FILE_ADVICE_RESP = 330,

    /*=====================================================
     * REVERSAL (04xx)
     *=====================================================*/
    MTI_REV_REQ         = 400,
    MTI_REV_RESP        = 410,
    MTI_REV_ADVICE      = 420,
    MTI_REV_ADVICE_RESP = 430,

    /*=====================================================
     * RECONCILIATION / SETTLEMENT (05xx)
     *=====================================================*/
    MTI_RECON_REQ         = 500,
    MTI_RECON_RESP        = 510,
    MTI_RECON_ADVICE      = 520,
    MTI_RECON_ADVICE_RESP = 530,

    /*=====================================================
     * ADMINISTRATIVE (06xx)
     *=====================================================*/
    MTI_ADMIN_REQ         = 600,
    MTI_ADMIN_RESP        = 610,
    MTI_ADMIN_ADVICE      = 620,
    MTI_ADMIN_ADVICE_RESP = 630,

    /*=====================================================
     * FEE COLLECTION (07xx)
     *=====================================================*/
    MTI_FEE_REQ  = 700,
    MTI_FEE_RESP = 710,

    /*=====================================================
     * NETWORK MANAGEMENT (08xx)
     *=====================================================*/
    MTI_NET_REQ         = 800,
    MTI_NET_RESP        = 810,
    MTI_NET_ADVICE      = 820,
    MTI_NET_ADVICE_RESP = 830,

} Mti_t;

/**********************************************************/
//                    PROCESS CODE
/**********************************************************/

typedef enum {
    PRC_LOG_ON       = 920000,
    PRC_CFG          = 930000,
    PRC_PURCHASE     = 0,
    PRC_BALANCE      = 310000,
    PRC_BILL_PAYMENT = 170000,
    PRC_VOUCHER      = 180000,
    PRC_TOPUP        = 220000,

    PRC_SETTLE  = -1,
    PRC_REVERSE = -1,
} PrCode_t;

#define SecRelControlInfo "0800050202031500"

typedef enum {

    TXN_STATUS_APPROVED,

    TXN_STATUS_DECLINED,

    TXN_STATUS_PENDING_REVERSE,

    TXN_STATUS_REVERSED,

    TXN_STATUS_PENDING_SETTLEMENT,

    TXN_STATUS_SETTLED

} TxnStatus;

typedef enum {
    TXN_SALE = 0,
    TXN_BILL,
    TXN_TOPUP,
    TXN_BALANCE,
    TXN_PAY,
    TXN_TOP_UP,
    TXN_VOUCHER,
    TXN_LOGON,
    TXN_CFG,
    TXN_ALL
} TxnType;

typedef enum {
    CL_NORMAL   = 1,
    CL_SPECIFIC = 2,
} ChargeLevel;

typedef struct {
    TxnType  txnType;
    Mti_t    mti;
    uint32_t processCode;
    char     pan[LEN_MAX_PAN + 1];
    uint64_t amount;
    uint64_t rrn;
    uint32_t trace;
    uint32_t stan;
    uint16_t respCode; // responce code
} TxnCore;

typedef union {
    struct {
        char billId[24];
        char paymentId[24];
        char purchaseId[31];
    } bill;

    struct {
        char        phoneNumber[12];
        ChargeLevel level;
        char        chargePin[16];
        char        chargeSerial[16];
    } charge;

    struct {
        char companyName[64];
        char phoneNumber[11 + 1];
    } kahroba;

    struct {
        uint64_t available;
        uint64_t ledger;
    } balance;

} TxnExtention;

typedef struct {
    uint64_t     dateTime;
    TxnStatus    status;
    TxnCore      core;
    TxnExtention extention;
} TxnData;

typedef enum {
    SIM_OP_MCI = 0,
    SIM_OP_IRANCELL,
    SIM_OP_RIGHTEL,
    SIM_OP_TALIA
} SimCardOperators_t;

int8_t getTxnName(TxnType type, char* name, size_t size);

#endif
