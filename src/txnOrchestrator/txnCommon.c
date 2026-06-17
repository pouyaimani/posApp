#include "txnFLow.h"
#include "phrases/phrases.h"
#include "states/states.h"
#include "ui/infoPage.h"

void showConnecting(TxnFlow *f) {
    SHOW_INFO(
        INFO_WAITING,
        phraseGetDef(
            PHRASE_CONNECTIING),
        "");
}

void showSending(TxnFlow *f) {
    SHOW_INFO(
        INFO_WAITING,
        phraseGetDef(
            PHRASE_SENDING_DATA),
        "");
}

void showReceiving(TxnFlow *f) {
    SHOW_INFO(
        INFO_WAITING,
        phraseGetDef(
            PHRASE_RECEIVING_DATA),
        "");
}