#pragma once

#if __has_include(<zeno/utils/logger.h>)
#include <zeno/utils/logger.h>
using namespace zeno::loggerstd;
#else
#include <cstdio>
#endif


<<<<<<< HEAD
#define dbg_printf(...) std::printf("[ZenoFX] " __VA_ARGS__)
=======
#define dbg_printf(...) log_printf("[ZenoFX] " __VA_ARGS__)
>>>>>>> master
