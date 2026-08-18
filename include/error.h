#ifndef ERROR_H_
#define ERROR_H_

#include "logger.h"

#define VAR_UNUSED(x) ((void)x)

/**********************************************************/
//                         ERRORS
/**********************************************************/

typedef int8_t Error_t;

#define ERR_OK                0
#define ERR_NOK               -1
#define ERR_NOT_SUPPORTED     -2
#define ERR_BAD_PARAMETER     -3
#define ERR_NULL_PARAMETER    -4
#define ERR_MEMORY_ALLOCATION -5

#define RETURN_VALUE_IF_NOT(expr, expected, on_error, retval)                  \
    do {                                                                       \
        __typeof__(expr)     __val = (expr);                                   \
        __typeof__(expected) __exp = (expected);                               \
                                                                               \
        if (__val != __exp) {                                                  \
            LOG_ERROR("Error: %s returns %d (expected %d)", #expr, (int)__val, \
                      (int)__exp);                                             \
                                                                               \
            on_error;                                                          \
            return (retval);                                                   \
        }                                                                      \
    } while (0)

#define RETURN_VALUE_IF(expr, expected, on_error, retval)                      \
    do {                                                                       \
        __typeof__(expr)     __val = (expr);                                   \
        __typeof__(expected) __exp = (expected);                               \
                                                                               \
        if (__val == __exp) {                                                  \
            LOG_ERROR("Error: %s returns %d (expected %d)", #expr, (int)__val, \
                      (int)__exp);                                             \
                                                                               \
            on_error;                                                          \
            return (retval);                                                   \
        }                                                                      \
    } while (0)

#define RETURN_VALUE_IF_GREATER(expr, expected, on_error, retval)              \
    do {                                                                       \
        __typeof__(expr)     __val = (expr);                                   \
        __typeof__(expected) __exp = (expected);                               \
                                                                               \
        if (__val > __exp) {                                                   \
            LOG_ERROR("Error: %s returns %d (expected %d)", #expr, (int)__val, \
                      (int)__exp);                                             \
                                                                               \
            on_error;                                                          \
            return (retval);                                                   \
        }                                                                      \
    } while (0)

#define RETURN_VALUE_IF_LIITLE(expr, expected, on_error, retval)               \
    do {                                                                       \
        __typeof__(expr)     __val = (expr);                                   \
        __typeof__(expected) __exp = (expected);                               \
                                                                               \
        if (__val < __exp) {                                                   \
            LOG_ERROR("Error: %s returns %d (expected %d)", #expr, (int)__val, \
                      (int)__exp);                                             \
                                                                               \
            on_error;                                                          \
            return (retval);                                                   \
        }                                                                      \
    } while (0)

#define RETURN_VALUE_IF_GE(expr, expected, on_error, retval)                   \
    do {                                                                       \
        __typeof__(expr)     __val = (expr);                                   \
        __typeof__(expected) __exp = (expected);                               \
                                                                               \
        if (__val >= __exp) {                                                  \
            LOG_ERROR("Error: %s returns %d (expected %d)", #expr, (int)__val, \
                      (int)__exp);                                             \
                                                                               \
            on_error;                                                          \
            return (retval);                                                   \
        }                                                                      \
    } while (0)

#define RETURN_VALUE_IF_LE(expr, expected, on_error, retval)                   \
    do {                                                                       \
        __typeof__(expr)     __val = (expr);                                   \
        __typeof__(expected) __exp = (expected);                               \
                                                                               \
        if (__val <= __exp) {                                                  \
            LOG_ERROR("Error: %s returns %d (expected %d)", #expr, (int)__val, \
                      (int)__exp);                                             \
                                                                               \
            on_error;                                                          \
            return (retval);                                                   \
        }                                                                      \
    } while (0)

#define RETURN_IF_NOT(expr, expected, on_error)                                \
    do {                                                                       \
        __typeof__(expr)     __val = (expr);                                   \
        __typeof__(expected) __exp = (expected);                               \
                                                                               \
        if (__val != __exp) {                                                  \
            LOG_ERROR("Error: %s returns %d (expected %d)", #expr, (int)__val, \
                      (int)__exp);                                             \
                                                                               \
            on_error;                                                          \
            return;                                                            \
        }                                                                      \
    } while (0)

#define RETURN_IF(expr, expected, on_error)                                    \
    do {                                                                       \
        __typeof__(expr)     __val = (expr);                                   \
        __typeof__(expected) __exp = (expected);                               \
                                                                               \
        if (__val == __exp) {                                                  \
            LOG_ERROR("Error: %s returns %d (expected %d)", #expr, (int)__val, \
                      (int)__exp);                                             \
                                                                               \
            on_error;                                                          \
            return;                                                            \
        }                                                                      \
    } while (0)

#define RETURN_VALUE_IF_NULL(ptr, on_error, retval)                            \
    do {                                                                       \
        if ((ptr) == NULL) {                                                   \
            LOG_ERROR("Error: %s is NULL", #ptr);                              \
            on_error;                                                          \
            return (retval);                                                   \
        }                                                                      \
    } while (0)

#define RETURN_IF_NULL(ptr, on_error)                                          \
    do {                                                                       \
        if ((ptr) == NULL) {                                                   \
            LOG_ERROR("Error: %s is NULL", #ptr);                              \
            on_error;                                                          \
            return;                                                            \
        }                                                                      \
    } while (0)

/*********************************************************************************************
 *                                                                                           *
 *                                      General Errors
 *                                                                                           *
 ********************************************************************************************/

typedef enum {
    ERR_DSC_OK = 0,

    /* Generic */
    ERR_DSC_UNKNOWN,
    ERR_DSC_INVALID_ARG,
    ERR_DSC_INVALID_STATE,
    ERR_DSC_NOT_SUPPORTED,
    ERR_DSC_TIMEOUT,
    ERR_DSC_CANCELLED,
    ERR_DSC_NO_SPACE,
    ERR_DSC_ALREADY_EXISTS,
    ERR_DSC_BAD_PARAMETER,
    ERR_DSC_NOT_FOUND,
    ERR_DSC_IS_NOT_INIT,

    /* Device */
    ERR_DSC_DEVICE,
    ERR_DSC_BUSY,

    /* Communication */
    ERR_DSC_COMMUNICATION,
    ERR_DSC_NETWORK,
    ERR_DSC_HOST,

    /* Payment */
    ERR_DSC_CARD,
    ERR_DSC_PIN,
    ERR_DSC_EMV,
    ERR_DSC_TRANSACTION_DECLINED,

    /* Security */
    ERR_DSC_SECURITY,
    ERR_DSC_AUTH,

    /* Storage */
    ERR_DSC_STORAGE,
    ERR_DSC_MEMORY,
    ERR_DSC_DATABASE,

    /* Hardware */
    ERR_DSC_PRINTER,
    ERR_DSC_DISPLAY,
    ERR_DSC_PED,
    ERR_DSC_SCANNER,
    ERR_DSC_CELLULAR,
    ERR_DSC_WIFI,

    /* System */
    ERR_DSC_SYSTEM,
    ERR_DSC_FATAL

} ErrorDsc_t;

/*********************************************************************************************
 *                                                                                           *
 *                                      Printer Errors
 *                                                                                           *
 ********************************************************************************************/
typedef enum PrinterErr_t {
    PRNT_ERR_OK,
    PRNT_ERR_NOK,
    PRNT_ERR_INPUT,
    PRNT_ERR_OVER_HEAT,
    PRNT_ERR_NO_PAPER,
    PRNT_ERR_TIME_OUT
} PrinterErr_t;

/*********************************************************************************************
 *                                                                                           *
 *                                      Database Errors
 *                                                                                           *
 ********************************************************************************************/
typedef enum DataBase_t {
    DB_ERR_OK,
    DB_ERR_NOK,
    DB_ERR_SETUP_FAILURE,
    DB_ERR_CREATING_SCHEMA_FAILURE,
    DB_ERR_CREATING_OP_FAILURE,
    DB_ERR_RESET_FAILURE,
    DB_ERR_INSERT_FAILURE
} DataBase_t;

/*********************************************************************************************
 *                                                                                           *
 *                                      Ped Errors
 *                                                                                           *
 ********************************************************************************************/
typedef enum PedErr_t { PED_ERR_OK, PED_ERR_INPUT } PedErr_t;

/*********************************************************************************************
 *                                                                                           *
 *                                      Magreader Errors
 *                                                                                           *
 ********************************************************************************************/
typedef enum MagReaderErr_t {
    MAG_ERR_SWIPED,
    MAG_ERR_OPEN_FAILED,
    MAG_ERR_NO_SWIPE,
    MAG_ERR_DATA_ERR,
    MAG_ERR_TIME_OUT,
    MAG_ERR_OTHER

} MagReaderErr_t;

/*********************************************************************************************
 *                                                                                           *
 *                                      Cellular Errors
 *                                                                                           *
 ********************************************************************************************/
typedef enum CellErr_t {
    CELL_ERR_OK,
    CELL_ERR_INIT,
    CELL_ERR_SIM_ERROR,
    CELL_ERR_OTHER
} CellErr_t;

/*********************************************************************************************
 *                                                                                           *
 *                                      Network Errors
 *                                                                                           *
 ********************************************************************************************/
typedef enum { NET_ERR_OK = 0, NET_ERR_NOK, NET_ERR_INPUT_ERR } NetError_t;

/*********************************************************************************************
 *                                                                                           *
 *                                      File Errors
 *                                                                                           *
 ********************************************************************************************/
typedef enum {
    FILE_ERR_NONE,
    FILE_ERR_CRCERR,
    FILE_ERROR,
    FILE_SEEK_ERROR,
    FILE_ERR_EOF,
    FILE_ERR_OK
} FileErr_t;

/*********************************************************************************************
 *                                                                                           *
 *                                      Wifi Errors
 *                                                                                           *
 ********************************************************************************************/

typedef enum WifiErr_t {
    WIFI_ERR_OK,
    WIFI_ERR_INIT,
    WIFI_ERR_NOK,
    WIFI_ERR_INPUT,
    WIFI_ERR_TIME_OUT,
    WIFI_ERR_CONNECT_FAILED
} WifiErr_t;

/*********************************************************************************************
 *                                                                                           *
 *                                          Results
 *                                                                                           *
 ********************************************************************************************/

typedef struct {
    ErrorDsc_t err;
    union {
        PedErr_t       ped;
        PrinterErr_t   printer;
        FileErr_t      file;
        NetError_t     net;
        CellErr_t      cellular;
        MagReaderErr_t mag;
        DataBase_t     db;
        WifiErr_t      wifi;
        int            raw;
    } detail;
} Result_t;

#endif