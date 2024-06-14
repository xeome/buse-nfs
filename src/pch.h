// src/pch.h
#pragma once

#include <iostream>

#include "loguru.hpp"
#include "cxxopts.hpp"
#include "buse.h"

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