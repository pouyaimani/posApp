#ifndef PHRASES_H_
#define PHRASES_H_

#include "oop.h"

/**********************************************************************
 *
 *                  Macro Definitions Categorized
 * 
 *          This document categorizes the provided macro 
 *          definitions for clarity and easier management.
 * 
 * 
 **********************************************************************/

/**********************************************************************
 *
 *                  General Terms and Phrases
 *          These are common prompts, statuses, and generic
 *          phrases used throughout the interface:
 * 
 **********************************************************************/

#define PHRASE_PLEASE_WAIT                          "pleaseWait"
#define PHRASE_NO_RESULT_FOUND                      "noResultFound"
#define PHRASE_EXTRACTING_DATA                      "extractingData"
#define PHRASE_ENTER_REF_NUM                        "enterRefNum"
#define PHRASE_ENTER_TRACE                          "enterTrace"
#define PHRASE_MERCHANT                             "merchant"
#define PHRASE_SUPERVISOR                           "supervisor"
#define PHRASE_CUSTOMER                             "customer"
#define PHRASE_PLEASE                               "please"
#define PHRASE_ENTER_SERVER                         "enterServer"
#define PHRASE_ENTER_PORT                           "enterPort"
#define PHRASE_INVALID_IP                           "invalidIp"
#define PHRASE_INVALID_PORT                         "invalidPort"
#define PHRASE_WIFI                                 "wifi"
#define PHRASE_GPRS                                 "gprs"
#define PHRASE_SOUND_STTINGS                        "soundSettings"
#define PHRASE_ENERGY_SETTINGS                      "energySettings"
#define PHRASE_RECEIPT_SETTINGS                     "receiptSettings"
#define PHRASE_BRIGHTNESS_SETTINGS                  "brightnessSettings"
#define PHRASE_TOUCH_STATUS                         "touchStatus"
#define PHRASE_UPDATE_DATE_TIME                     "updateDateTime"
#define PHRASE_ENABLE                               "enable"
#define PHRASE_DISABLE                              "disable"
#define PHRASE_POWER_OFF                            "powerOff"
#define PHRASE_MENU                                 "menu"
#define PHRASE_CANCEL                               "cancel"
#define PHRASE_CONFIRM                              "confirm"

/**********************************************************************
 *
 *                  Financial & Transaction-Related
 *          Terms linked to transaction types, reports, 
 *          and financial info:
 * 
 **********************************************************************/

#define PHRASE_AMOUNT                               "amount"
#define PHRASE_AUTHENTICATION_TYPE                  "authenticationType"
#define PHRASE_SUCCESSFUL_OPERATION                 "successfulOperation"
#define PHRASE_UNSUCCESSFUL_OPERATION               "unsuccessfulOperation"
#define PHRASE_INSUFFICIENT_FUND                    "insufficientFund"
#define PHRASE_INVALID_PIN                          "invalidPin"
#define PHRASE_INVALID_CARD                         "invalidCard"
#define PHRASE_INVALID_TRANSACTION                  "invalidTransaction"
#define PHRASE_PREPAID_PIN                          "prepaidPin"
#define PHRASE_BILL_ID                              "billId"
#define PHRASE_PAYMENT_ID                           "paymentId"
#define PHRASE_DEBIT_CARD                           "debitCard"
#define PHRASE_CREDIT_CARD                          "creditCard"
#define PHRASE_PREPAID_CARD                         "prepaidCard"
#define PHRASE_CUSTOMERS_SECOND_STATEMENT           "customersSecondStatement"
#define PHRASE_ANY_DEDUCTION_TO_YOUR_ACCOUNT        "anyDeductionToYourAccount"
#define PHRASE_REFUND_AMOUNT                        "refundAmount"
#define PHRASE_LOCAL_DATE_REFUND_TRANSACTION        "localDateRefundTransaction"
#define PHRASE_LOCAL_TIME_REFUND_TRANSACTION        "localTimeRefundTransaction"
#define PHRASE_MONEY_REFUNDED_WITHIN_WORKDAY        "moneyRefundedWithinWorkday"
#define PHRASE_MERCHANTS_NAME                       "merchantsName"
#define PHRASE_PURCHASE                             "purchase"
#define PHRASE_BILL_PAYMENT                         "billPayment"
#define PHRASE_PREPAID_RECHARGE                     "prepaidRecharge"
#define PHRASE_BALANCE_INQUIRY                      "balanceInquiry"
#define PHRASE_CUSTOMERS_RECEIPT                    "customersReceipt"
#define PHRASE_MERCHANTS_RECEIPT                    "merchantsReceipt"
#define PHRASE_TERMINAL_ID                          "terminalId"
#define PHRASE_TERMINAL_NUMBER                      "terminalNumber"
#define PHRASE_MERCHANT_ID                          "merchantId"
#define PHRASE_APPLICATION_NAME                     "applicationName"
#define PHRASE_DEVICE_TYPE                          "deviceType"
#define PHRASE_CARD_NUMBER                          "cardNumber"
#define PHRASE_ISSUING_BANK                         "issuingBank"
#define PHRASE_ACQUIRING_BANK                       "acquiringBank"
#define PHRASE_TRACE_NUMBER                         "traceNumber"
#define PHRASE_LOCAL_DATE_TRANSACTION               "localDateTransaction"
#define PHRASE_LOCAL_TIME_TRANSACTION               "localTimeTransaction"
#define PHRASE_RETRIEVAL_REFERENCE_NUMBER           "retrievalReferenceNumber"

/**********************************************************************
 *
 *                  Report and Transaction Types
 *          Specific report or transaction categories:
 * 
 **********************************************************************/

#define PHRASE_DAILY_REPORT                         "dailyReport"
#define PHRASE_AGGREGATED_REPORT                    "aggregatedReport"
#define PHRASE_TRANACTION_DETAILS                   "transactionDetails"
#define PHRASE_TXN_SALE                             "txnSale"
#define PHRASE_TXN_BALANCE                          "txnBalance"
#define PHRASE_TXN_BILL_PAYMENT                     "txnBillPayment"
#define PHRASE_TXN_PAY                              "txnPay"
#define PHRASE_TXN_VOUCHER                          "txnVoucher"
#define PHRASE_TXN_TOP_UP                           "txnTopUp"
#define PHRASE_ALL_TXNS                             "allTxns"

/**********************************************************************
 *
 *                  Operational/Action Buttons & Interfaces
 *                  Common interface actions
 * 
 **********************************************************************/

#define PHRASE_REPRINT                              "reprint"
#define PHRASE_UPDATE_APP                           "updateApp"
#define PHRASE_RESET_MERCHAT_PIN                    "resetMerchatPin"
#define PHRASE_DEFAULT_SETTINGS                     "defaultSettings"
#define PHRASE_SERVER_SETTINGS                      "serverSettings"
#define PHRASE_TMS_SETTINGS                         "tmsSettings"


/**********************************************************************
 *
 *                   Miscellaneous/Other Settings & Labels
 *              Less common or context-specific phrases:
 * 
 **********************************************************************/

#define PHRASE_SPECIAL_PROJECTS                    "specialProjects"
#define PHRASE_SSL                                 "ssl"
#define PHRASE_KEY_INJECTION                       "keyInjection"
#define PHRASE_CONFIGURATION                       "configuration"
#define PHRASE_UPDATE_DATE_TIME                    "updateDateTime"
#define PHRASE_NETWORK_SETTINGS                    "networkSettings"
#define PHRASE_POWER_OFF                           "powerOff"
#define PHRASE_MENU                                "menu"
#define PHRASE_CANCEL                              "cancel"
#define PHRASE_CONFIRM                             "confirm"

OOP_CLASS(Phrases) {
    OOP_METHOD(void, getPhrase, const char *phrase, int lang, const char *out, size_t size);
};

Phrases *phrases();

#endif