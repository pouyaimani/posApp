// #include "fdb.h"

// /* =========================================================
//  * CRC (simple but consistent)
//  * ========================================================= */
// static uint32_t db_crc32(const uint8_t *data, uint32_t len)
// {
//     uint32_t crc = 0;
//     for (uint32_t i = 0; i < len; i++)
//         crc = (crc * 31) ^ data[i];
//     return crc;
// }

// /* =========================================================
//  * LE helpers
//  * ========================================================= */
// static void write_u16_le(uint8_t *p, uint16_t v)
// {
//     p[0] = (uint8_t)(v);
//     p[1] = (uint8_t)(v >> 8);
// }

// static void write_u32_le(uint8_t *p, uint32_t v)
// {
//     p[0] = (uint8_t)(v);
//     p[1] = (uint8_t)(v >> 8);
//     p[2] = (uint8_t)(v >> 16);
//     p[3] = (uint8_t)(v >> 24);
// }

// static uint32_t read_u32_le(const uint8_t *p)
// {
//     return (uint32_t)p[0]
//         | ((uint32_t)p[1] << 8)
//         | ((uint32_t)p[2] << 16)
//         | ((uint32_t)p[3] << 24);
// }

// /* =========================================================
//  * Frame layout:
//  * [ MAGIC(2) | VER(1) | FLAGS(1) | SIZE(4) | PAYLOAD | CRC(4) | COMMIT(1) ]
//  * ========================================================= */

// #define DB_HEADER_SIZE   8
// #define DB_COMMIT_BYTE   0xA5
// #define DB_MAX_PAYLOAD(io) ((io)->size - DB_FRAME_OVERHEAD)

// /* =========================================================
//  * Encode
//  * ========================================================= */
// int db_record_encode(
//     DbBuffer *buf,
//     const void *payload,
//     uint32_t payload_size,
//     uint8_t version)
// {
//     uint32_t total =
//         DB_HEADER_SIZE + payload_size + 4;

//     if (buf->length + total > buf->capacity)
//         return -1;

//     uint8_t *p = buf->buffer + buf->length;

//     /* HEADER */
//     write_u16_le(p, DB_MAGIC); p += 2;
//     *p++ = version;
//     *p++ = 0; /* flags */
//     write_u32_le(p, payload_size); p += 4;

//     /* PAYLOAD */
//     memcpy(p, payload, payload_size);
//     p += payload_size;

//     /* CRC (header + payload only) */
//     uint32_t crc = db_crc32(
//         buf->buffer + buf->length,
//         DB_HEADER_SIZE + payload_size
//     );

//     write_u32_le(p, crc);
//     p += 4;

//     buf->length += total;
//     return 0;
// }

// /* =========================================================
//  * IO safe primitives
//  * ========================================================= */
// static int db_write_full(
//     DbFileOps *ops,
//     void *ctx,
//     const void *buf,
//     uint32_t size,
//     uint32_t offset)
// {
//     uint32_t written = 0;

//     while (written < size) {
//         int ret = ops->write(
//             ctx,
//             (const uint8_t *)buf + written,
//             size - written,
//             offset + written
//         );

//         if (ret <= 0)
//             return -1;

//         written += (uint32_t)ret;
//     }

//     return 0;
// }

// static int db_read_full(
//     DbFileOps *ops,
//     void *ctx,
//     void *buf,
//     uint32_t size,
//     uint32_t offset)
// {
//     uint32_t read = 0;

//     while (read < size) {
//         int ret = ops->read(
//             ctx,
//             (uint8_t *)buf + read,
//             size - read,
//             offset + read
//         );

//         if (ret <= 0)
//             return -1;

//         read += (uint32_t)ret;
//     }

//     return 0;
// }

// /* =========================================================
//  * Safe header decoder (SINGLE SOURCE OF TRUTH)
//  * ========================================================= */
// static int db_decode_header(
//     const uint8_t *buf,
//     uint8_t *ver,
//     uint32_t *size)
// {
//     uint16_t magic = buf[0] | (buf[1] << 8);
//     if (magic != DB_MAGIC)
//         return -1;

//     *ver = buf[2];

//     *size =
//         buf[4] |
//         (buf[5] << 8) |
//         (buf[6] << 16) |
//         (buf[7] << 24);

//     return 0;
// }

// /* =========================================================
//  * Log init
//  * ========================================================= */
// int db_log_init(
//     DbLog *log,
//     DbFileOps *ops,
//     void *ctx)
// {
//     if (!log || !ops)
//         return -1;

//     if (!ops->read || !ops->write || !ops->sync || !ops->size)
//         return -2;

//     log->ops = *ops;
//     log->ctx = ctx;
//     log->write_offset = 0;

//     return 0;
// }

// /* =========================================================
//  * Append (CRASH SAFE)
//  * ========================================================= */
// int db_log_append(
//     DbLog *log,
//     const void *payload,
//     uint32_t size,
//     uint8_t version,
//     DbIoBuffer *io)
// {
//     if (!log || !payload || !io)
//         return -1;

//     uint32_t required =
//         DB_HEADER_SIZE + size + 4;

//     if (required > io->size)
//         return -2;

//     DbBuffer buf = {
//         .buffer = io->buf,
//         .capacity = io->size,
//         .length = 0
//     };

//     if (db_record_encode(&buf, payload, size, version) != 0)
//         return -3;

//     uint32_t offset = log->write_offset;

//     if (db_write_full(&log->ops, log->ctx,
//                       buf.buffer, buf.length, offset) != 0)
//         return -4;

//     offset += buf.length;

//     uint8_t commit = DB_COMMIT_BYTE;

//     if (db_write_full(&log->ops, log->ctx,
//                       &commit, 1, offset) != 0)
//         return -5;

//     if (log->ops.sync(log->ctx) != 0)
//         return -6;

//     log->write_offset = offset + 1;
//     return 0;
// }

// /* =========================================================
//  * Recovery (STRICT + SAFE)
//  * ========================================================= */
// int db_log_recover(
//     DbLog *log,
//     DbIoBuffer *io)
// {
//     if (!log || !io)
//         return -1;

//     uint32_t offset = 0;
//     uint32_t file_size = log->ops.size(log->ctx);

//     while (offset + DB_HEADER_SIZE <= file_size) {

//         if (db_read_full(
//                 &log->ops,
//                 log->ctx,
//                 io->buf,
//                 DB_HEADER_SIZE,
//                 offset) != 0)
//             break;

//         uint8_t ver;
//         uint32_t size;

//         if (db_decode_header(io->buf, &ver, &size) != 0)
//             break;

//         if (size == 0 || size > io->size - DB_MAX_PAYLOAD)
//             break;

//         uint32_t total = DB_HEADER_SIZE + size + 4;

//         if (offset + total + 1 > file_size)
//             break;

//         if (db_read_full(
//                 &log->ops,
//                 log->ctx,
//                 io->buf,
//                 total,
//                 offset) != 0)
//             break;

//         uint32_t stored_crc = read_u32_le(io->buf + DB_HEADER_SIZE + size);
//         uint32_t calc_crc = db_crc32(io->buf, DB_HEADER_SIZE + size);

//         if (stored_crc != calc_crc)
//             break;

//         uint8_t commit;

//         if (db_read_full(
//                 &log->ops,
//                 log->ctx,
//                 &commit,
//                 1,
//                 offset + total) != 0)
//             break;

//         if (commit != DB_COMMIT_BYTE)
//             break;

//         offset += total + 1;
//     }

//     log->write_offset = offset;
//     return 0;
// }

// /* =========================================================
//  * Iterate (SAFE + CONSISTENT)
//  * ========================================================= */
// int db_log_iterate(
//     DbLog *log,
//     DbIoBuffer *io,
//     int (*cb)(const void*, uint32_t))
// {
//     if (!log || !cb || !io)
//         return -1;

//     uint32_t offset = 0;

//     while (offset + DB_HEADER_SIZE <= log->write_offset) {

//         if (db_read_full(
//                 &log->ops,
//                 log->ctx,
//                 io->buf,
//                 DB_HEADER_SIZE,
//                 offset) != 0)
//             return -2;

//         uint8_t ver;
//         uint32_t size;

//         if (db_decode_header(io->buf, &ver, &size) != 0)
//             return -3;

//         if (size == 0 || size > io->size - DB_MAX_PAYLOAD)
//             return -4;

//         uint32_t total = DB_HEADER_SIZE + size + 4;

//         if (offset + total + 1 > log->write_offset)
//             return -5;

//         if (db_read_full(
//                 &log->ops,
//                 log->ctx,
//                 io->buf,
//                 total,
//                 offset) != 0)
//             return -6;

//         uint32_t stored_crc = read_u32_le(io->buf + DB_HEADER_SIZE + size);
//         uint32_t calc_crc = db_crc32(io->buf, DB_HEADER_SIZE + size);

//         if (stored_crc != calc_crc)
//             return -7;

//         uint8_t commit = io->buf[total];

//         if (commit != DB_COMMIT_BYTE)
//             return -8;

//         if (cb(io->buf + DB_HEADER_SIZE, size) != 0)
//             return -9;

//         offset += total + 1;
//     }

//     return 0;
// }