#pragma once
#if defined(_WIN64)
#include "./fiber_win_amd64.h"
#elif defined(__linux__) && (defined(__x86_64__) || defined(__amd64__))
#include "./fiber_linux_amd64.h"
#else
#error "Unsupported platform"
#endif