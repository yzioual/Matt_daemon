# Matt_daemon

A Unix daemon written in C++23 featuring a TCP server, single-instance enforcement, and custom logging.

## Features

- **Daemonization**: Detaches from the controlling terminal (`fork`, `setsid`, `umask`, `chdir`).
- **Single Instance**: Enforces a single running process using `/var/lock/matt_daemon.lock`.
- **TCP Server**: Listens on port `4242` using `poll()`, supporting up to 3 concurrent clients.
- **Logging**: Records events and client messages to `/var/log/matt_daemon/matt_daemon.log` and `syslog`.
- **Clean Termination**: Shuts down cleanly on signals (`SIGINT`, `SIGTERM`, `SIGQUIT`, `SIGHUP`) or via client `quit` command.

## Requirements

- C++23 compiler (`g++` or `clang++`)
- Make
- Linux environment with root privileges

## Build

```bash
make
```

Cleaning targets:
- `make clean` — Remove object files.
- `make fclean` — Remove object files and binary.
- `make re` — Rebuild from scratch.

## Usage

1. **Run the daemon** (root required):
   ```bash
   sudo ./Matt_daemon
   ```

2. **Connect to the server**:
   ```bash
   nc 127.0.0.1 4242
   ```

3. **Commands**:
   - Send any text message to write it to the log file.
   - Send `quit` to cleanly stop the daemon.

4. **View logs**:
   ```bash
   tail -f /var/log/matt_daemon/matt_daemon.log
   ```
