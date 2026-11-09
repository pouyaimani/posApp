#ifndef PNA_TMS_PROTOCOL_H_
#define PNA_TMS_PROTOCOL_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* PNA's legacy TCP protocol. Not exposed through the common TmsMgr API. */
#define PNA_TMS_MAX_FILES        40u  /* Document says 10; legacy accepts 40. */
#define PNA_TMS_SECTOR_SIZE      4096u
#define PNA_TMS_MAX_SECTORS      10000u /* Four ASCII decimal sector digits. */
#define PNA_TMS_MAX_FILE_SIZE    (PNA_TMS_MAX_SECTORS * PNA_TMS_SECTOR_SIZE)
#define PNA_TMS_FILE_NAME_SIZE   16u
#define PNA_TMS_MD5_SIZE         16u
#define PNA_TMS_MAX_FRAME_SIZE   4111u
#define PNA_TMS_RESUME_SIZE      193u
#define PNA_TMS_FILE_REQ_SIZE    27u
#define PNA_TMS_END_SIZE         7u

/* Raw padded fields: NO embedded automatic NUL terminators. */
typedef struct {
    uint8_t model[16];
    uint8_t serial[30];
    uint8_t terminal[8];
    uint8_t appVersion[16];
    uint8_t biosVersion[16];
    uint8_t hwInfo[100];
} PnaTmsIdentity;

typedef struct {
    uint8_t name[PNA_TMS_FILE_NAME_SIZE];
    uint32_t size;
    uint8_t md5[PNA_TMS_MD5_SIZE]; /* Raw manifest bytes, NOT assumed hex. */
} PnaTmsFile;

typedef struct {
    uint16_t index;
    uint16_t length;
    const uint8_t* bytes; /* Borrowed pointer into the input frame. */
} PnaTmsSector;

typedef struct {
    uint8_t lengthField[2];
    uint8_t message[64];
    size_t messageLength;
} PnaTmsServerError;

typedef enum {
    PNA_PROTO_OK = 0,
    PNA_PROTO_ARGUMENT,
    PNA_PROTO_CAPACITY,
    PNA_PROTO_FRAME,
    PNA_PROTO_LRC,
    PNA_PROTO_COMMAND,
    PNA_PROTO_FIELD
} PnaProtoResult;

typedef enum {
    PNA_CMD_RESUME = 10,
    PNA_CMD_MANIFEST = 11,
    PNA_CMD_FILE_REQUEST = 12,
    PNA_CMD_ERROR = 13,
    PNA_CMD_SECTOR = 21,
    PNA_CMD_END = 22
} PnaTmsCommand;

/* Fill identity with ASCII space padding, then copy each fixed-width field. */
void pnaTmsIdentityClear(PnaTmsIdentity* identity);

/* Returns total packet size (including 2-byte prefix). */
PnaProtoResult pnaTmsFrameSize(const uint8_t prefix[2], size_t* frameSize);
PnaProtoResult pnaTmsValidate(const uint8_t* frame, size_t len,
                              PnaTmsCommand* cmd);
PnaProtoResult pnaTmsBuildResume(const PnaTmsIdentity* identity,
                                 uint8_t* output, size_t capacity,
                                 size_t* written);
PnaProtoResult pnaTmsBuildSectorRequest(const uint8_t name[16],
                                        uint16_t sectorIndex,
                                        uint8_t* output, size_t capacity,
                                        size_t* written);
PnaProtoResult pnaTmsParseManifest(const uint8_t* frame, size_t len,
                                   PnaTmsFile* files, size_t capacity,
                                   size_t* count);
PnaProtoResult pnaTmsParseSector(const uint8_t* frame, size_t len,
                                 PnaTmsSector* sector);
PnaProtoResult pnaTmsParseError(const uint8_t* frame, size_t len,
                                PnaTmsServerError* error);
PnaProtoResult pnaTmsParseEnd(const uint8_t* frame, size_t len);

#endif
