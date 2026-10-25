// #include "emv_tag_db.h"
// #include <string.h>

// /* Portable zeroing helper; volatile stores inhibit dead-store elimination.
// */ static void secure_clear(void *ptr, size_t len) {
//     volatile uint8_t *p = (volatile uint8_t *)ptr;
//     while (len-- != 0u) { *p++ = 0u; }
// }

// void emvTagDbClear(EmvTagDb *db) {
//     if (db != NULL) { secure_clear(db, sizeof(*db)); }
// }

// EmvError emvTagDbSet(EmvTagDb *db, EmvTag tag,
//                      const uint8_t *value, size_t value_len) {
//     size_t i;
//     if (db == NULL || tag == 0u || (value == NULL && value_len != 0u) ||
//         value_len > UINT16_MAX) { return EMV_ERR_ARGUMENT; }
//     for (i = 0; i < db->count; ++i) {
//         EmvTagEntry *entry = &db->entries[i];
//         if (entry->tag == tag) {
//             if (entry->length == value_len) {
//                 if (value_len != 0u) { memmove(&db->bytes[entry->offset],
//                 value, value_len); } return EMV_OK;
//             }
//             /* Larger replacement: orphan old range until next clear. */
//             if (value_len > sizeof(db->bytes) - db->used) { return
//             EMV_ERR_CAPACITY; } if (value_len != 0u) {
//             memmove(&db->bytes[db->used], value, value_len); }
//             secure_clear(&db->bytes[entry->offset], entry->length);
//             entry->offset = (uint16_t)db->used;
//             entry->length = (uint16_t)value_len;
//             db->used += value_len;
//             return EMV_OK;
//         }
//     }
//     if (db->count >= EMV_TAG_DB_MAX_ITEMS ||
//         value_len > sizeof(db->bytes) - db->used) { return EMV_ERR_CAPACITY;
//         }
//     db->entries[db->count].tag = tag;
//     db->entries[db->count].offset = (uint16_t)db->used;
//     db->entries[db->count].length = (uint16_t)value_len;
//     ++db->count;
//     if (value_len != 0u) { memmove(&db->bytes[db->used], value, value_len); }
//     db->used += value_len;
//     return EMV_OK;
// }

// bool emvTagDbGet(const EmvTagDb *db, EmvTag tag,
//                  const uint8_t **value, size_t *length) {
//     size_t i;
//     if (db == NULL || value == NULL || length == NULL) { return false; }
//     for (i = 0; i < db->count; ++i) {
//         if (db->entries[i].tag == tag) {
//             *length = db->entries[i].length;
//             *value = &db->bytes[db->entries[i].offset];
//             return true;
//         }
//     }
//     return false;
// }

// /* Decode a BER identifier and a definite length without any out-of-bounds
// reads. */ static EmvError parse_header(const uint8_t *data, size_t size,
//                              size_t *header_size, EmvTag *tag,
//                              size_t *value_len, bool *constructed) {
//     size_t pos = 0u, j, n;
//     uint8_t b;
//     EmvTag t;
//     size_t l;
//     if (size < 2u || data == NULL) { return EMV_ERR_TLV; }
//     t = data[pos++];
//     *constructed = (data[0] & 0x20u) != 0u;
//     if ((data[0] & 0x1Fu) == 0x1Fu) {
//         /* Restrict identifiers to 3 bytes (EMV uses at most 3 here). */
//         for (j = 0u; j < 2u; ++j) {
//             if (pos >= size) { return EMV_ERR_TLV; }
//             b = data[pos++];
//             if (j == 0u && (b & 0x7Fu) == 0u) { return EMV_ERR_TLV; }
//             t = (t << 8u) | b;
//             if ((b & 0x80u) == 0u) { break; }
//             if (j == 1u) { return EMV_ERR_TLV; }
//         }
//     }
//     if (pos >= size) { return EMV_ERR_TLV; }
//     b = data[pos++];
//     if ((b & 0x80u) == 0u) { l = b; }
//     else {
//         n = (size_t)(b & 0x7Fu);
//         if (n == 0u || n > 2u || n > size - pos) { return EMV_ERR_TLV; }
//         l = 0u;
//         for (j = 0u; j < n; ++j) { l = (l << 8u) | data[pos++]; }
//         if (l < 128u || (n == 2u && l <= UINT8_MAX)) { return EMV_ERR_TLV; }
//     }
//     if (l > size - pos) { return EMV_ERR_TLV; }
//     *tag = t;
//     *header_size = pos;
//     *value_len = l;
//     return EMV_OK;
// }

// /**
//  * Store only Kahroba flow tags to avoid retaining certificates or unrelated
//  * card records in a small embedded tag database.
//  */
// static bool relevant_card_tag(EmvTag tag) {
//     switch (tag) {
//         case 0x50u: case 0x57u: case 0x5Au: case 0x82u:
//         case 0x84u: case 0x94u: case 0x8Cu:
//         case 0x9F10u: case 0x9F26u: case 0x9F27u: case 0x9F36u:
//             return true;
//         default: return false;
//     }
// }

// /** Tags provided by the terminal must never originate from a card response.
// */ static bool terminal_owned_tag(EmvTag tag) {
//     switch (tag) {
//         case 0x9F02u: case 0x9F03u: case 0x9F37u:
//         case 0x9F1Au: case 0x5F2Au: case 0x9Au:
//         case 0x9Cu: case 0x9F35u:
//         case 0x9F21u: case 0x95u: case 0x9F34u:
//         case 0x9F4Eu: case 0x9F6Eu:
//             return true;
//         default: return false;
//     }
// }

// /** Walk BER-TLV recursively; optionally collect, protect or inspect values.
// */ static EmvError walk_tlv(EmvTagDb *db, const uint8_t *tlv, size_t len,
//                          unsigned depth, bool inspect, bool protect_existing,
//                          EmvTag wanted, const uint8_t *expected, size_t
//                          expected_len, bool *found) {
//     size_t pos = 0u;
//     if (depth > EMV_TLV_MAX_DEPTH) { return EMV_ERR_TLV; }
//     while (pos < len) {
//         size_t hdr = 0u, val_len = 0u;
//         EmvTag tag = 0u;
//         bool constructed = false;
//         EmvError err = parse_header(tlv + pos, len - pos,
//                                      &hdr, &tag, &val_len, &constructed);
//         if (err != EMV_OK) { return err; }
//         if (constructed) {
//             err = walk_tlv(db, tlv + pos + hdr, val_len, depth + 1u,
//                            inspect, protect_existing, wanted, expected,
//                            expected_len, found);
//         } else if (inspect) {
//             if (tag == wanted && val_len == expected_len &&
//                 (val_len == 0u || memcmp(tlv + pos + hdr, expected, val_len)
//                 == 0)) { *found = true;
//             }
//         } else {
//             const uint8_t *prior = NULL;
//             size_t prior_len = 0u;
//             if (protect_existing && terminal_owned_tag(tag)) {
//                 return EMV_ERR_PROTOCOL;
//             }
//             if (protect_existing && !relevant_card_tag(tag)) {
//                 pos += hdr + val_len;
//                 continue;
//             }
//             if (protect_existing && emvTagDbGet(db, tag, &prior, &prior_len))
//             {
//                 if (prior_len != val_len ||
//                     (val_len != 0u && memcmp(prior, tlv + pos + hdr, val_len)
//                     != 0)) { return EMV_ERR_PROTOCOL;
//                 }
//             } else {
//                 err = emvTagDbSet(db, tag, tlv + pos + hdr, val_len);
//             }
//         }
//         if (err != EMV_OK) { return err; }
//         pos += hdr + val_len;
//     }
//     return EMV_OK;
// }

// EmvError emvTlvCollect(EmvTagDb *db, const uint8_t *tlv, size_t len) {
//     if (db == NULL || (len != 0u && tlv == NULL)) { return EMV_ERR_ARGUMENT;
//     } return walk_tlv(db, tlv, len, 0u, false, false, 0u, NULL, 0u, NULL);
// }

// EmvError emvTlvCollectProtected(EmvTagDb *db, const uint8_t *tlv, size_t len)
// {
//     if (db == NULL || (len != 0u && tlv == NULL)) { return EMV_ERR_ARGUMENT;
//     } return walk_tlv(db, tlv, len, 0u, false, true, 0u, NULL, 0u, NULL);
// }

// EmvError emvTlvContains(const uint8_t *tlv, size_t len, EmvTag tag,
//                         const uint8_t *value, size_t value_len, bool *found)
//                         {
//     if (found == NULL || (len != 0u && tlv == NULL) ||
//         (value_len != 0u && value == NULL)) { return EMV_ERR_ARGUMENT; }
//     *found = false;
//     return walk_tlv(NULL, tlv, len, 0u, true, false, tag, value, value_len,
//     found);
// }
