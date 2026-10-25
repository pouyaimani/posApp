// #ifndef TXN_EMV_SYNC_PORT_H
// #define TXN_EMV_SYNC_PORT_H

// #include <stdbool.h>
// #include <stddef.h>
// #include <stdint.h>
// #include "txnMgr.h"
// #include "emvMgr.h"
// #include "emv_kahroba.h"

// /**
//  * Fill Kahroba terminal parameters from active transaction/settings.
//  * Set terminal_checks_valid and cvm_result_valid only when those checks
//  * have actually been performed according to your scheme requirements.
//  */
// bool txnEmvBuildTerminal(const TxnMgr* txn, EmvKahrobaTerminal* terminal);

// /** Use the device's approved cryptographically secure random-number source.
// */ bool txnEmvRandom(void* context, uint8_t* bytes, size_t length);

// /** Copy validated result and binary Field 55 into transaction-owned memory.
// */ bool txnEmvImportResult(TxnMgr* txn, const EmvResult* result,
//                         const uint8_t* field55, size_t field55Length);

// /** Map EMV failure to the existing TxnMgr's error/status model. */
// void txnEmvFailTransaction(TxnMgr* txn, EmvOutcome outcome, EmvError error);

// #endif
