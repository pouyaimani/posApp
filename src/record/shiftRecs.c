#include "shiftRecs.h"
#include "embedDB/embedDBsetup.h"
#include "logger.h"
#include "file/file.h"
#include "storage/storage.h"

#define SHIFTS_RECORD_PATH          "/mtd0/shifts_records"
#define SHIFTS_RECORD_IDX_PATH      "/mtd0/shifts_records_idx"
#define SHIFTS_RECORD_TMP           "/mtd0/shifts_record_tmp"
#define PAGE_NUMBER                 8

#define MAX_SHIFT_COUNT             200

static Shifts __shifts;

static embedDBState *state;

// static uint32_t parameters = EMBEDDB_RECORD_LEVEL_CONSISTENCY | (EMBEDDB_USE_BMAP | EMBEDDB_USE_INDEX);

static uint32_t latestIdx;

// Using storage to keep open shifts temporary and insert it 
// into db when shifts is closed
static ShiftData tmpShift;
BEGIN_DSC_ARRAY;
static const DataDescriptor shiftDesc[] = {
    DSC_INT(tmpShift.startDate, 0),
    DSC_INT(tmpShift.startTime, 0),
    DSC_INT(tmpShift.endDate, 0),
    DSC_INT(tmpShift.endTime, 0),
};

static int8_t getLatestFirstTime(uint32_t *latest) {
    if (!state || !latest) return ERR_NOK;
    embedDBIterator it;
    embedDBInitIterator(state, &it);
    uint32_t key;
    uint32_t tmp = 0;
    ShiftData shift;
    it.nextDataPage = 0;
    while (embedDBNext(state, &it, &key, &shift)) {
        if (key > tmp) {
            tmp = key;
        }
    }
    embedDBCloseIterator(&it);
    *latest = tmp;
    return ERR_OK;
}

static void extractLatestIdx() {
    CALL_ONCE(
        getLatestFirstTime(&latestIdx);
    );
}

static int8_t shiftInsert(ShiftData *shift) {
    if (!shift || !state) return ERR_NOK;
    extractLatestIdx();
    if (latestIdx >= MAX_SHIFT_COUNT) {
        return ERR_NOK;
    }
    uint32_t idx = latestIdx + 1;
    int8_t res = embedDBPut(state, &idx, shift);
    if (res != 0) {
        LOG_ERROR("Shift: error in inserting record. error = %d", res);
        return ERR_NOK;
    }
    LOG_DEBUG("shift: idx = %d, startTime = %d, endTime = %d, startDate = %d, endDate = %d",
            idx, shift->startTime, shift->endTime, shift->startDate, shift->endDate);
    latestIdx++;
    return ERR_OK;
}

static int8_t shiftGet(uint32_t index, ShiftData *shift) {
    if (!shift || !state) return ERR_NOK;
    if (index < 0 || index > latestIdx) return ERR_NOK;
    bool found = false;
    embedDBIterator it;
    embedDBInitIterator(state, &it);
    uint32_t key;
    while (embedDBNext(state, &it, &key, shift)) {
        LOG_DEBUG("shift: idx = %d, startTime = %d, endTime = %d, startDate = %d, endDate = %d",
            key, shift->startTime, shift->endTime, shift->startDate, shift->endDate);
    }
    embedDBCloseIterator(&it);
    // if (embedDBGet(state, &index, shift) == 0) {
    //     found = true;
    // }
    return found ? ERR_OK : ERR_NOK;
}

static uint32_t getLatestIdx() {
    return latestIdx;
}


static int8_t getLatest(uint32_t *latest, ShiftData *shift) {
    if (!latest || !shift) return ERR_NOK;
    *latest = latestIdx;
    return shiftGet(latestIdx, shift);
}

static int8_t keep(ShiftData *data) {
    if (!data) return ERR_NOK;
    tmpShift = *data;
    return storage()->save(shiftDesc, sizeof(shiftDesc) / sizeof(DataDescriptor), SHIFTS_RECORD_TMP);
}

static int8_t getKeeped(ShiftData *data) {
    if (!data) return ERR_NOK;
    if (storage()->load(shiftDesc, 
        sizeof(shiftDesc) / sizeof(DataDescriptor), SHIFTS_RECORD_TMP) != ERR_OK) {
            return ERR_NOK;
    }
    data->startDate = tmpShift.startDate;
    data->startTime = tmpShift.startTime;
    data->endDate = tmpShift.endDate;
    data->endTime = tmpShift.endTime;
    return ERR_OK;
}


static int8_t reset() {
    if (!state) return ERR_NOK;
    if (embedDBreset(state, SHIFTS_RECORD_PATH, SHIFTS_RECORD_IDX_PATH) != 0) {
        LOG_DEBUG("Shift: error in reseting database.");
        return ERR_NOK;
    }
    latestIdx = 0;
    return ERR_OK;
}

static int8_t init() {
    uint32_t parameters = EMBEDDB_RECORD_LEVEL_CONSISTENCY
                            | (EMBEDDB_USE_BMAP | EMBEDDB_USE_INDEX);
    if (embedDBSetup(state, SHIFTS_RECORD_PATH, SHIFTS_RECORD_IDX_PATH, sizeof(uint32_t),
             sizeof(ShiftData), 512, 8, parameters) != 0) {
                embedDBClose(state);
                embedDBtearDown(state);
                EMDB_MEM_FREE(state);
                state = NULL;
                LOG_ERROR("Error in setuping embedDB.");
                return ERR_NOK;
    }
    return ERR_OK;
}

OOP_CTOR(Shifts) {
    self->get = shiftGet;
    self->insert = shiftInsert;
    self->reset = reset;
    self->getKeeped = getKeeped;
    self->getLatest = getLatest;
    self->getLatestIdx = getLatestIdx;
    self->keep = keep;
    self->init = init;

    state = (embedDBState *)EMDB_MEM_ALLOC(sizeof(embedDBState));
    if (!state) {
        LOG_ERROR("Shifts: not enough memory for embedDB.");
        return;
    }
}

Shifts *shifts() {
    CALL_ONCE(
        OOP_CALL_CTOR(Shifts, &__shifts);
    );
    return &__shifts;
}