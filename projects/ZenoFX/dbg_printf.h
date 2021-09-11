#pragma once

#if __has_include(<zeno/utils/logger.h>)
#include <zeno/utils/logger.h>
using namespace zeno::loggerstd;
#else
#include <cstdio>
#endif


#define dbg_printf(...) printf("[ZenoFX] " __VA_ARGS__)
