#ifndef EMV_FIELD55_H
#define EMV_FIELD55_H

#include "emv_tag_db.h"

typedef struct {
    EmvTag tag;      /* EMV tag identifier (e.g. 0x9F26). */
    bool   required; /* Missing required tags cause an error. */
} EmvField55Spec;

/**
 * Build binary BER-TLV host Field 55 from an explicit acquirer-approved tag
 * list. Never assume a universal tag list; account for network/host
 * specifications. The builder does not include PAN/track data unless explicitly
 * requested.
 */
EmvError emvField55Build(const EmvTagDb* tags, const EmvField55Spec* spec,
                         size_t spec_count, uint8_t* output,
                         size_t output_capacity, size_t* written);

#endif
