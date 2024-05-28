#include <iostream>

#include "pch.h"  // Include the precompiled header

static int init_options(const int argc, char** argv, cxxopts::Options& options, cxxopts::ParseResult& result) {
    result = options.parse(argc, argv);

    if (result.count("help")) {
        std::cout << options.help() << std::endl;
        exit(0);
    }

    if (result.arguments().empty()) {
        std::cout << options.help() << std::endl;
        exit(0);
    }

    return 0;
}

int main(int argc, char* argv[]) {
    cxxopts::Options options("buse_nfs", "Network file system using buse");
    cxxopts::ParseResult result;

    // clang-format off
    options.add_options()
        ("d,dev", "NBD Device path", cxxopts::value<std::string>()->default_value("/dev/nbd0"))
        ("s,size", "Block device size in bytes", cxxopts::value<int>()->default_value("1048576"))
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

    return 0;
}