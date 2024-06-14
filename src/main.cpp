#include <queue>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include "pch.h"  // Include the precompiled header

/* BUSE callbacks */
static void* data;
static void* remote_data;
static std::atomic<bool> stop_sync_thread{false};
static std::mutex sync_mutex;
static std::condition_variable sync_cv;

// Write operation to be sent to the remote data
struct write_op {
    u_int64_t offset;
    u_int32_t len;
};

// Write operation queue
std::vector<write_op> write_ops;

const u_int64_t BLOCK_SIZE = 4096;  // Example block size

// Function to merge write operations
void merge_write_ops() {
    if (write_ops.empty())
        return;

    // Sort write_ops based on offset
    std::vector<write_op> sorted_ops(write_ops.size());
    std::copy(std::begin(write_ops), std::end(write_ops), std::begin(sorted_ops));
    std::sort(sorted_ops.begin(), sorted_ops.end(), [](const write_op& a, const write_op& b) { return a.offset < b.offset; });

    LOG_F(INFO, "Before merging: %lu", write_ops.size());

    std::queue<write_op> merged_ops;
    write_op current_op = sorted_ops[0];

    for (size_t i = 1; i < sorted_ops.size(); ++i) {
        write_op next_op = sorted_ops[i];
        // Check if current and next operations can be merged
        if (current_op.offset + current_op.len >= next_op.offset || next_op.offset - current_op.offset < BLOCK_SIZE) {
            // Merge operations
            u_int64_t new_end = std::max(current_op.offset + current_op.len, next_op.offset + next_op.len);
            current_op.len = new_end - current_op.offset;
        } else {
            // No merge possible, push current_op to merged_ops and move to next
            merged_ops.push(current_op);
            current_op = next_op;
        }
    }
    // Push the last operation
    merged_ops.push(current_op);

    // Replace original queue with merged operations
    write_ops.clear();
    while (!merged_ops.empty()) {
        write_ops.push_back(merged_ops.front());
        merged_ops.pop();
    }

    LOG_F(INFO, "After merging: %lu", write_ops.size());
}

static int xmp_sync(void* verbose) {
    if (*(int*)verbose)
        LOG_F(INFO, "Received a sync request.");

    // Sync the data to the remote data
    // Diff the data and remote_data, write only the diff to the remote_data
    merge_write_ops();
    for (const auto& op : write_ops) {
        memcpy((char*)remote_data + op.offset, (char*)data + op.offset, op.len);
        if (*(int*)verbose)
            LOG_F(INFO, "Synced %lu, %u", op.offset, op.len);
    }
    write_ops.clear();

    // compare data and remote_data
    if (memcmp(data, remote_data, BLOCK_SIZE) != 0) {
        LOG_F(ERROR, "Data and remote_data are not in sync");
    }

    return 0;
}

static int xmp_read(void* buf, u_int32_t len, u_int64_t offset, void* verbose) {
    if (*(int*)verbose)
        LOG_F(INFO, "R - %lu, %u", offset, len);
    memcpy(buf, (char*)data + offset, len);
    return 0;
}

static int xmp_write(const void* buf, u_int32_t len, u_int64_t offset, void* verbose) {
    if (*(int*)verbose)
        LOG_F(INFO, "W - %lu, %u", offset, len);
    memcpy((char*)data + offset, buf, len);
    write_ops.push_back({offset, len});
    return 0;
}

static void xmp_disc(void* verbose) {
    if (*(int*)verbose)
        LOG_F(INFO, "Received a disconnect request.");
}

static int xmp_flush(void* verbose) {
    if (*(int*)verbose)
        LOG_F(INFO, "Received a flush request.");
    return 0;
}

static int xmp_trim(u_int64_t from, u_int32_t len, void* verbose) {
    if (*(int*)verbose)
        LOG_F(INFO, "T - %lu, %u", from, len);
    return 0;
}

void sync_data_periodically(int verbose) {
    std::unique_lock<std::mutex> lock(sync_mutex);
    while (!stop_sync_thread.load()) {
        if (sync_cv.wait_for(lock, std::chrono::seconds(5)) == std::cv_status::timeout) {
            if (!write_ops.empty()) {
                xmp_sync((void*)&verbose);
            }
        }
    }
    // Perform final sync before exit
    if (!write_ops.empty()) {
        LOG_F(INFO, "Performing final sync before exit");
        xmp_sync((void*)&verbose);
    }
}

int main(int argc, char* argv[]) {
    cxxopts::Options options("buse_nfs", "Network file system using buse");
    cxxopts::ParseResult result;

    // clang-format off
    options.add_options()
        ("d,dev", "NBD Device path", cxxopts::value<std::string>()->default_value("/dev/nbd0"))
        ("s,size", "Block device size in bytes", cxxopts::value<int>()->default_value("1048576"))
        ("v,verbose", "Enable verbose output", cxxopts::value<int>()->default_value("1"))
        ("h,help", "Print usage")
    ;
    // clang-format on

    if (init_options(argc, argv, options, result) != 0) {
        LOG_F(ERROR, "Failed to initialize options");
        return 1;
    }
    loguru::init(argc, argv);
    LOG_F(INFO, "Starting buse_nfs");
    LOG_F(INFO, "Creating block device at %s with size %d bytes", result["dev"].as<std::string>().c_str(),
          result["size"].as<int>());

    struct buse_operations aop = {
        xmp_read,                                          // read
        xmp_write,                                         // write
        xmp_disc,                                          // disc
        xmp_flush,                                         // flush
        xmp_trim,                                          // trim
        static_cast<u_int64_t>(result["size"].as<int>()),  // size
        512,                                               // blksize
        0                                                  // size_blocks
    };

    data = malloc(aop.size);
    if (data == nullptr) {
        LOG_F(ERROR, "Failed to allocate memory for block device");
        return 1;
    }

    remote_data = malloc(aop.size);
    if (remote_data == nullptr) {
        LOG_F(ERROR, "Failed to allocate memory for remote block device");
        return 1;
    }

    // create thread to sync data to remote_data periodically
    std::thread sync_thread(sync_data_periodically, result["verbose"].as<int>());

    if (buse_main(result["dev"].as<std::string>().c_str(), &aop, (void*)&result["verbose"].as<int>()) != 0) {
        LOG_F(ERROR, "Failed to create block device");
        stop_sync_thread = true;
        sync_cv.notify_all();  // Notify the sync thread to stop
        sync_thread.join();    // Wait for sync thread to finish
        free(data);
        free(remote_data);
        return 1;
    }

    stop_sync_thread.store(true);
    sync_cv.notify_all();  // Notify the sync thread to stop
    sync_thread.join();    // Wait for sync thread to finish

    LOG_F(INFO, "Exiting buse_nfs");

    free(data);
    free(remote_data);

    return 0;
}
