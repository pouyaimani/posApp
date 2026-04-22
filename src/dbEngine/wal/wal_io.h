#ifndef WAL_IO_H_
#define WAL_IO_H_

#include <stdint.h>
#include "wal_types.h"

/* ================= SAFE IO ================= */

int wal_write_full(WalOps *ops, void *ctx,
                    const void *buf,
                    uint32_t size,
                    uint32_t offset);

int wal_read_full(WalOps *ops, void *ctx,
                  void *buf,
                  uint32_t size,
                  uint32_t offset);

#endif