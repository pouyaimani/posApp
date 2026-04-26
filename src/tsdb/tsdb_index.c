/**
 * @file tsdb_index.c
 * @brief Sparse time index management with binary search
 */

#include "tsdb_internal.h"
#include "logger.h"

static const char *TAG = "TSDB_INDEX";

/**
 * @brief Find block containing or near a timestamp using binary search on sparse index
 *
 * @param file File handle
 * @param header Database header
 * @param timestamp Target timestamp
 * @param block_num Output block number
 * @return ESP_OK on success
 */
esp_err_t tsdb_find_block_for_timestamp(tsdb_file_io_handle_t *file,
                                        const tsdb_header_t *header,
                                        uint32_t timestamp,
                                        uint32_t *block_num) {
    if (file == NULL || header == NULL || block_num == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // No index entries yet
    if (header->index_entries == 0) {
        *block_num = 0;
        return ESP_OK;
    }

    // Check bounds
    if (timestamp < header->oldest_timestamp) {
        LOG_DEBUG("Timestamp %lu before oldest %lu",
                 (unsigned long)timestamp, (unsigned long)header->oldest_timestamp);
        *block_num = header->oldest_record_idx / header->records_per_block;
        return ESP_OK;
    }

    if (timestamp > header->newest_timestamp) {
        LOG_DEBUG("Timestamp %lu after newest %lu",
                 (unsigned long)timestamp, (unsigned long)header->newest_timestamp);
        *block_num = header->newest_record_idx / header->records_per_block;
        return ESP_OK;
    }

    // Binary search on sparse index
    int32_t left = 0;
    int32_t right = header->index_entries - 1;
    uint32_t best_block = 0;
    uint32_t best_timestamp = 0;

    LOG_DEBUG("Binary search for timestamp %lu (entries: %lu)",
             (unsigned long)timestamp, (unsigned long)header->index_entries);

    while (left <= right) {
        int32_t mid = (left + right) / 2;

        // Read index entry
        tsdb_index_entry_t entry;
        uint32_t index_file_offset = header->index_offset +
                                     (mid * sizeof(tsdb_index_entry_t));

        // fseek(file, index_file_offset, SEEK_SET);
        tsdb_file_io.seek(file, index_file_offset, FILE_IO_SEEK_SET);
        // if (fread(&entry, sizeof(tsdb_index_entry_t), 1, file) != 1) {
        if (tsdb_file_io.read(&entry, sizeof(tsdb_index_entry_t), 1, file) != 1) {
            LOG_ERROR("Failed to read index entry %ld", (long)mid);
            break;
        }

        LOG_DEBUG("Index[%ld]: timestamp=%lu, block=%lu",
                 (long)mid, (unsigned long)entry.timestamp, (unsigned long)entry.block_number);

        if (entry.timestamp == 0) {
            // Uninitialized entry, search left
            right = mid - 1;
            continue;
        }

        if (entry.timestamp == timestamp) {
            // Exact match
            *block_num = entry.block_number;
            LOG_DEBUG("Exact match at block %lu", (unsigned long)*block_num);
            return ESP_OK;
        } else if (entry.timestamp < timestamp) {
            // This is a candidate, but keep searching right
            best_block = entry.block_number;
            best_timestamp = entry.timestamp;
            left = mid + 1;
        } else {
            // Too far right, search left
            right = mid - 1;
        }
    }

    *block_num = best_block;

    LOG_DEBUG("Found block %lu (index timestamp=%lu)",
             (unsigned long)*block_num, (unsigned long)best_timestamp);

    return ESP_OK;
}
