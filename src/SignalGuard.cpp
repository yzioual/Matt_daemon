#include "SignalGuard.hpp"

volatile sig_atomic_t SignalGuard::signalNumber_ = 0;

void SignalGuard::handle(int signum) {
    signalNumber_ = signum;
}

void SignalGuard::install() {
    struct sigaction sa_stop{};
    sa_stop.sa_flags = 0;
    sigemptyset(&sa_stop.sa_mask);
    sa_stop.sa_handler = handle;

    sigaction(SIGTERM, &sa_stop, nullptr);
    sigaction(SIGINT, &sa_stop, nullptr);
    sigaction(SIGQUIT, &sa_stop, nullptr);
    sigaction(SIGHUP, &sa_stop, nullptr);
    // A client dropping its connection right before we write() to it would
    // otherwise raise SIGPIPE, whose default action kills the whole daemon.
    signal(SIGPIPE, SIG_IGN);
    // SIGKILL/SIGSTOP are unblockable by POSIX design, and synchronous
    // fault signals (SIGSEGV, SIGBUS, SIGABRT, SIGFPE) leave the process
    // in an undefined state, so resuming normal execution or doing I/O
    // from their handler would itself be unsafe -- neither is handled.
}

int SignalGuard::consumeSignal() {
    int sig = signalNumber_;
    signalNumber_ = 0;
    return sig;
}
