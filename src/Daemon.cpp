#include "Daemon.hpp"

#include <cstdlib>
#include <iostream>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>
#include <utility>

#include "TcpServer.hpp"
#include "Tintin_reporter.hpp"

Daemon::Daemon(int port, int maxClients, std::string lockPath)
    : port_(port), maxClients_(maxClients), lockPath_(std::move(lockPath)) {}

bool Daemon::hasRootPrivileges() const {
    return geteuid() == 0;
}

bool Daemon::daemonize() {
    pid_t pid = fork();
    if (pid > 0) {
        // Parent's job is done: the child carries on as the daemon. It
        // must not fall through to running the server itself.
        exit(EXIT_SUCCESS);
    }
    if (pid < 0)
        return false;

    umask(0);
    openlog("Matt_daemon", LOG_NOWAIT | LOG_PID, LOG_USER);
    syslog(LOG_NOTICE, "Successfully started Matt_daemon");
    Tintin_reporter::getInstance().log(Tintin_reporter::Level::INFO, "Successfully started Matt_daemon");

    if (setsid() < 0) {
        syslog(LOG_ERR, "Could not generate session ID for child process");
        Tintin_reporter::getInstance().log(Tintin_reporter::Level::ERROR,
                                           "Could not generate session ID for child process");
        return false;
    }

    if (chdir("/") < 0) {
        syslog(LOG_ERR, "Could not change working directory to /");
        Tintin_reporter::getInstance().log(Tintin_reporter::Level::ERROR,
                                           "Could not change working directory to /");
        return false;
    }

    // The lock was first written with the parent's PID (so a concurrent
    // second instance can fail fast, pre-fork). Now that we're the real
    // daemon, make the lock file reflect that.
    lock_.refreshPid();

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    return true;
}

int Daemon::run() {
    auto& logger = Tintin_reporter::getInstance();

    if (!hasRootPrivileges()) {
        std::cerr << "Matt_daemon requires root privileges.\n";
        openlog("Matt_daemon", LOG_PID, LOG_DAEMON);
        syslog(LOG_ERR, "Matt_daemon requires root privileges");
        closelog();
        return EXIT_FAILURE;
    }

    if (!logger.init()) {
        std::cerr << "Failed to init Tintin_reporter.\n";
        return EXIT_FAILURE;
    }

    if (!lock_.acquire(lockPath_)) {
        openlog("Matt_daemon", LOG_PID, LOG_DAEMON);
        logger.log(Tintin_reporter::Level::ERROR, "An instance of Matt_daemon is already running.");
        syslog(LOG_ERR, "An instance of Matt_daemon is already running.");
        closelog();
        return EXIT_FAILURE;
    }

    if (!daemonize()) {
        closelog();
        return EXIT_FAILURE;
    }

    // Mirrors the pre-refactor behavior: a fatal server error (socket /
    // bind / listen / poll) is retried forever rather than exiting, since
    // the daemon should never quit unexpectedly. Only a client "quit" or a
    // caught signal ends the loop.
    while (true) {
        TcpServer server(port_, static_cast<std::size_t>(maxClients_));
        if (server.start() && server.run() != TcpServer::StopReason::FatalError)
            break;
    }

    closelog();
    return EXIT_SUCCESS;
}
