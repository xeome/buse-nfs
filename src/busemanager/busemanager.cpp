#include <sys/types.h>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <loguru.hpp>

#include "busemanager.hpp"

std::unique_ptr<char[]> BuseManager::buffer;
std::unique_ptr<char[]> BuseManager::remoteBuffer;
std::mutex BuseManager::writeMutex;

BuseManager::BuseManager(uint64_t bufferSize) : BUFFER_SIZE(bufferSize) {
    try {
        buffer = std::make_unique<char[]>(BUFFER_SIZE);
        remoteBuffer = std::make_unique<char[]>(BUFFER_SIZE);
    } catch (const std::bad_alloc& e) {
        LOG_F(ERROR, "Failed to allocate buffer: %s", e.what());
        throw;
    }
    LOG_F(INFO, "Buffer allocated with size %lu", BUFFER_SIZE);
}

BuseManager::~BuseManager() {
    if (isRunning.load()) {
        stopSyncThread();
    }
}

void BuseManager::runPeriodicSync() {
    std::unique_lock<std::mutex> lock(lockMutex);
    while (isRunning.load()) {
        if (intervalCV.wait_for(lock, std::chrono::seconds(SYNC_INTERVAL)) == std::cv_status::timeout && hasWrites.load()) {
            LOG_F(INFO, "Syncing data");
            synchronizeData();
            hasWrites.store(false);
        }
    }
    synchronizeData();
    LOG_F(INFO, "Sync thread stopped");
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
            writeOps.emplace_back(WriteOp{offset, static_cast<uint32_t>(std::min(MAX_WRITE_LENGTH, endOffset - offset + 1))});
        }
    } else {
        writeOps.emplace_back(WriteOp{startOffset, static_cast<uint32_t>(length)});
    }
}

std::pair<uint64_t, uint64_t> BuseManager::findNextDifference(uint64_t startOffset) {
    auto firstDiff = std::mismatch(buffer.get() + startOffset, buffer.get() + BUFFER_SIZE, remoteBuffer.get() + startOffset);
    if (firstDiff.first == buffer.get() + BUFFER_SIZE) {
        return {BUFFER_SIZE, BUFFER_SIZE};  // Indicate no more differences
    }

    uint64_t diffStart = firstDiff.first - buffer.get();
    uint64_t diffEnd = diffStart;
    for (uint64_t i = diffStart; i < BUFFER_SIZE && i - diffStart < MAX_WRITE_LENGTH; i++) {
        if (buffer[i] != remoteBuffer[i]) {
            diffEnd = i;
        }
    }

    if (diffEnd - diffStart >= MAX_WRITE_LENGTH) {
        LOG_F(ERROR, "Difference too large - %lu, %lu", diffStart, diffEnd);
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
    std::lock_guard<std::mutex> lock(writeMutex);
    consolidateWriteOperations();

    for (const auto& op : writeOps) {
        std::memcpy(remoteBuffer.get() + op.offset, buffer.get() + op.offset, op.len);
        // LOG_F(INFO, "Synced %lu, %u", op.offset, op.len);
    }
    writeOps.clear();

    if (memcmp(buffer.get(), remoteBuffer.get(), BUFFER_SIZE) != 0) {
        LOG_F(ERROR, "buffer and remoteBuffer are not in sync");
    }
}
