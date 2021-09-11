#ifdef ZENO_FAULTHANDLER
#include <zeno/zeno.h>
#include <zeno/utils/logger.h>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <sstream>
#include <zeno/utils/print_traceback.h>
#ifdef __linux__
#include <string.h>
#endif

namespace zeno {

static const char *signal_to_string(int signo) {
#ifdef __linux__
    return strsignal(signo);
#else
    const char *signame = "SIG-unknown";
    if (signo == SIGSEGV) signame = "SIGSEGV";
    if (signo == SIGFPE) signame = "SIGFPE";
    if (signo == SIGILL) signame = "SIGILL";
    return signame;
#endif
}

static void signal_handler(int signo) {
    log_error("recieved signal {}: {}", signo, signal_to_string(signo));
    print_traceback(1);
    exit(-signo);
}

static void register_my_handlers() {
    if (getenv("ZEN_NOSIGHOOK")) {
        return;
    }
    signal(SIGSEGV, signal_handler);
    signal(SIGFPE, signal_handler);
    signal(SIGILL, signal_handler);
    signal(SIGABRT, signal_handler);
#ifdef __linux__
    signal(SIGBUS, signal_handler);
#endif
}

static int register_my_handlers_helper = (register_my_handlers(), 0);

}
#endif
