#include "busemanager.hpp"
#include <algorithm>
#include <cstring>
#include <loguru.hpp>

BuseManager::BuseManager(int bufferSize, int blockSize) : shouldStopSyncThread(false), BLOCK_SIZE(blockSize), BUFFER_SIZE(bufferSize) {
    buffer = malloc(bufferSize);
    remoteBuffer = malloc(bufferSize);
}

void BuseManager::runPeriodicSync(int verbose) {
    std::unique_lock<std::mutex> lock(lockMutex);
    while (!shouldStopSyncThread.load()) {
        if (syncCondVar.wait_for(lock, std::chrono::seconds(SYNC_INTERVAL)) == std::cv_status::timeout) {
            if (!writeOps.empty()) {
                synchronizeData(verbose);
            }
        }
    }
    // Perform final sync before exit
    if (!writeOps.empty()) {
        LOG_F(INFO, "Performing final sync before exit");
        synchronizeData(verbose);
    }
    LOG_F(INFO, "Exiting sync thread");
}

void BuseManager::addWriteOp(uint64_t offset, uint32_t len) {
    std::lock_guard<std::mutex> guard(lockMutex);
    writeOps.push_back({offset, len});
}

void BuseManager::stopSyncThread() {
    shouldStopSyncThread.store(true);
    syncCondVar.notify_all();
}

void BuseManager::consolidateWriteOperations() {
    if (writeOps.empty())
        return;

    // Sort writeOps based on offset
    std::sort(writeOps.begin(), writeOps.end(), [](const WriteOp& a, const WriteOp& b) { return a.offset < b.offset; });

    LOG_F(INFO, "Before merge: %lu", writeOps.size());

    std::vector<WriteOp> mergedOps;
    WriteOp currentOp = writeOps[0];

    for (size_t i = 1; i < writeOps.size(); ++i) {
        WriteOp nextOp = writeOps[i];
        // Check if current and next operations can be merged
        const bool shouldMerge = currentOp.offset + currentOp.len >= nextOp.offset || nextOp.offset - currentOp.offset < BLOCK_SIZE;

        if (shouldMerge) {
            // Merge operations
            uint64_t newEnd = std::max(currentOp.offset + currentOp.len, nextOp.offset + nextOp.len);
            currentOp.len = newEnd - currentOp.offset;
        } else {
            // No merge possible, push currentOp to mergedOps and move to next
            mergedOps.push_back(currentOp);
            currentOp = nextOp;
        }
    }
    // Push the last operation
    mergedOps.push_back(currentOp);

    LOG_F(INFO, "After merge: %lu", mergedOps.size());

    writeOps = std::move(mergedOps);
}

void BuseManager::synchronizeData(int verbose) {
    if (verbose)
        LOG_F(INFO, "Received a sync request.");

    consolidateWriteOperations();
    for (const auto& op : writeOps) {
        memcpy(static_cast<char*>(remoteBuffer) + op.offset, static_cast<char*>(buffer) + op.offset, op.len);
        if (verbose)
            LOG_F(INFO, "Synced %lu, %u", op.offset, op.len);
    }
    writeOps.clear();

    if (memcmp(buffer, remoteBuffer, BUFFER_SIZE) != 0) {
        LOG_F(ERROR, "Data and remote_data are not in sync");
    }
}