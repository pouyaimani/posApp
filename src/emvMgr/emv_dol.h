#ifndef EMV_DOL_H
#define EMV_DOL_H

#include "emv_tag_db.h"

/**
 * Serialize DOL (PDOL / CDOL) into raw concatenated values.
 * Missing optional tags are zero-filled; missing critical terminal tags fail.
 * A DOL contains tag+one-byte requested-length entries, NOT TLV values.
 */
EmvError emvDolBuild(const uint8_t* dol, size_t dol_len, const EmvTagDb* tags,
                     uint8_t* out, size_t capacity, size_t* written);

#endif
