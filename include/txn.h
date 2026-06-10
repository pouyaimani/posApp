#ifndef TXN_H_
#define TXN_H_

#include <stdint.h>
#include "oop.h"

/**********************************************************/
//                         MTI
/**********************************************************/

typedef enum {
	MTI_LOG_ON = 0,
	MTI_CFG,
} MTI_t;

/**********************************************************/
//                    PROCESS CODE
/**********************************************************/

#define SecRelControlInfo	"0800050202031500"

typedef enum {
	TS_START = 0,
	TS_BUILD_REQUEST = 1,

	TS_REQUEST_SEND = 11,
	TS_REQUEST_RECIEVE = 12,

	TS_REQUEST_HAVE_RESPONSE = 3,

	TS_REQUEST_SUCCEED = 30,
	TS_REQUEST_SUCCEED_PRINT = 31,
	TS_REQUEST_SUCCEED_FINALIZED = 100,

	TS_REQUEST_FAILED = 35,
	TS_REQUEST_FAILED_PRINT = 36,
	TS_REQUEST_FAILED_FINALIZED = 101,

	TS_REQUEST_HAVE_NO_RESPONSE = 4,
	TS_REQUEST_REVERSE_PRINT_72H = 40,
	TS_REQUEST_REVERSE_FINALIZED = 110,
} TransactionStatus;

typedef enum {
	TXN_SALE = 0,
	TXN_BILL,
	TXN_TOPUP,
	TXN_BALANCE,
	TXN_PAY,
	TXN_SIM_CHARGE,
	TXN_ALL
} TxnType;

typedef enum {
	CL_NORMAL = 1,
	CL_SPECIFIC = 2,
} ChargeLevel;

typedef struct {
	TxnType txnType;
	uint16_t processCode;
	char pan[16+1];
	uint64_t amount;
	uint32_t refNum;
	uint32_t trace;
	uint32_t RRN;
	uint16_t respCode; 				// responce code
} TxnCore;

typedef union {
    struct {
        char billId[24];
        char paymentId[24];
		char purchaseId[31];
    } bill;

	struct {
        char phoneNumber[12];
        ChargeLevel level;
		char chargePin[16];
		char chargeSerial[16];
    } charge;

	struct {
		char companyName[64];
		char phoneNumber[11+1];
	} kahroba;

	struct {
		uint64_t balance;
		uint64_t ledger;
	} balance;
	
} TxnExtention;

typedef struct {
	uint64_t dateTime;
	TxnCore core;
	TxnExtention extention;
} TxnData;

typedef enum {
	SIM_OP_MCI = 0,
	SIM_OP_IRANCELL,
	SIM_OP_RIGHTEL,
	SIM_OP_TALIA
} SimCardOperators_t;

int8_t getTxnName(TxnType type, char *name, size_t size);

#endif
