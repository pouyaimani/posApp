#include "supervisor.h"
#include "iso8583.h"

STATE_DEF_ENTER(KeyInjection) {
    ISO_RESET();
    ISO_SET_MTI("0800");
    ISO_SET_STR(ELEMENT_PROCESSING_CODE, "600000");
    // iso8583()->element[7].data = date + time
    // iso8583()->element[11].data = getStanStr()
    // iso8583()->element[12].data = time
    // iso8583()->element[13].data = date
    // iso8583()->element[24].data =  req.nii
    // iso8583()->element[48].data = getField48BasicInfo().pack()
    // iso8583()->element[53].data = getField53()
    // iso8583()->element[60].data = getCompressPublicKey()
    // iso8583()->element[62].data = getSHA1Hash(req.otp.toByteArray())
}

OOP_CTOR(KeyInjection, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(KeyInjection);
}