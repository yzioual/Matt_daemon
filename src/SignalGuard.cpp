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

    struct sigaction sa_ignore{};
    sa_ignore.sa_flags = 0;
    sigemptyset(&sa_ignore.sa_mask);
    sa_ignore.sa_handler = SIG_IGN;

    sigaction(SIGTERM, &sa_stop, nullptr);
    sigaction(SIGINT, &sa_stop, nullptr);
    // A client dropping its connection right before we write() to it would
    // otherwise raise SIGPIPE, whose default action kills the whole daemon.
    sigaction(SIGPIPE, &sa_ignore, nullptr);
}

int SignalGuard::consumeSignal() {
    int sig = signalNumber_;
    signalNumber_ = 0;
    return sig;
}
