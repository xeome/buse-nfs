#ifndef BUSE_MANAGER_H
#define BUSE_MANAGER_H

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <vector>

constexpr uint64_t MAX_WRITE_LENGTH = 4096;

struct WriteOp {
    uint64_t offset;
    uint32_t len;
};

class BuseManager {
   public:
    explicit BuseManager(uint64_t bufferSize = 0);
    ~BuseManager();

    void runPeriodicSync();
    void addWriteOperation(uint64_t startOffset, uint64_t endOffset);
    std::pair<uint64_t, uint64_t> findNextDifference(uint64_t startOffset);
    void stopSyncThread();
    uint64_t getBufferSize() const { return BUFFER_SIZE; }
    std::atomic<bool>& getHasWrites() { return hasWrites; }
    void synchronizeData();

    static std::unique_ptr<char[]> buffer;
    static std::unique_ptr<char[]> remoteBuffer;
    static std::mutex writeMutex;

   private:
    std::vector<WriteOp> writeOps;
    std::atomic<bool> isRunning{true};
    std::atomic<bool> hasWrites{false};
    std::mutex lockMutex;
    std::condition_variable intervalCV;
    const uint64_t SYNC_INTERVAL = 5;
    uint64_t BUFFER_SIZE;

    void consolidateWriteOperations();
};

#endif  // BUSE_MANAGER_H