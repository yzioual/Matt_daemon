#include "TcpServer.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <syslog.h>
#include <unistd.h>

#include "SignalGuard.hpp"
#include "Tintin_reporter.hpp"

namespace {
constexpr int BUFFER_SIZE = 1024;
}

TcpServer::TcpServer(int port, std::size_t maxClients) : port_(port), maxClients_(maxClients) {}

TcpServer::~TcpServer() {
    closeAll();
}

bool TcpServer::start() {
    auto& logger = Tintin_reporter::getInstance();

    serverFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd_ < 0) {
        logger.log(Tintin_reporter::Level::ERROR,
                   "Failed to create socket: " + std::string(strerror(errno)));
        return false;
    }

    int opt = 1;
    setsockopt(serverFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);

    if (bind(serverFd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        logger.log(Tintin_reporter::Level::ERROR,
                   "Failed to bind to port " + std::to_string(port_) + ": " + strerror(errno));
        close(serverFd_);
        serverFd_ = -1;
        return false;
    }

    if (listen(serverFd_, static_cast<int>(maxClients_)) < 0) {
        logger.log(Tintin_reporter::Level::ERROR, "Listen failed: " + std::string(strerror(errno)));
        close(serverFd_);
        serverFd_ = -1;
        return false;
    }

    logger.log(Tintin_reporter::Level::INFO, "Server listening on port " + std::to_string(port_));

    pollfd serverPfd{};
    serverPfd.fd = serverFd_;
    serverPfd.events = POLLIN;
    fds_.push_back(serverPfd);
    return true;
}

void TcpServer::acceptClient() {
    auto& logger = Tintin_reporter::getInstance();

    sockaddr_in clientAddr{};
    socklen_t addrLen = sizeof(clientAddr);
    int newFd = accept(serverFd_, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
    if (newFd < 0)
        return;

    if (fds_.size() - 1 >= maxClients_) {
        logger.log(Tintin_reporter::Level::WARN,
                   "Max clients (" + std::to_string(maxClients_) + ") reached. Rejecting connection from " +
                       std::string(inet_ntoa(clientAddr.sin_addr)));

        const std::string msg =
            "Server full: Maximum " + std::to_string(maxClients_) + " clients connected.\n";
        write(newFd, msg.c_str(), msg.size());
        close(newFd);
        return;
    }

    pollfd clientPfd{};
    clientPfd.fd = newFd;
    clientPfd.events = POLLIN;
    fds_.push_back(clientPfd);

    logger.log(Tintin_reporter::Level::INFO,
               "Client connected from " + std::string(inet_ntoa(clientAddr.sin_addr)) + " [Total: " +
                   std::to_string(fds_.size() - 1) + "/" + std::to_string(maxClients_) + "]");
}

TcpServer::ClientEvent TcpServer::handleClient(std::size_t index) {
    auto& logger = Tintin_reporter::getInstance();

    char buffer[BUFFER_SIZE];
    ssize_t bytesRead = read(fds_[index].fd, buffer, sizeof(buffer) - 1);

    if (bytesRead <= 0) {
        logger.log(Tintin_reporter::Level::INFO,
                   "Client disconnected. [Total: " + std::to_string(fds_.size() - 2) + "/" +
                       std::to_string(maxClients_) + "]");
        close(fds_[index].fd);
        fds_.erase(fds_.begin() + static_cast<long>(index));
        return ClientEvent::Disconnected;
    }

    buffer[bytesRead] = '\0';
    std::string message(buffer);
    while (!message.empty() && (message.back() == '\n' || message.back() == '\r'))
        message.pop_back();

    if (message.empty())
        return ClientEvent::Continue;

    if (message == "quit") {
        logger.log(Tintin_reporter::Level::INFO, "Request quit.");
        return ClientEvent::QuitRequested;
    }

    logger.log(Tintin_reporter::Level::INFO, "User input: " + message);
    return ClientEvent::Continue;
}

void TcpServer::closeAll() {
    for (const auto& pfd : fds_)
        close(pfd.fd);
    fds_.clear();
    serverFd_ = -1;
}

TcpServer::StopReason TcpServer::run() {
    auto& logger = Tintin_reporter::getInstance();

    while (true) {
        int pollCount = poll(fds_.data(), fds_.size(), -1);
        if (pollCount < 0) {
            if (errno == EINTR) {
                int sig = SignalGuard::consumeSignal();
                if (sig != 0) {
                    logger.log(Tintin_reporter::Level::INFO,
                               "Received signal " + std::to_string(sig) + " (" + strsignal(sig) +
                                   "), exiting right now...");
                    syslog(LOG_NOTICE, "Received signal %d, exiting", sig);
                    return StopReason::Signal;
                }
                continue;
            }
            logger.log(Tintin_reporter::Level::ERROR, "Poll error: " + std::string(strerror(errno)));
            return StopReason::FatalError;
        }

        if (fds_[0].revents & POLLIN)
            acceptClient();

        for (std::size_t i = 1; i < fds_.size();) {
            if (fds_[i].revents & POLLIN) {
                switch (handleClient(i)) {
                    case ClientEvent::QuitRequested:
                        logger.log(Tintin_reporter::Level::INFO, "Quitting.");
                        return StopReason::Quit;
                    case ClientEvent::Disconnected:
                        continue;  // fds_[i] was erased; don't advance i.
                    case ClientEvent::Continue:
                        break;
                }
            }
            ++i;
        }
    }
}
