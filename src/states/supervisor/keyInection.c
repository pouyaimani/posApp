#include "supervisor.h"
#include "iso8583.h"
#include "ped/ped.h"
#include "common.h"
#include "states/states.h"
#include "phrases/phrases.h"
#include "ui/infoPage.h"

int8_t injectOffline() {
    	// 8F161C2B5F0E891FD943013BC2D85909
	unsigned char masterKey[16] = { 0x8F, 0x16, 0x1C, 0x2B, 0x5F, 0x0E, 0x89, 0x1F,
									0xD9, 0x43, 0x01, 0x3B, 0xC2, 0xD8, 0x59, 0x09};
	// FB5AE17356401A273D40FC4693373960
	unsigned char logonMacKey[16] = { 0xFB, 0x5A, 0xE1, 0x73, 0x56, 0x40, 0x1A, 0x27, 
									  0x3D, 0x40, 0xFC, 0x46, 0x93, 0x37, 0x39, 0x60};

    PedErr_t err = ped()->injectMasterKey(masterKey, sizeof(masterKey));
    RETURN_VALUE_IF_NOT(err, PED_ERR_OK, ;, ERR_NOK);
    err = ped()->injectMacKey(logonMacKey, sizeof(logonMacKey));
    RETURN_VALUE_IF_NOT(err, PED_ERR_OK, ;, ERR_NOK);
	return ERR_OK;
}

STATE_DEF_ENTER(KeyInjection) {
    int8_t err = injectOffline();
    Phrases_t id = err == ERR_OK ? 
                        PHRASE_KEY_INJ_SUCCEED : PHRASE_KEY_INJ_FAILED;
    int8_t infoType = err == ERR_OK ? 
                        INFO_SUCCESS : INFO_ERROR;                        
    GOTO_INFO(state->parent, state->parent, infoType, phraseGetDef(id), "");
}

OOP_CTOR(KeyInjection, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(KeyInjection);
}