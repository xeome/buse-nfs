#include <iostream>

#include "pch.h"  // Include the precompiled header

static int init_options(const int argc, char** argv, cxxopts::Options& options, cxxopts::ParseResult& result) {
    result = options.parse(argc, argv);

    if (result.count("help")) {
        std::cout << options.help() << std::endl;
        exit(0);
    }

    // if (result.arguments().empty()) {
    //     std::cout << options.help() << std::endl;
    //     exit(0);
    // }

    return 0;
}

/* BUSE callbacks */
static void* data;

static int xmp_read(void* buf, u_int32_t len, u_int64_t offset, void* userdata) {
    if (*(int*)userdata)
        LOG_F(INFO, "R - %lu, %u", offset, len);
    memcpy(buf, (char*)data + offset, len);
    return 0;
}

static int xmp_write(const void* buf, u_int32_t len, u_int64_t offset, void* userdata) {
    if (*(int*)userdata)
        LOG_F(INFO, "W - %lu, %u", offset, len);
    memcpy((char*)data + offset, buf, len);
    return 0;
}

static void xmp_disc(void* userdata) {
    if (*(int*)userdata) {
        LOG_F(INFO, "Received a disconnect request.");
    }
}

static int xmp_flush(void* userdata) {
    if (*(int*)userdata)
        LOG_F(INFO, "Received a flush request.");
    return 0;
}

static int xmp_trim(u_int64_t from, u_int32_t len, void* userdata) {
    if (*(int*)userdata)
        LOG_F(INFO, "T - %lu, %u", from, len);
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

    if (buse_main(result["dev"].as<std::string>().c_str(), &aop, (void*)&result["verbose"].as<int>()) != 0) {
        LOG_F(ERROR, "Failed to create block device");
        return 1;
    }

    return 0;
}