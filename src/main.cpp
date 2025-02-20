#include <unistd.h>
#include <cstdlib>
#include <thread>
#include <cstring>  // For memcpy and memcmp

#include "pch.h"  // Include the precompiled header
#include <cxxopts.hpp>

std::unique_ptr<BuseManager> buseManager;
std::thread syncThread;

static int xmp_read(void* buf, uint32_t len, uint64_t offset, void* verbose) {
    if (*(int*)verbose)
        LOG_F(INFO, "R - %lu, %u", offset, len);

    if (__builtin_expect((offset + len > buseManager->getBufferSize()), 0)) {
        LOG_F(ERROR, "Read request out of bounds - %lu, %u", offset, len);
        return 0;
    }

    std::memcpy(buf, BuseManager::buffer.get() + offset, len);
    return 0;
}

static int xmp_write(const void* buf, uint32_t len, uint64_t offset, void* verbose) {
    std::lock_guard<std::mutex> lock(BuseManager::writeMutex);

    if (*(int*)verbose)
        LOG_F(INFO, "W - %lu, %u", offset, len);

    if (__builtin_expect((offset + len > buseManager->getBufferSize()), 0)) {
        LOG_F(ERROR, "Write request out of bounds - %lu, %u", offset, len);
        return 0;
    }

    std::memcpy(BuseManager::buffer.get() + offset, buf, len);

    if (buseManager->getHasWrites().load() == false) {
        buseManager->getHasWrites().store(true);
    }

    return 0;
}

static void xmp_disc(void* verbose) {
    if (*(int*)verbose)
        LOG_F(INFO, "Disconnect request received");
    buseManager->stopSyncThread();
}

static int xmp_flush(void* verbose) {
    if (*(int*)verbose)
        LOG_F(INFO, "Flush");
    buseManager->synchronizeData();
    return 0;
}

static int xmp_trim(uint64_t offset, uint32_t len, void* verbose) {
    if (*(int*)verbose)
        LOG_F(INFO, "Trim - %lu, %u", offset, len);
    return 0;
}

static int xmp_init(void* verbose) {
    if (*(int*)verbose)
        LOG_F(INFO, "Init");

    syncThread = std::thread(&BuseManager::runPeriodicSync, buseManager.get());
    buseManager->getHasWrites().store(false);

    return 0;
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
    LOG_F(INFO, "Creating block device at %s with size %d bytes", result["dev"].as<std::string>().c_str(), result["size"].as<int>());

    buseManager = std::make_unique<BuseManager>(result["size"].as<int>());

    // Start buse
    struct buse_operations aop = {
        xmp_read,                                          // read
        xmp_write,                                         // write
        xmp_disc,                                          // disc
        xmp_flush,                                         // flush
        xmp_trim,                                          // trim
        xmp_init,                                          // init
        static_cast<u_int64_t>(result["size"].as<int>()),  // size
        512,                                               // blksize
        0  // size_blocks, setting other than 0 causes out of bound reads and writes for some reason
    };

    std::thread buseThread([&]() {
        if (buse_main(result["dev"].as<std::string>().c_str(), &aop, (void*)&result["verbose"].as<int>()) != 0) {
            LOG_F(ERROR, "Failed to create block device");

            // Stop the sync thread
            buseManager->stopSyncThread();
        }
        LOG_F(INFO, "Exiting buse thread");
    });

    if (buseThread.joinable()) {
        buseThread.join();
    }

    if (syncThread.joinable()) {
        syncThread.join();
    }

    LOG_F(INFO, "Exiting buse_nfs");

    return 0;
}
