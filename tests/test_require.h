#pragma once
#include <cstdlib>
#include <iostream>
#define REQUIRE(condition) do { \
    if (!(condition)) { \
        std::cerr << "REQUIRE failed: " << #condition << " at " << __FILE__ << ":" << __LINE__ << '\n'; \
        std::exit(EXIT_FAILURE); \
    } \
} while (false)
