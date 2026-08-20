#include <syslog.h>
#include <print>
#include <unistd.h>
#include <sys/stat.h>
#include <atomic>
#include <csignal>
#include <iostream>
#include <fcntl.h>
#include <sys/file.h>
#include <cstring>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include "Tintin_reporter.hpp"

std::atomic<bool> g_running{true};
volatile sig_atomic_t g_signal_number = 0;
constexpr const char *LOCK_FILE_PATH = "/var/lock/matt_daemon.lock";
constexpr int PORT = 4242;
constexpr int MAX_CLIENTS = 3;
constexpr int BUFFER_SIZE = 1024;

// Async-signal-safe: only sets a flag (sig_atomic_t assignment is the one
// thing the C++ standard guarantees is safe inside a signal handler).
// The actual logging + exit happen back in work()'s loop once poll()
// returns with EINTR, i.e. in normal execution context.
void handle_signal(int signum) {
    g_signal_number = signum;
}

int g_lock_fd = -1;

// Rewrites the PID currently held in the lock file. Called once before the
// fork() (so a concurrent instance check fails fast) and once again after
// fork()+setsid() so the file ends up holding the real daemon PID rather
// than the parent's, which has already exited by then.
void write_pid_to_lock() {
    if (g_lock_fd < 0)
        return;
    if (ftruncate(g_lock_fd, 0) < 0) {
        std::cerr << "Failed to truncate file: " << std::strerror(errno) << "\n";
        return;
    }
    lseek(g_lock_fd, 0, SEEK_SET);
    std::string pid_str = std::to_string(getpid()) + "\n";
    write(g_lock_fd, pid_str.c_str(), pid_str.size());
}

bool ensure_one_instance() {
    int fd = open(LOCK_FILE_PATH, O_RDWR | O_CREAT, 0640);
    if (fd < 0) {
        std::cerr << "Failed to open lock file: " << std::strerror(errno) << "\n";
        return false;
    }

    if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            std::cerr << "Can't open :/var/lock/matt_daemon.lock\n";
        } else {
            std::cerr << "Failed to lock file: " << std::strerror(errno) << "\n";
        }
        close(fd);
        return false;
    }

    g_lock_fd = fd;
    write_pid_to_lock();
    return true;
}

void work() {
    auto &logger = Tintin_reporter::getInstance();

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        logger.log(Tintin_reporter::Level::ERROR,
                   "Failed to create socket: " + std::string(strerror(errno)));
        return;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0) {
        logger.log(Tintin_reporter::Level::ERROR,
                   "Failed to bind to port " + std::to_string(PORT) + ": " + strerror(errno));
        close(server_fd);
        return;
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        logger.log(Tintin_reporter::Level::ERROR, "Listen failed: " + std::string(strerror(errno)));
        close(server_fd);
        return;
    }

    logger.log(Tintin_reporter::Level::INFO, "Server listening on port " + std::to_string(PORT));

    std::vector<pollfd> fds;

    pollfd server_pfd{};
    server_pfd.fd = server_fd;
    server_pfd.events = POLLIN;
    fds.push_back(server_pfd);

    while (true) {
        int poll_count = poll(fds.data(), fds.size(), -1);
        if (poll_count < 0) {
            if (errno == EINTR) {
                if (g_signal_number != 0) {
                    int sig = g_signal_number;
                    logger.log(Tintin_reporter::Level::INFO,
                               "Received signal " + std::to_string(sig) + " (" +
                                   strsignal(sig) + "), exiting right now...");
                    syslog(LOG_NOTICE, "Received signal %d, exiting", sig);
                    for (const auto &pfd : fds)
                        close(pfd.fd);
                    closelog();
                    exit(EXIT_SUCCESS);
                }
                continue;
            }
            logger.log(Tintin_reporter::Level::ERROR,
                       "Poll error: " + std::string(strerror(errno)));
            break;
        }

        if (fds[0].revents & POLLIN) {
            sockaddr_in client_addr{};
            socklen_t addr_len = sizeof(client_addr);
            int new_client_fd =
                accept(server_fd, reinterpret_cast<sockaddr *>(&client_addr), &addr_len);

            if (new_client_fd >= 0) {
                if (fds.size() - 1 >= MAX_CLIENTS) {
                    logger.log(Tintin_reporter::Level::WARN,
                               "Max clients (3) reached. Rejecting connection from " +
                                   std::string(inet_ntoa(client_addr.sin_addr)));

                    const char *msg = "Server full: Maximum 3 clients connected.\n";
                    write(new_client_fd, msg, strlen(msg));
                    close(new_client_fd);
                } else {
                    pollfd client_pfd{};
                    client_pfd.fd = new_client_fd;
                    client_pfd.events = POLLIN;
                    fds.push_back(client_pfd);

                    logger.log(Tintin_reporter::Level::INFO,
                               "Client connected from " +
                                   std::string(inet_ntoa(client_addr.sin_addr)) +
                                   " [Total: " + std::to_string(fds.size() - 1) + "/3]");
                }
            }
        }

        for (size_t i = 1; i < fds.size();) {
            if (fds[i].revents & POLLIN) {
                char buffer[BUFFER_SIZE];
                ssize_t bytes_read = read(fds[i].fd, buffer, sizeof(buffer) - 1);

                if (bytes_read <= 0) {
                    logger.log(
                        Tintin_reporter::Level::INFO,
                        "Client disconnected. [Total: " + std::to_string(fds.size() - 2) + "/3]");
                    close(fds[i].fd);
                    fds.erase(fds.begin() + i);
                    continue;
                } else {
                    buffer[bytes_read] = '\0';
                    std::string message(buffer);

                    while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) {
                        message.pop_back();
                    }

                    if (!message.empty()) {
                        if (message == "quit") {
                            logger.log(Tintin_reporter::Level::INFO, "Byte Byte.....");
                            exit(EXIT_SUCCESS);
                        }
                        logger.log(Tintin_reporter::Level::INFO, "User input: " + message);
                    }
                }
            }
            ++i;
        }
    }

    for (const auto &pfd : fds) {
        close(pfd.fd);
    }
}

int daemon(void) {
    if (!(geteuid() == 0)) {
        std::println("Matt_daemon requires root privileges.");
        Tintin_reporter::getInstance().log(Tintin_reporter::Level::ERROR,
                                           "Matt_daemon requires root privileges.");
        openlog("Matt_daemon", LOG_PID, LOG_DAEMON);
        syslog(LOG_ERR, "Matt_daemon requires root privileges");
        closelog();
        exit(EXIT_FAILURE);
    }

    if (!Tintin_reporter::getInstance().init()) {
        std::cerr << "Failed to init Tintin_reporter.\n";
        exit(EXIT_FAILURE);
    }

    if (!ensure_one_instance()) {
        openlog("Matt_daemon", LOG_PID, LOG_DAEMON);
        Tintin_reporter::getInstance().log(Tintin_reporter::Level::ERROR,
                                           "An instance of Matt_daemon is already running.");
        syslog(LOG_ERR, "An instance of Matt_daemon is already running.");
        closelog();
        exit(EXIT_FAILURE);
    }

    pid_t pid, sid;

    pid = fork();
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    } else if (pid < 0) {
        exit(EXIT_FAILURE);
    }

    umask(0);
    openlog("Matt_daemon", LOG_NOWAIT | LOG_PID, LOG_USER);
    syslog(LOG_NOTICE, "Successfully started Matt_daemon");
    Tintin_reporter::getInstance().log(Tintin_reporter::Level::INFO,
                                       "Successfully started Matt_daemon");
    sid = setsid();
    if (sid < 0) {
        syslog(LOG_ERR, "Could not generate session ID for child process");
        Tintin_reporter::getInstance().log(Tintin_reporter::Level::ERROR,
                                           "Could not generate session ID for child process");
        exit(EXIT_FAILURE);
    }

    if (chdir("/") < 0) {
        syslog(LOG_ERR, "Could not change working directory to /");
        Tintin_reporter::getInstance().log(Tintin_reporter::Level::ERROR,
                                           "Could not change working directory to /");
        exit(EXIT_FAILURE);
    }

    write_pid_to_lock();

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    while (g_running.load(std::memory_order_relaxed)) {
        work();
    }

    closelog();
    exit(EXIT_SUCCESS);
    return 0;
}

int main(void) {
    struct sigaction sa_stop{};
    struct sigaction sa_ignore{};

    sa_stop.sa_flags = 0;
    sigemptyset(&sa_stop.sa_mask);
    sa_stop.sa_handler = handle_signal;

    sa_ignore.sa_flags = 0;
    sigemptyset(&sa_ignore.sa_mask);
    sa_ignore.sa_handler = SIG_IGN;

    sigaction(SIGTERM, &sa_stop, nullptr);
    sigaction(SIGINT, &sa_stop, nullptr);
    // Ignore SIGPIPE: a client that drops its connection right before we
    // write() to its socket would otherwise raise SIGPIPE, whose default
    // action kills the whole daemon (violates "never quit unexpectedly").
    sigaction(SIGPIPE, &sa_ignore, nullptr);
    daemon();
    return (0);
}
