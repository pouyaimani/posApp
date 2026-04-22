#include "wal.h"

static uint32_t wal_crc32(uint8_t version,
                          uint8_t flags,
                          uint32_t size,
                          const uint8_t *payload)
{
    uint32_t crc = 0;

    crc = (crc * 31) ^ version;
    crc = (crc * 31) ^ flags;
    crc = (crc * 31) ^ (size & 0xFF);
    crc = (crc * 31) ^ ((size >> 8) & 0xFF);
    crc = (crc * 31) ^ ((size >> 16) & 0xFF);
    crc = (crc * 31) ^ ((size >> 24) & 0xFF);

    for (uint32_t i = 0; i < size; i++)
        crc = (crc * 31) ^ payload[i];

    return crc;
}

static int wal_encode_frame(uint8_t *p,
                            const void *payload,
                            uint32_t size,
                            uint8_t version)
{
    uint32_t crc = wal_crc32(version, 0, size, payload);

    /* HEADER */
    p[0] = WAL_MAGIC & 0xFF;
    p[1] = WAL_MAGIC >> 8;
    p[2] = version;
    p[3] = WAL_FLAGS_NONE;

    p[4] = size & 0xFF;
    p[5] = (size >> 8) & 0xFF;
    p[6] = (size >> 16) & 0xFF;
    p[7] = (size >> 24) & 0xFF;

    /* PAYLOAD */
    memcpy(p + WAL_HEADER_SIZE, payload, size);

    /* CRC */
    memcpy(p + WAL_HEADER_SIZE + size, &crc, 4);

    return 0;
}

static int wal_write_full(
    WalOps *ops,
    void *ctx,
    const void *buf,
    uint32_t size,
    uint32_t offset)
{
    uint32_t written = 0;

    while (written < size) {
        int ret = ops->write(
            ctx,
            (const uint8_t *)buf + written,
            size - written,
            offset + written
        );

        if (ret <= 0)
            return -1;

        written += (uint32_t)ret;
    }

    return 0;
}

static int wal_read_full(
    WalOps *ops,
    void *ctx,
    void *buf,
    uint32_t size,
    uint32_t offset)
{
    uint32_t read = 0;

    while (read < size) {
        int ret = ops->read(
            ctx,
            (uint8_t *)buf + read,
            size - read,
            offset + read
        );

        if (ret <= 0)
            return -1;

        read += (uint32_t)ret;
    }

    return 0;
}

int wal_append(WalLog *log,
               const void *payload,
               uint32_t size,
               uint8_t version,
               WalIoBuffer *io)
{
    uint32_t frame_size = WAL_HEADER_SIZE + size + 4;

    if (frame_size + 1 > io->size)
        return -1;

    uint8_t *buf = io->buf;

    wal_encode_frame(buf, payload, size, version);

    uint32_t offset = log->write_offset;

    if (wal_write_full(&log->ops, log->ctx,
                       buf, frame_size, offset) != 0)
        return -2;

    uint8_t commit = WAL_COMMIT_BYTE;

    if (wal_write_full(&log->ops, log->ctx,
                       &commit, 1,
                       offset + frame_size) != 0)
        return -3;

    if (log->ops.sync(log->ctx) != 0)
        return -4;

    log->write_offset = offset + frame_size + 1;

    return 0;
}

int wal_recover(WalLog *log, WalIoBuffer *io)
{
    uint32_t offset = 0;
    uint32_t file_size = log->ops.size(log->ctx);

    while (offset + WAL_HEADER_SIZE < file_size)
    {
        if (wal_read_full(&log->ops, log->ctx,
                          io->buf,
                          WAL_HEADER_SIZE,
                          offset) != 0)
        {
            offset++;
            continue;
        }

        uint16_t magic = io->buf[0] | (io->buf[1] << 8);

        if (magic != WAL_MAGIC)
        {
            offset++;
            continue;
        }

        uint32_t size =
            io->buf[4] |
            (io->buf[5] << 8) |
            (io->buf[6] << 16) |
            (io->buf[7] << 24);

        uint32_t frame = WAL_FRAME_OVERHEAD + size;

        if (offset + frame + 1 > file_size)
            break;

        if (wal_read_full(&log->ops, log->ctx,
                          io->buf,
                          frame + 1,
                          offset) != 0)
        {
            offset++;
            continue;
        }

        uint32_t stored_crc;
        memcpy(&stored_crc,
               io->buf + WAL_HEADER_SIZE + size,
               4);

        uint32_t calc_crc =
            wal_crc32(io->buf[2],
                      io->buf[3],
                      size,
                      io->buf + WAL_HEADER_SIZE);

        if (stored_crc != calc_crc)
        {
            offset++;
            continue;
        }

        if (io->buf[frame] != WAL_COMMIT_BYTE)
        {
            offset++;
            continue;
        }

        offset += frame + 1;
    }

    log->write_offset = offset;
    return 0;
}

void wal_iter_init(WalIterator *it, WalLog *log, WalIoBuffer *io)
{
    it->log = log;
    it->io = io;
    it->offset = 0;
    it->file_size = log->ops.size(log->ctx);
}

bool wal_iter_next(WalIterator *it, WalIterRecord *out)
{
    while (it->offset < it->file_size)
    {
        uint8_t header[8];

        if (it->log->ops.read(it->log->ctx, header, 8, it->offset) != 0)
        {
            it->offset++;
            continue;
        }

        uint16_t magic = header[0] | (header[1] << 8);
        if (magic != WAL_MAGIC)
        {
            it->offset++;
            continue;
        }

        uint32_t size = header[4]
            | (header[5] << 8)
            | (header[6] << 16)
            | (header[7] << 24);

        uint32_t frame = 8 + size + 4 + 1;

        if (it->offset + frame > it->file_size)
        {
            return false;
        }

        if (it->log->ops.read(it->log->ctx, it->io->buf, frame, it->offset) != 0)
        {
            it->offset++;
            continue;
        }

        uint8_t commit = it->io->buf[frame - 1];
        if (commit != WAL_COMMIT_BYTE)
        {
            it->offset++;
            continue;
        }

        uint32_t payload_size = size;

        out->payload = it->io->buf + 8;
        out->size = payload_size;
        out->offset = it->offset;
        out->frame_size = frame;

        it->offset += frame;
        return true;
    }

    return false;
}