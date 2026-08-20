#ifndef DAEMON_HPP
#define DAEMON_HPP

#include <string>

#include "InstanceLock.hpp"

// Orchestrates the full Matt_daemon lifecycle: privilege check, logger
// init, single-instance lock, daemonization (fork/setsid/chdir/umask),
// then runs the TCP server until quit/signal/fatal error.
class Daemon {
public:
    Daemon(int port, int maxClients, std::string lockPath);

    int run();

private:
    int port_;
    int maxClients_;
    std::string lockPath_;
    InstanceLock lock_;

    bool hasRootPrivileges() const;
    // Forks and detaches from the terminal. The parent branch exits the
    // process directly since it must never proceed to run the server
    // itself. Returns false only on fork()/setsid()/chdir() failure in
    // the child.
    bool daemonize();
};

#endif
