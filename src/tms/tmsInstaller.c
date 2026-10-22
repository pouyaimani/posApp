#include "tmsInstaller.h"
#include "logger.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * TMS installer diagnostics.
 *
 * Log meaningful transitions, byte offsets, operation results and CRC values.
 * Log verification progress every 64 KiB rather than once per 4 KiB read,
 * which would flood the serial/debug console for large firmware packages.
 *
 * The package is verified with CRC-32/ISO-HDLC (reflected polynomial
 * 0xEDB88320, initialization/final XOR 0xFFFFFFFF).
 */
#define TMS_VERIFY_CHUNK_SIZE   4096u
#define TMS_VERIFY_LOG_INTERVAL (64u * 1024u)

/**
 * @brief Parse an expected hexadecimal CRC-32 without accepting trailing
 *        garbage, whitespace, empty strings or values larger than 32 bits.
 *
 * Accepts 1-8 hexadecimal digits, optionally prefixed with "0x" or "0X".
 * Does not log the supplied string, which may originate from a remote server.
 */
static bool parseExpectedCrc32(const char* text, uint32_t* expected) {
    uint32_t value  = 0u;
    size_t   digits = 0u;

    if (text == NULL || expected == NULL) {
        LOG_ERROR("TMS INST parseExpectedCrc32(): invalid args textSet=%d "
                  "outputSet=%d",
                  (int)(text != NULL), (int)(expected != NULL));
        return false;
    }

    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        text += 2;
        LOG_TRACE("TMS INST parseExpectedCrc32(): optional 0x prefix detected");
    }

    while (*text != '\0') {
        unsigned char ch = (unsigned char)*text;
        uint32_t      digit;

        if (ch >= '0' && ch <= '9')
            digit = (uint32_t)(ch - '0');
        else if (ch >= 'a' && ch <= 'f')
            digit = (uint32_t)(ch - 'a' + 10);
        else if (ch >= 'A' && ch <= 'F')
            digit = (uint32_t)(ch - 'A' + 10);
        else {
            LOG_ERROR(
                "TMS INST parseExpectedCrc32(): non-hex character at digit=%lu",
                (unsigned long)digits);
            return false;
        }

        if (digits >= 8u) {
            LOG_ERROR("TMS INST parseExpectedCrc32(): CRC exceeds 32 bits");
            return false;
        }

        value = (value << 4) | digit;
        digits++;
        text++;
    }

    if (digits == 0u) {
        LOG_ERROR("TMS INST parseExpectedCrc32(): expected CRC is empty");
        return false;
    }

    *expected = value;
    LOG_TRACE("TMS INST parseExpectedCrc32(): valid CRC-32 digits=%lu "
              "expected=0x%08lX",
              (unsigned long)digits, (unsigned long)value);
    return true;
}

/**
 * @brief Update a running reflected CRC-32 accumulator.
 *
 * A caller must initialize crc to 0xFFFFFFFF and XOR the final accumulator
 * with 0xFFFFFFFF. No per-call trace: this is a hot path during verification.
 * A NULL data pointer with nonzero length is logged and leaves crc unchanged,
 * preserving the previous implementation's return behavior.
 */
uint32_t tmsCrc32Update(uint32_t crc, const uint8_t* data, size_t length) {
    size_t   i;
    unsigned bit;

    if (data == NULL) {
        if (length != 0u) {
            LOG_ERROR("TMS INST tmsCrc32Update(): NULL data with length=%lu",
                      (unsigned long)length);
        }
        return crc;
    }

    for (i = 0u; i < length; ++i) {
        crc ^= data[i];
        for (bit = 0u; bit < 8u; ++bit) {
            crc = (crc >> 1u) ^ (0xEDB88320u & (uint32_t)-(int32_t)(crc & 1u));
        }
    }

    return crc;
}

/**
 * @brief Verify the downloaded payload using CRC-32.
 *
 * Reads at most TMS_VERIFY_CHUNK_SIZE bytes at a time, and requires every
 * platform read to return exactly the requested number of bytes.
 */
bool tmsInstallerVerify(const TmsPlatformOps* platform, const char* path,
                        uint32_t dataOffset, uint32_t dataLength,
                        const char* expectedHash) {
    uint8_t  buffer[TMS_VERIFY_CHUNK_SIZE];
    uint32_t position           = 0u;
    uint32_t lastLoggedPosition = 0u;
    uint32_t crc                = 0xFFFFFFFFu;
    uint32_t expectedCrc        = 0u;

    LOG_TRACE("TMS INST tmsInstallerVerify(): begin offset=%lu length=%lu "
              "chunkSize=%lu pathSet=%d",
              (unsigned long)dataOffset, (unsigned long)dataLength,
              (unsigned long)sizeof(buffer), (int)(path != NULL));

    if (platform == NULL || platform->readFile == NULL || path == NULL ||
        expectedHash == NULL) {
        LOG_ERROR("TMS INST tmsInstallerVerify(): invalid args platform=%d "
                  "readFile=%d path=%d hash=%d",
                  (int)(platform != NULL),
                  (int)(platform != NULL && platform->readFile != NULL),
                  (int)(path != NULL), (int)(expectedHash != NULL));
        return false;
    }

    if (dataLength == 0u) {
        LOG_ERROR("TMS INST tmsInstallerVerify(): empty payload is not valid");
        return false;
    }

    /* Prevent a 32-bit file offset from wrapping during chunk reads. */
    if (dataOffset > UINT32_MAX - dataLength) {
        LOG_ERROR("TMS INST tmsInstallerVerify(): file offset overflow "
                  "offset=%lu length=%lu",
                  (unsigned long)dataOffset, (unsigned long)dataLength);
        return false;
    }

    if (!parseExpectedCrc32(expectedHash, &expectedCrc)) {
        LOG_ERROR("TMS INST tmsInstallerVerify(): invalid expected CRC-32");
        return false;
    }

    LOG_TRACE("TMS INST tmsInstallerVerify(): reading payload path=%s "
              "fileOffset=%lu total=%lu",
              path, (unsigned long)dataOffset, (unsigned long)dataLength);

    while (position < dataLength) {
        uint32_t remaining    = dataLength - position;
        uint32_t fileOffset   = dataOffset + position;
        size_t   requested    = remaining < (uint32_t)sizeof(buffer)
                                    ? (size_t)remaining
                                    : sizeof(buffer);
        size_t   actualLength = 0u;
        int      readResult;

        readResult = platform->readFile(path, fileOffset, buffer, requested,
                                        &actualLength);
        if (readResult != 0) {
            LOG_ERROR("TMS INST tmsInstallerVerify(): readFile failed rc=%d "
                      "offset=%lu requested=%lu actual=%lu",
                      readResult, (unsigned long)fileOffset,
                      (unsigned long)requested, (unsigned long)actualLength);
            return false;
        }

        if (actualLength != requested) {
            LOG_ERROR("TMS INST tmsInstallerVerify(): short/invalid read "
                      "offset=%lu requested=%lu actual=%lu",
                      (unsigned long)fileOffset, (unsigned long)requested,
                      (unsigned long)actualLength);
            return false;
        }

        crc = tmsCrc32Update(crc, buffer, actualLength);
        position += (uint32_t)actualLength;

        if ((position - lastLoggedPosition) >= TMS_VERIFY_LOG_INTERVAL ||
            position == dataLength) {
            LOG_TRACE(
                "TMS INST tmsInstallerVerify(): read progress=%lu/%lu (%lu%%)",
                (unsigned long)position, (unsigned long)dataLength,
                (unsigned long)(((uint64_t)position * 100u) / dataLength));
            lastLoggedPosition = position;
        }
    }

    crc ^= 0xFFFFFFFFu;
    LOG_TRACE(
        "TMS INST tmsInstallerVerify(): CRC computed=0x%08lX expected=0x%08lX",
        (unsigned long)crc, (unsigned long)expectedCrc);

    if (crc != expectedCrc) {
        LOG_ERROR("TMS INST tmsInstallerVerify(): CRC mismatch "
                  "computed=0x%08lX expected=0x%08lX",
                  (unsigned long)crc, (unsigned long)expectedCrc);
        return false;
    }

    LOG_TRACE("TMS INST tmsInstallerVerify(): verification SUCCESS bytes=%lu",
              (unsigned long)dataLength);
    return true;
}

/**
 * @brief Build the update manifest header and write it at offset zero.
 *
 * The full fixed-size header is zero-filled before formatting, so unused
 * header bytes are deterministic and no stack data is written to disk.
 */
bool tmsInstallerPrepare(const TmsPlatformOps* platform, const char* path,
                         const char* payloadName, uint32_t payloadLength,
                         uint32_t payloadCrc) {
    uint8_t header[TMS_APUPDATE_HEADER_LEN];
    int     length;
    int     writeResult;

    LOG_TRACE("TMS INST tmsInstallerPrepare(): begin payloadLength=%lu "
              "payloadCrc=0x%08lX headerSize=%lu",
              (unsigned long)payloadLength, (unsigned long)payloadCrc,
              (unsigned long)sizeof(header));

    if (platform == NULL || platform->writeFile == NULL || path == NULL ||
        payloadName == NULL) {
        LOG_ERROR("TMS INST tmsInstallerPrepare(): invalid args platform=%d "
                  "writeFile=%d path=%d name=%d",
                  (int)(platform != NULL),
                  (int)(platform != NULL && platform->writeFile != NULL),
                  (int)(path != NULL), (int)(payloadName != NULL));
        return false;
    }

    memset(header, 0, sizeof(header));

    LOG_TRACE("TMS INST tmsInstallerPrepare(): formatting header payload=%s "
              "base=0x%08lX",
              payloadName, (unsigned long)TMS_APUPDATE_HEADER_LEN);
    length = snprintf((char*)header, sizeof(header),
                      "[namelist]\r\nfile0=%s\r\n"
                      "[adrlist]\r\nadr0=0x%08lX\r\n"
                      "[lenlist]\r\nlen0=%lu\r\n"
                      "[crclist]\r\ncrc0=0x%08lX\r\n",
                      payloadName, (unsigned long)TMS_APUPDATE_HEADER_LEN,
                      (unsigned long)payloadLength, (unsigned long)payloadCrc);

    if (length < 0) {
        LOG_ERROR(
            "TMS INST tmsInstallerPrepare(): snprintf formatting failed rc=%d",
            length);
        return false;
    }

    if ((size_t)length >= sizeof(header)) {
        LOG_ERROR("TMS INST tmsInstallerPrepare(): header too large "
                  "required=%lu capacity=%lu",
                  (unsigned long)length + 1u, (unsigned long)sizeof(header));
        return false;
    }

    LOG_TRACE("TMS INST tmsInstallerPrepare(): header ready textLen=%d "
              "padding=%lu path=%s",
              length, (unsigned long)(sizeof(header) - (size_t)length), path);
    LOG_TRACE("TMS INST tmsInstallerPrepare(): writeFile offset=0 length=%lu",
              (unsigned long)sizeof(header));

    writeResult = platform->writeFile(path, 0u, header, sizeof(header));
    if (writeResult != 0) {
        LOG_ERROR(
            "TMS INST tmsInstallerPrepare(): writeFile failed rc=%d path=%s",
            writeResult, path);
        return false;
    }

    LOG_TRACE("TMS INST tmsInstallerPrepare(): header prepared successfully");
    return true;
}

/**
 * @brief Activate the prepared update, either using the platform callback
 *        or the legacy switch flag file; synchronize files if supported.
 */
bool tmsInstallerActivate(const TmsPlatformOps* platform) {
    static const uint8_t flag = '1';
    int                  result;

    LOG_TRACE("TMS INST tmsInstallerActivate(): begin");

    if (platform == NULL) {
        LOG_ERROR("TMS INST tmsInstallerActivate(): platform is NULL");
        return false;
    }

    if (platform->setUpdateFlag != NULL) {
        LOG_TRACE(
            "TMS INST tmsInstallerActivate(): using setUpdateFlag callback");

        result = platform->setUpdateFlag();
        if (result != 0) {
            LOG_ERROR(
                "TMS INST tmsInstallerActivate(): setUpdateFlag failed rc=%d",
                result);
            return false;
        }
        LOG_TRACE(
            "TMS INST tmsInstallerActivate(): update flag set successfully");
    } else {
        LOG_TRACE("TMS INST tmsInstallerActivate(): using switch-file fallback "
                  "path=%s",
                  TMS_SWITCH_FLAG_PATH);

        if (platform->createFile == NULL || platform->writeFile == NULL) {
            LOG_ERROR("TMS INST tmsInstallerActivate(): missing flag-file ops "
                      "create=%d write=%d",
                      (int)(platform->createFile != NULL),
                      (int)(platform->writeFile != NULL));
            return false;
        }

        if (platform->removeFile != NULL) {
            result = platform->removeFile(TMS_SWITCH_FLAG_PATH);
            /* This may fail when the flag file does not yet exist. */
            LOG_TRACE("TMS INST tmsInstallerActivate(): remove old switch flag "
                      "rc=%d (nonfatal)",
                      result);
        } else {
            LOG_TRACE("TMS INST tmsInstallerActivate(): removeFile "
                      "unavailable, skipping removal");
        }

        result = platform->createFile(TMS_SWITCH_FLAG_PATH, 1u);
        if (result != 0) {
            LOG_ERROR("TMS INST tmsInstallerActivate(): create switch flag "
                      "failed rc=%d",
                      result);
            return false;
        }
        LOG_TRACE(
            "TMS INST tmsInstallerActivate(): switch flag file created size=1");

        result = platform->writeFile(TMS_SWITCH_FLAG_PATH, 0u, &flag, 1u);
        if (result != 0) {
            LOG_ERROR("TMS INST tmsInstallerActivate(): write switch flag "
                      "failed rc=%d",
                      result);
            return false;
        }
        LOG_TRACE("TMS INST tmsInstallerActivate(): switch flag written "
                  "successfully");
    }

    if (platform->syncFiles == NULL) {
        LOG_TRACE("TMS INST tmsInstallerActivate(): syncFiles unavailable, "
                  "skipping sync");
        LOG_TRACE("TMS INST tmsInstallerActivate(): activation SUCCESS");
        return true;
    }

    LOG_TRACE("TMS INST tmsInstallerActivate(): synchronizing files");
    result = platform->syncFiles();
    if (result != 0) {
        LOG_ERROR("TMS INST tmsInstallerActivate(): syncFiles failed rc=%d",
                  result);
        return false;
    }

    LOG_TRACE(
        "TMS INST tmsInstallerActivate(): sync complete, activation SUCCESS");
    return true;
}
