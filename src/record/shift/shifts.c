#include "shifts.h"
#include "embedDB/embedDBsetup.h"
#include "logger.h"
#include "file/file.h"
#include "storage/storage.h"

#define SHIFTS_RECORD_PATH          "/mtd0/shifts_records"
#define SHIFTS_RECORD_IDX_PATH      "/mtd0/shifts_records_idx"
#define SHIFTS_RECORD_TMP           "/mtd0/shifts_record_tmp"
#define PAGE_NUMBER                 4

static Shifts __shifts;

static embedDBState *state;

// Load latest index in boot and keep it in RAM 
static uint32_t latestIdx;

// Using storage to keep open shifts temporary and insert it 
// into db when shifts is closed
static ShiftData tmpShift;
BEGIN_DSC_ARRAY;
static const DataDescriptor shiftDesc[] = {
    DSC_INT(tmpShift.startTime, 0),
    DSC_INT(tmpShift.startTime, 0),
    DSC_INT(tmpShift.endDate, 0),
    DSC_INT(tmpShift.endDate, 0),
};

static int shiftInsert(ShiftData *shift) {
    if (embedDBSetup(state, SHIFTS_RECORD_PATH, SHIFTS_RECORD_IDX_PATH, sizeof(uint32_t),
             sizeof(ShiftData), PAGE_SIZE_512, PAGE_NUMBER) != 0) {
                embedDBtearDown(state);
                EMDB_MEM_FREE(state);
                LOG_ERROR("Error in setuping embedDB.");
                return -1;
    }
    latestIdx++;
    LOG_ERROR("EmbedDB max key = %d.", latestIdx);
    int8_t res = embedDBPut(state, &latestIdx, shift);
    if (res != 0) {
        LOG_ERROR("Shift: error in inserting record. error = %d", res);
        return -1;
    }
    LOG_DEBUG("shift: idx = %d, startTime = %d, endTime = %d, startDate = %d, endDate = %d",
            latestIdx, shift->startTime, shift->endTime, shift->startDate, shift->endDate);
    embedDBClose(state);
    embedDBtearDown(state);
    return 0;
}

static int shiftGet(uint32_t index, ShiftData *shift) {
    if (embedDBSetup(state, SHIFTS_RECORD_PATH, SHIFTS_RECORD_IDX_PATH, sizeof(uint32_t),
             sizeof(ShiftData), PAGE_SIZE_512, PAGE_NUMBER) != 0) {
                embedDBtearDown(state);
                EMDB_MEM_FREE(state);
                LOG_ERROR("Error in setuping embedDB.");
                return -1;
    }
    bool found = false;
    if (embedDBGet(state, &index, shift) == 0) {
        found = true;
    }
    embedDBClose(state);
    embedDBtearDown(state);
    return found ? 0 : -1;
}

static uint32_t getLatestIdx() {
    return latestIdx;
}

static int getLatest(uint32_t *latest, ShiftData *shift) {
    return shiftGet(latestIdx, shift);
}

static uint32_t getLatestFirstTime() {
    if (embedDBSetup(state, SHIFTS_RECORD_PATH, SHIFTS_RECORD_IDX_PATH, sizeof(uint32_t),
             sizeof(ShiftData), PAGE_SIZE_512, PAGE_NUMBER) != 0) {
                embedDBtearDown(state);
                EMDB_MEM_FREE(state);
                LOG_ERROR("Error in setuping embedDB.");
                return -1;
    }
    uint32_t latest = embedDBGetLatestKey32(state);
    embedDBClose(state);
    embedDBtearDown(state);
    return;
}

static int keep(ShiftData *data) {
    tmpShift.startDate = data->startDate;
    tmpShift.startTime = data->startTime;
    tmpShift.endDate = data->endDate;
    tmpShift.endTime = data->endTime;
    storage()->save(shiftDesc, sizeof(shiftDesc) / sizeof(DataDescriptor), SHIFTS_RECORD_TMP);
}

static int getKeeped(ShiftData *data) {
    storage()->load(shiftDesc, sizeof(shiftDesc) / sizeof(DataDescriptor), SHIFTS_RECORD_TMP);
    data->startDate = tmpShift.startDate;
    data->startTime = tmpShift.startTime;
    data->endDate = tmpShift.endDate;
    data->endTime = tmpShift.endTime;
    return 0;
}


void reset() {
    OOP_CALL(file(), remove, SHIFTS_RECORD_PATH);
    OOP_CALL(file(), remove, SHIFTS_RECORD_IDX_PATH);
}

OOP_CTOR(Shifts) {
    self->get = shiftGet;
    self->insert = shiftInsert;
    self->reset = reset;
    self->getKeeped = getKeeped;
    self->getLatest = getLatest;
    self->getLatestIdx = getLatestIdx;
    self->keep = keep;

    state = (embedDBState *)EMDB_MEM_ALLOC(sizeof(embedDBState));
    if (!state) {
        LOG_DEBUG("Shifts: not enough memory for embedDB.");
        return -1;
    }

    latestIdx = getLatestFirstTime();
}

Shifts *shifts() {
    CALL_ONCE(
        OOP_CALL_CTOR(Shifts, &__shifts);
    );
    return &__shifts;
}