#include "Daemon.hpp"
#include "SignalGuard.hpp"

namespace {
constexpr int PORT = 4242;
constexpr int MAX_CLIENTS = 3;
constexpr const char* LOCK_FILE_PATH = "/var/lock/matt_daemon.lock";
}  // namespace

int main(void) {
    SignalGuard::install();
    Daemon daemon(PORT, MAX_CLIENTS, LOCK_FILE_PATH);
    return daemon.run();
}
