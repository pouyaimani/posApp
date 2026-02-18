#ifndef MSG_ISO8583_H_
#define MSG_ISO8583_H_

#include "../msg.h"

#if MSG_STANDARD == ISO8583

/* ===== Primary Bitmap Fields ===== */

#define ELEMENT_BITMAP_SECONDARY                    1
#define ELEMENT_PAN                                 2
#define ELEMENT_PROCESSING_CODE                     3
#define ELEMENT_AMOUNT_TRANSACTION                  4
#define ELEMENT_AMOUNT_SETTLEMENT                   5
#define ELEMENT_AMOUNT_CARDHOLDER_BILLING           6
#define ELEMENT_TRANSMISSION_DATE_TIME              7
#define ELEMENT_AMOUNT_CARDHOLDER_FEE               8
#define ELEMENT_CONVERSION_RATE_SETTLEMENT          9
#define ELEMENT_CONVERSION_RATE_BILLING             10
#define ELEMENT_STAN                                11
#define ELEMENT_TIME_LOCAL_TRANSACTION              12
#define ELEMENT_DATE_LOCAL_TRANSACTION              13
#define ELEMENT_DATE_EXPIRATION                     14
#define ELEMENT_DATE_SETTLEMENT                     15
#define ELEMENT_DATE_CONVERSION                     16
#define ELEMENT_DATE_CAPTURE                        17
#define ELEMENT_MERCHANT_TYPE                       18
#define ELEMENT_ACQUIRING_COUNTRY_CODE              19
#define ELEMENT_PAN_EXTENDED_COUNTRY_CODE           20
#define ELEMENT_FORWARDING_COUNTRY_CODE             21
#define ELEMENT_POS_ENTRY_MODE                      22
#define ELEMENT_CARD_SEQUENCE_NUMBER                23
#define ELEMENT_NETWORK_INTL_ID                     24
#define ELEMENT_POS_CONDITION_CODE                  25
#define ELEMENT_POS_PIN_CAPTURE_CODE                26
#define ELEMENT_AUTH_ID_RESPONSE_LENGTH             27
#define ELEMENT_AMOUNT_TRANSACTION_FEE              28
#define ELEMENT_AMOUNT_SETTLEMENT_FEE               29
#define ELEMENT_AMOUNT_TXN_PROCESSING_FEE           30
#define ELEMENT_AMOUNT_SETTLEMENT_PROCESSING_FEE    31
#define ELEMENT_ACQUIRING_INSTITUTION_ID            32
#define ELEMENT_FORWARDING_INSTITUTION_ID           33
#define ELEMENT_PAN_EXTENDED                        34
#define ELEMENT_TRACK2                              35
#define ELEMENT_TRACK3                              36
#define ELEMENT_RETRIEVAL_REFERENCE_NUMBER          37
#define ELEMENT_AUTH_ID_RESPONSE                    38
#define ELEMENT_RESPONSE_CODE                       39
#define ELEMENT_SERVICE_RESTRICTION_CODE            40
#define ELEMENT_TERMINAL_ID                         41
#define ELEMENT_CARD_ACCEPTOR_ID                    42
#define ELEMENT_CARD_ACCEPTOR_NAME_LOCATION         43
#define ELEMENT_ADDITIONAL_RESPONSE_DATA            44
#define ELEMENT_TRACK1                              45
#define ELEMENT_ADDITIONAL_DATA_ISO                 46
#define ELEMENT_ADDITIONAL_DATA_NATIONAL            47
#define ELEMENT_ADDITIONAL_DATA_PRIVATE             48
#define ELEMENT_CURRENCY_CODE_TRANSACTION           49
#define ELEMENT_CURRENCY_CODE_SETTLEMENT            50
#define ELEMENT_CURRENCY_CODE_BILLING               51
#define ELEMENT_PIN_DATA                            52
#define ELEMENT_SECURITY_CONTROL_INFO               53
#define ELEMENT_ADDITIONAL_AMOUNTS                  54
#define ELEMENT_ICC_DATA                            55
#define ELEMENT_RESERVED_ISO_56                     56
#define ELEMENT_RESERVED_NATIONAL_57                57
#define ELEMENT_RESERVED_NATIONAL_58                58
#define ELEMENT_RESERVED_NATIONAL_59                59
#define ELEMENT_RESERVED_PRIVATE_60                 60
#define ELEMENT_RESERVED_PRIVATE_61                 61
#define ELEMENT_RESERVED_PRIVATE_62                 62
#define ELEMENT_RESERVED_PRIVATE_63                 63
#define ELEMENT_MAC                                 64

/* ===== Secondary Bitmap Fields ===== */

#define ELEMENT_BITMAP_TERTIARY                     65
#define ELEMENT_SETTLEMENT_CODE                     66
#define ELEMENT_EXTENDED_PAYMENT_CODE               67
#define ELEMENT_RECEIVING_INSTITUTION_COUNTRY_CODE  68
#define ELEMENT_SETTLEMENT_INSTITUTION_COUNTRY_CODE 69
#define ELEMENT_NETWORK_MANAGEMENT_INFO_CODE        70
#define ELEMENT_MESSAGE_NUMBER                      71
#define ELEMENT_MESSAGE_NUMBER_LAST                 72
#define ELEMENT_DATE_ACTION                         73
#define ELEMENT_CREDITS_NUMBER                      74
#define ELEMENT_CREDITS_REVERSAL_NUMBER             75
#define ELEMENT_DEBITS_NUMBER                       76
#define ELEMENT_DEBITS_REVERSAL_NUMBER              77
#define ELEMENT_TRANSFER_NUMBER                     78
#define ELEMENT_TRANSFER_REVERSAL_NUMBER            79
#define ELEMENT_INQUIRIES_NUMBER                    80
#define ELEMENT_AUTHORIZATION_NUMBER                81
#define ELEMENT_CREDITS_PROCESSING_FEE_AMOUNT       82
#define ELEMENT_CREDITS_TRANSACTION_FEE_AMOUNT      83
#define ELEMENT_DEBITS_PROCESSING_FEE_AMOUNT        84
#define ELEMENT_DEBITS_TRANSACTION_FEE_AMOUNT       85
#define ELEMENT_CREDITS_AMOUNT                      86
#define ELEMENT_CREDITS_REVERSAL_AMOUNT              87
#define ELEMENT_DEBITS_AMOUNT                       88
#define ELEMENT_DEBITS_REVERSAL_AMOUNT               89
#define ELEMENT_ORIGINAL_DATA_ELEMENTS              90
#define ELEMENT_FILE_UPDATE_CODE                    91
#define ELEMENT_FILE_SECURITY_CODE                  92
#define ELEMENT_RESPONSE_INDICATOR                  93
#define ELEMENT_SERVICE_INDICATOR                   94
#define ELEMENT_REPLACEMENT_AMOUNTS                 95
#define ELEMENT_MESSAGE_SECURITY_CODE               96
#define ELEMENT_NET_SETTLEMENT_AMOUNT               97
#define ELEMENT_PAYEE                               98
#define ELEMENT_SETTLEMENT_INSTITUTION_ID_CODE      99
#define ELEMENT_RECEIVING_INSTITUTION_ID_CODE       100
#define ELEMENT_FILE_NAME                           101
#define ELEMENT_ACCOUNT_ID_1                        102
#define ELEMENT_ACCOUNT_ID_2                        103
#define ELEMENT_TRANSACTION_DESCRIPTION             104
#define ELEMENT_RESERVED_ISO_105                    105
#define ELEMENT_RESERVED_ISO_106                    106
#define ELEMENT_RESERVED_ISO_107                    107
#define ELEMENT_RESERVED_ISO_108                    108
#define ELEMENT_RESERVED_ISO_109                    109
#define ELEMENT_RESERVED_ISO_110                    110
#define ELEMENT_RESERVED_ISO_111                    111
#define ELEMENT_RESERVED_NATIONAL_112               112
#define ELEMENT_RESERVED_NATIONAL_113               113
#define ELEMENT_RESERVED_NATIONAL_114               114
#define ELEMENT_RESERVED_NATIONAL_115               115
#define ELEMENT_RESERVED_NATIONAL_116               116
#define ELEMENT_RESERVED_NATIONAL_117               117
#define ELEMENT_RESERVED_NATIONAL_118               118
#define ELEMENT_RESERVED_NATIONAL_119               119
#define ELEMENT_RESERVED_PRIVATE_120                120
#define ELEMENT_RESERVED_PRIVATE_121                121
#define ELEMENT_RESERVED_PRIVATE_122                122
#define ELEMENT_RESERVED_PRIVATE_123                123
#define ELEMENT_RESERVED_PRIVATE_124                124
#define ELEMENT_RESERVED_PRIVATE_125                125
#define ELEMENT_RESERVED_PRIVATE_126                126
#define ELEMENT_RESERVED_PRIVATE_127                127
#define ELEMENT_MAC_2                               128

/* Field format types */
typedef enum {
    FMT_N,      /* Numeric */
    FMT_AN,     /* Alphanumeric */
    FMT_ANS,    /* Alphanumeric Special */
    FMT_B,      /* Binary */
    FMT_Z       /* Track data */
} Isoformat_t;

/* Length types */
typedef enum {
    LEN_FIXED,
    LEN_LLVAR,
    LEN_LLLVAR
} IsolenType_t;

/* Field descriptor */
typedef struct {
    uint16_t field;
    const char *name;
    Isoformat_t format;
    IsolenType_t lenType;
    uint16_t maxLen;
} IsofieldDef_t;

#define ISO8583_FIELD_TABLE(X) \
X(1,   "Bitmap Secondary",                      FMT_B,   LEN_FIXED, 8)   \
X(2,   "Primary Account Number",                FMT_N,   LEN_LLVAR, 19)  \
X(3,   "Processing Code",                       FMT_N,   LEN_FIXED, 6)   \
X(4,   "Amount Transaction",                    FMT_N,   LEN_FIXED, 12)  \
X(5,   "Amount Settlement",                     FMT_N,   LEN_FIXED, 12)  \
X(6,   "Amount Cardholder Billing",             FMT_N,   LEN_FIXED, 12)  \
X(7,   "Transmission Date & Time",              FMT_N,   LEN_FIXED, 10)  \
X(8,   "Amount Cardholder Billing Fee",         FMT_N,   LEN_FIXED, 8)   \
X(9,   "Conversion Rate Settlement",            FMT_N,   LEN_FIXED, 8)   \
X(10,  "Conversion Rate Billing",               FMT_N,   LEN_FIXED, 8)   \
X(11,  "System Trace Audit Number",             FMT_N,   LEN_FIXED, 6)   \
X(12,  "Time Local Transaction",                FMT_N,   LEN_FIXED, 6)   \
X(13,  "Date Local Transaction",                FMT_N,   LEN_FIXED, 4)   \
X(14,  "Date Expiration",                       FMT_N,   LEN_FIXED, 4)   \
X(15,  "Date Settlement",                       FMT_N,   LEN_FIXED, 4)   \
X(16,  "Date Conversion",                       FMT_N,   LEN_FIXED, 4)   \
X(17,  "Date Capture",                          FMT_N,   LEN_FIXED, 4)   \
X(18,  "Merchant Type",                         FMT_N,   LEN_FIXED, 4)   \
X(19,  "Acquiring Institution Country Code",    FMT_N,   LEN_FIXED, 3)   \
X(20,  "PAN Extended Country Code",             FMT_N,   LEN_FIXED, 3)   \
X(21,  "Forwarding Institution Country Code",   FMT_N,   LEN_FIXED, 3)   \
X(22,  "Point of Service Entry Mode",           FMT_N,   LEN_FIXED, 3)   \
X(23,  "Card Sequence Number",                  FMT_N,   LEN_FIXED, 3)   \
X(24,  "Network International ID",              FMT_N,   LEN_FIXED, 3)   \
X(25,  "POS Condition Code",                    FMT_N,   LEN_FIXED, 2)   \
X(26,  "POS PIN Capture Code",                  FMT_N,   LEN_FIXED, 2)   \
X(27,  "Authorization ID Response Length",      FMT_N,   LEN_FIXED, 1)   \
X(28,  "Amount Transaction Fee",                FMT_N,   LEN_FIXED, 8)   \
X(29,  "Amount Settlement Fee",                 FMT_N,   LEN_FIXED, 8)   \
X(30,  "Amount Transaction Processing Fee",     FMT_N,   LEN_FIXED, 8)   \
X(31,  "Amount Settlement Processing Fee",      FMT_N,   LEN_FIXED, 8)   \
X(32,  "Acquiring Institution ID Code",         FMT_N,   LEN_LLVAR, 11)  \
X(33,  "Forwarding Institution ID Code",        FMT_N,   LEN_LLVAR, 11)  \
X(34,  "PAN Extended",                          FMT_ANS,  LEN_LLVAR, 28) \
X(35,  "Track 2 Data",                          FMT_Z,   LEN_LLVAR, 37)  \
X(36,  "Track 3 Data",                          FMT_Z,   LEN_LLLVAR,104) \
X(37,  "Retrieval Reference Number",            FMT_AN,  LEN_FIXED, 12)  \
X(38,  "Authorization ID Response",             FMT_AN,  LEN_FIXED, 6)   \
X(39,  "Response Code",                         FMT_AN,  LEN_FIXED, 2)   \
X(40,  "Service Restriction Code",              FMT_AN,  LEN_FIXED, 3)   \
X(41,  "Card Acceptor Terminal ID",             FMT_ANS, LEN_FIXED, 8)   \
X(42,  "Card Acceptor ID Code",                 FMT_ANS, LEN_FIXED, 15)  \
X(43,  "Card Acceptor Name/Location",           FMT_ANS, LEN_FIXED, 40)  \
X(44,  "Additional Response Data",              FMT_AN,  LEN_LLVAR, 25)  \
X(45,  "Track 1 Data",                          FMT_AN,  LEN_LLVAR, 76)  \
X(46,  "Additional Data ISO",                   FMT_AN,  LEN_LLLVAR,999) \
X(47,  "Additional Data National",              FMT_AN,  LEN_LLLVAR,999) \
X(48,  "Additional Data Private",               FMT_AN,  LEN_LLLVAR,999) \
X(49,  "Currency Code Transaction",             FMT_AN,  LEN_FIXED, 3)   \
X(50,  "Currency Code Settlement",              FMT_AN,  LEN_FIXED, 3)   \
X(51,  "Currency Code Billing",                 FMT_AN,  LEN_FIXED, 3)   \
X(52,  "PIN Data",                              FMT_B,   LEN_FIXED, 8)   \
X(53,  "Security Related Control Info",         FMT_N,   LEN_FIXED, 16)  \
X(54,  "Additional Amounts",                    FMT_AN,  LEN_LLLVAR,120) \
X(55,  "ICC Data EMV",                          FMT_B,   LEN_LLLVAR,255) \
X(56,  "Reserved ISO",                          FMT_AN,  LEN_LLLVAR,999) \
X(57,  "Reserved National",                     FMT_AN,  LEN_LLLVAR,999) \
X(58,  "Reserved National",                     FMT_AN,  LEN_LLLVAR,999) \
X(59,  "Reserved National",                     FMT_AN,  LEN_LLLVAR,999) \
X(60,  "Reserved Private",                      FMT_AN,  LEN_LLLVAR,999) \
X(61,  "Reserved Private",                      FMT_AN,  LEN_LLLVAR,999) \
X(62,  "Reserved Private",                      FMT_AN,  LEN_LLLVAR,999) \
X(63,  "Reserved Private",                      FMT_AN,  LEN_LLLVAR,999) \
X(64,  "Message Authentication Code",           FMT_B,   LEN_FIXED, 8)


OOP_CLASS(Iso8583Parser) {
    OOP_EXTENDS(Parser);
};

OOP_CTOR(Iso8583Parser);

OOP_CLASS(Iso8583Packer) {
    OOP_EXTENDS(Packer);
};

OOP_CTOR(Iso8583Packer);

#endif

#endif