#include "shiftRecs.h"
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

static int8_t shiftInsert(ShiftData *shift) {
    if (embedDBSetup(state, SHIFTS_RECORD_PATH, SHIFTS_RECORD_IDX_PATH, sizeof(uint32_t),
             sizeof(ShiftData), PAGE_SIZE_512, PAGE_NUMBER) != 0) {
                embedDBtearDown(state);
                EMDB_MEM_FREE(state);
                LOG_ERROR("Error in setuping embedDB.");
                return -1;
    }
    uint32_t idx = latestIdx + 1;
    LOG_ERROR("EmbedDB max key = %d.", idx);
    int8_t res = embedDBPut(state, &idx, shift);
    if (res != 0) {
        LOG_ERROR("Shift: error in inserting record. error = %d", res);
        return -1;
    }
    LOG_DEBUG("shift: idx = %d, startTime = %d, endTime = %d, startDate = %d, endDate = %d",
            idx, shift->startTime, shift->endTime, shift->startDate, shift->endDate);
    embedDBClose(state);
    embedDBtearDown(state);
    latestIdx++;
    return 0;
}

static int8_t shiftGet(uint32_t index, ShiftData *shift) {
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

static int8_t getLatest(uint32_t *latest, ShiftData *shift) {
    return shiftGet(latestIdx, shift);
}

static int8_t getLatestFirstTime(uint32_t *latest) {
    if (embedDBSetup(state, SHIFTS_RECORD_PATH, SHIFTS_RECORD_IDX_PATH, sizeof(uint32_t),
             sizeof(ShiftData), PAGE_SIZE_512, PAGE_NUMBER) != 0) {
                embedDBtearDown(state);
                EMDB_MEM_FREE(state);
                LOG_ERROR("Error in setuping embedDB.");
                return -1;
    }
    *latest = embedDBGetLatestKey32(state);
    embedDBClose(state);
    embedDBtearDown(state);
    return 0;
}

static int8_t keep(ShiftData *data) {
    tmpShift.startDate = data->startDate;
    tmpShift.startTime = data->startTime;
    tmpShift.endDate = data->endDate;
    tmpShift.endTime = data->endTime;
    storage()->save(shiftDesc, sizeof(shiftDesc) / sizeof(DataDescriptor), SHIFTS_RECORD_TMP);
}

static int8_t getKeeped(ShiftData *data) {
    storage()->load(shiftDesc, sizeof(shiftDesc) / sizeof(DataDescriptor), SHIFTS_RECORD_TMP);
    data->startDate = tmpShift.startDate;
    data->startTime = tmpShift.startTime;
    data->endDate = tmpShift.endDate;
    data->endTime = tmpShift.endTime;
    return 0;
}


static int8_t reset() {
    if (embedDBreset(state, SHIFTS_RECORD_PATH, SHIFTS_RECORD_IDX_PATH, sizeof(uint32_t),
             sizeof(ShiftData), PAGE_SIZE_512, PAGE_NUMBER) != 0) {
                LOG_ERROR("Error in setuping embedDB.");
                return -1;
    }
    return 0;
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
        LOG_ERROR("Shifts: not enough memory for embedDB.");
        return -1;
    }
    uint32_t idx;
    if (getLatestFirstTime(&idx) == 0) {
        LOG_ERROR("Shifts: latest index = %d.", idx);
        latestIdx = idx;
    } else {
        LOG_ERROR("Shifts: could not get latest index of shifts.");
        latestIdx = 0;
    }
}

Shifts *shifts() {
    CALL_ONCE(
        OOP_CALL_CTOR(Shifts, &__shifts);
    );
    return &__shifts;
}