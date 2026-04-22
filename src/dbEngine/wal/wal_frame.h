#ifndef WAL_FRAME_H_
#define WAL_FRAME_H_

#include <stdint.h>

/* ================= MAGIC + VERSION ================= */

#define WAL_MAGIC        0xDBDB
#define WAL_VERSION      1
#define WAL_COMMIT_BYTE  0xA5

/* ================= FRAME CONSTANTS ================= */

#define WAL_HEADER_SIZE  8
#define WAL_CRC_SIZE     4
#define WAL_COMMIT_SIZE  1

#define WAL_FLAGS_NONE   0

/* ================= FRAME LAYOUT ================= */

#define WAL_FRAME_OVERHEAD (WAL_HEADER_SIZE + WAL_CRC_SIZE + WAL_COMMIT_SIZE)

/* ================= FRAME RULES =================
 * [MAGIC][VER][FLAGS][SIZE][PAYLOAD][CRC][COMMIT]
 * CRC = version + flags + size + payload
 * ================================================= */

#endif