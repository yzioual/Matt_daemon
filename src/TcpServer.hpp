#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP

#include <cstddef>
#include <poll.h>
#include <vector>

// Minimal TCP server used by Matt_daemon: binds a single listening socket,
// accepts up to maxClients concurrent connections, and logs incoming
// lines. Sending "quit" on any client connection stops the whole server
// (see subject: "the program must quit...", not just that one client).
class TcpServer {
public:
    enum class StopReason { Quit, Signal, FatalError };

    TcpServer() = delete;
    TcpServer(int port, std::size_t maxClients);
    ~TcpServer();

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    bool start();
    // Blocks until a client sends "quit", a signal is caught (see
    // SignalGuard), or an unrecoverable poll() error occurs.
    StopReason run();

private:
    enum class ClientEvent { Continue, Disconnected, QuitRequested };

    int port_;
    std::size_t maxClients_;
    int serverFd_ = -1;
    std::vector<pollfd> fds_;

    void acceptClient();
    ClientEvent handleClient(std::size_t index);
    void closeAll();
};

#endif
