#ifndef SIGNAL_GUARD_HPP
#define SIGNAL_GUARD_HPP

#include <csignal>

// Installs SIGINT/SIGTERM handlers that only set a flag -- the one thing
// the C++ standard guarantees is safe to do inside a signal handler -- and
// ignores SIGPIPE so a client dropping mid-write can't kill the daemon.
// The actual logging and shutdown happen wherever consumeSignal() is
// polled, back in normal execution context.
class SignalGuard {
public:
    SignalGuard() = delete;

    static void install();
    static int consumeSignal();

private:
    static volatile sig_atomic_t signalNumber_;

    static void handle(int signum);
};

#endif
