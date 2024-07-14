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

    /**
     * @brief Initiates periodic synchronization of data between local and remote buffers.
     *
     * This method starts a thread that periodically checks for differences between the local and remote buffers
     * and synchronizes them accordingly.
     */
    void runPeriodicSync();

    /**
     * @brief Stops the synchronization thread.
     *
     * This method stops the synchronization thread if it is running, ensuring that no further synchronization
     * operations are performed.
     */
    void stopSyncThread();

    /**
     * @brief Returns the size of the buffer.
     * @return The size of the buffer.
     */
    uint64_t getBufferSize() const { return BUFFER_SIZE; }

    /**
     * @brief Returns a reference to the atomic boolean indicating if there are pending write operations.
     * @return A reference to the atomic boolean.
     */
    std::atomic<bool>& getHasWrites() { return hasWrites; }

    /**
     * @brief Synchronizes data between local and remote buffers immediately.
     *
     * This method forces an immediate synchronization of data between the local and remote buffers,
     * processing any pending write operations.
     */
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

    /**
     * @brief Consolidates write operations in the queue.
     *
     * This method consolidates overlapping or adjacent write operations in the queue to optimize
     * the synchronization process.
     */
    void consolidateWriteOperations();

    /**
     * @brief Adds a write operation to the queue.
     * @param startOffset The starting offset of the write operation.
     * @param endOffset The ending offset of the write operation.
     *
     * This method adds a write operation defined by its start and end offsets to the queue of operations
     * to be processed during synchronization.
     */
    void addWriteOperation(uint64_t startOffset, uint64_t endOffset);

    /**
     * @brief Finds the next difference between local and remote buffers starting from a given offset.
     * @param startOffset The offset from which to start searching for differences.
     * @return A pair of offsets indicating the start and end of the next difference.
     *
     * This method scans the buffers starting from the specified offset to find the next range where the local
     * and remote buffers differ. It returns a pair of offsets indicating the start and end of this range.
     */
    std::pair<uint64_t, uint64_t> findNextDifference(uint64_t startOffset);
};

#endif  // BUSE_MANAGER_H