#pragma once
#include <cstdio>

#ifdef DEBUG
  #define DBG_PRINT(fmt, ...) \
    std::printf("[DEBUG] %s:%d: " fmt "\n", \
                __FILE__, __LINE__, ##__VA_ARGS__)
#else
  #define DBG_PRINT(fmt, ...) ((void)0)
#endif