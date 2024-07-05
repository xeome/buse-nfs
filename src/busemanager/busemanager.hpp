#ifndef BUSE_MANAGER_H
#define BUSE_MANAGER_H

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <vector>

// Forward declaration for WriteOp structure
struct WriteOp {
    uint64_t offset;
    uint32_t len;
};

class BuseManager {
   public:
    BuseManager(int bufferSize = 1048576);
    void runPeriodicSync();
    void addWriteOperation(uint64_t startOffset, uint64_t endOffset);
    std::pair<uint64_t, uint64_t> findNextDifference(uint64_t startOffset);
    void stopSyncThread();

    static void* buffer;
    static void* remoteBuffer;
    static std::mutex writeMutex;

   private:
    std::vector<WriteOp> writeOps;
    std::atomic<bool> shouldStopSyncThread;
    std::mutex lockMutex;
    std::condition_variable syncCondVar;
    const uint64_t SYNC_INTERVAL = 5;
    const uint64_t BUFFER_SIZE;

    void consolidateWriteOperations();
    void synchronizeData();
};

#endif  // BUSE_MANAGER_H