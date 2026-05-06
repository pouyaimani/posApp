#ifndef TXN_H_
#define TXN_H_

#include <stdint.h>
#include "oop.h"

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
	CL_NORMAL = 1,
	CL_SPECIFIC = 2,
} ChargeLevel;

OOP_CLASS(TxnData) {
	uint8_t id;
	char processCode[6+1];
	char maskedPan[16+1];
	char purchaseId[31];
	char amount[12+1];
	char priceWithDiscount[12+1];
	char stan[6+1];
	char trace[6+1];
	char dateTime[14+1];
	char RRN[12+1];
	char billId[24];
	char paymentId[24];
	unsigned long companyId;
	char companyName[64]; // 116 kahroba
	char phoneNumber[11+1]; // For Kahroba
	ChargeLevel chargeLevel;
	char accountIndex[8];
	char accountCaption[32+1];
	char responseCode[2+1];

	TransactionStatus  Status;
};


#endif
