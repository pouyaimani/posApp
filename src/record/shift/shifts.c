#include "shifts.h"
#include "embedDB/embedDBsetup.h"
#include "logger.h"
#include "file/file.h"

#define SHIFTS_RECORD_PATH   "/mtd0/shifts_records"
#define RECORDS_PER_PAGE    50
#define PAGE_NUMBER         4

static Shifts __shifts;
static embedDBState *state;

static int shiftInsert(uint32_t index, ShiftData *shift) {
    if (embedDBSetup(state, SHIFTS_RECORD_PATH, sizeof(uint32_t),
             sizeof(ShiftData), PAGE_NUMBER, RECORDS_PER_PAGE) != 0) {
                embedDBtearDown(state);
                EMDB_MEM_FREE(state);
                LOG_ERROR("Error in setuping embedDB.");
                return -1;
    }
    int8_t res = embedDBPut(state, &index, shift);
    if (res != 0) {
        LOG_ERROR("Shift: error in inserting record. error = %d", res);
        return -1;
    }
    // if (embedDBFlush(state) != 0) {
    //     LOG_ERROR("Shift: error in flushing record");
    // }
    embedDBClose(state);
    embedDBtearDown(state);
    return 0;
}

static int shiftGet(uint32_t index, ShiftData *shift) {
    if (embedDBSetup(state, SHIFTS_RECORD_PATH, sizeof(uint32_t),
             sizeof(ShiftData), PAGE_SIZE_512, PAGE_NUMBER) != 0) {
                embedDBtearDown(state);
                EMDB_MEM_FREE(state);
                LOG_ERROR("Error in setuping embedDB.");
                return -1;
    }
    embedDBIterator it;
    memset(&it, 0, sizeof(it));

    embedDBInitIterator(state, &it);
    uint32_t key;
    bool found = false;
    while (embedDBNext(state, &it, &key, shift)) {
        if (key == index) {
            // LOG_DEBUG("shift is found...");
            found = true;
            break;
        }
    }
    if (!found) {
        // LOG_DEBUG("shift is not found...");
    }
    embedDBCloseIterator(&it);
    embedDBClose(state);
    embedDBtearDown(state);
    return found ? 0 : -1;
}

void reset() {
    OOP_CALL(file(), remove, SHIFTS_RECORD_PATH);
}

OOP_CTOR(Shiftss) {
    self->init = NULL;
    self->get = shiftGet;
    self->insert = shiftInsert;
    self->reset = reset;

    state = (embedDBState *)EMDB_MEM_ALLOC(sizeof(embedDBState));
    if (!state) {
        LOG_DEBUG("Shifts: not enough memory for embedDB.");
        return -1;
    }
}

Shiftss *shifts() {
    CALL_ONCE(
        OOP_CALL_CTOR(Shiftss, &__shifts);
    );
    return &__shifts;
}