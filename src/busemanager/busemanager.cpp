#include "busemanager.hpp"
#include <sys/types.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <loguru.hpp>

constexpr uint64_t MAX_WRITE_LENGTH = 4096;

BuseManager::BuseManager(int bufferSize) : isRunning(true), BUFFER_SIZE(bufferSize) {
    buffer = malloc(bufferSize);
    remoteBuffer = malloc(bufferSize);
}

void BuseManager::runPeriodicSync() {
    std::unique_lock<std::mutex> lock(lockMutex);
    while (isRunning.load()) {
        if (intervalCV.wait_for(lock, std::chrono::seconds(SYNC_INTERVAL)) == std::cv_status::timeout && hasWrites.load()) {
            synchronizeData();
            hasWrites.store(false);
        }
    }
    LOG_F(INFO, "Performing final sync before exit");
    synchronizeData();
    LOG_F(INFO, "Exiting sync thread");
}

void BuseManager::stopSyncThread() {
    isRunning.store(false);
    intervalCV.notify_all();
    hasWrites.store(true);  // Ensure that the final sync is performed
}

void BuseManager::addWriteOperation(uint64_t startOffset, uint64_t endOffset) {
    uint64_t length = endOffset - startOffset + 1;
    if (length > MAX_WRITE_LENGTH) {
        LOG_F(ERROR, "Length is too large. startOffset: %lu, endOffset: %lu", startOffset, endOffset);
        for (uint64_t offset = startOffset; offset < endOffset; offset += MAX_WRITE_LENGTH) {
            writeOps.push_back({offset, static_cast<uint32_t>(std::min(MAX_WRITE_LENGTH, endOffset - offset + 1))});
        }
    } else {
        writeOps.push_back({startOffset, static_cast<uint32_t>(length)});
    }
}

std::pair<uint64_t, uint64_t> BuseManager::findNextDifference(uint64_t startOffset) {
    auto firstDiff = std::mismatch(static_cast<char*>(buffer) + startOffset, static_cast<char*>(buffer) + BUFFER_SIZE,
                                   static_cast<char*>(remoteBuffer) + startOffset);
    if (firstDiff.first == static_cast<char*>(buffer) + BUFFER_SIZE) {
        return {BUFFER_SIZE, BUFFER_SIZE};  // Indicate no more differences
    }

    uint64_t diffStart = firstDiff.first - static_cast<char*>(buffer);
    uint64_t diffEnd = diffStart;
    for (uint64_t i = diffStart; i < BUFFER_SIZE && i - diffStart < MAX_WRITE_LENGTH; i++) {
        if (static_cast<char*>(buffer)[i] != static_cast<char*>(remoteBuffer)[i]) {
            diffEnd = i;
        }
    }

    if (diffEnd - diffStart >= MAX_WRITE_LENGTH) {
        LOG_F(ERROR, "Difference is too large. diffStart: %lu, diffEnd: %lu", diffStart, diffEnd);
    }

    return {diffStart, diffEnd};
}

void BuseManager::consolidateWriteOperations() {
    uint64_t startOffset = 0;
    while (startOffset < BUFFER_SIZE) {
        auto [diffStart, diffEnd] = findNextDifference(startOffset);
        if (diffStart >= BUFFER_SIZE)
            break;  // No more differing bytes

        addWriteOperation(diffStart, diffEnd);
        startOffset = diffEnd + 1;
    }
}

void BuseManager::synchronizeData() {
    consolidateWriteOperations();

    std::lock_guard<std::mutex> lock(writeMutex);
    for (const auto& op : writeOps) {
        memcpy(static_cast<char*>(remoteBuffer) + op.offset, static_cast<char*>(buffer) + op.offset, op.len);
        // LOG_F(INFO, "Synced %lu, %u", op.offset, op.len);
    }
    writeOps.clear();

    if (memcmp(buffer, remoteBuffer, BUFFER_SIZE) != 0) {
        LOG_F(ERROR, "Data and remote_data are not in sync");
    }

    LOG_F(INFO, "Data synchronized");
}