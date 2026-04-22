#ifndef FDB_H_
#define FDB_H_

#include <stdint.h>
#include <string.h>

//  TODO: improvements:
//    1- make little/big endian safe


/* ================= ENCODE/DECODE LAYER ================= */

#define DB_MAGIC 0xDBDB

typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint8_t  version;
    uint8_t  flags;
    uint32_t size;
} DbRecordHeader;

typedef struct {
    uint32_t crc;
} DbRecordFooter;

typedef struct {
    uint8_t *buffer;
    uint32_t capacity;
    uint32_t length;
} DbBuffer;

/* ================= APPEND LOG ENGINE ================= */

typedef struct {
    int (*open)(void *ctx, const char *path);
    int (*read)(void *ctx, void *buf, uint32_t size, uint32_t offset);
    int (*write)(void *ctx, const void *buf, uint32_t size, uint32_t offset);
    int (*sync)(void *ctx);
    uint32_t (*size)(void *ctx);
} DbFileOps;

typedef struct {
    uint8_t *buf;
    uint32_t size;
} DbIoBuffer;

typedef struct {
    DbFileOps ops;
    void *ctx;
    uint32_t write_offset;
} DbLog;

int db_log_open(DbLog *log, const char *path);

int db_log_append(
    DbLog *log,
    const void *payload,
    uint32_t size,
    uint8_t version,
    DbIoBuffer *io);

int db_log_recover(
    DbLog *log,
    DbIoBuffer *io);

int db_log_iterate(
    DbLog *log,
    DbIoBuffer *io,
    int (*cb)(const void*, uint32_t));


#endif