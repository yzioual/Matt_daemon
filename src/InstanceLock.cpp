#include "InstanceLock.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/file.h>
#include <unistd.h>

bool InstanceLock::acquire(const std::string& path) {
    int fd = open(path.c_str(), O_RDWR | O_CREAT, 0640);
    if (fd < 0) {
        std::cerr << "Failed to open lock file: " << std::strerror(errno) << "\n";
        return false;
    }

    if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            std::cerr << "Can't open :" << path << "\n";
        } else {
            std::cerr << "Failed to lock file: " << std::strerror(errno) << "\n";
        }
        close(fd);
        return false;
    }

    fd_ = fd;
    path_ = path;
    refreshPid();
    return true;
}

void InstanceLock::refreshPid() {
    if (fd_ < 0)
        return;
    if (ftruncate(fd_, 0) < 0) {
        std::cerr << "Failed to truncate file: " << std::strerror(errno) << "\n";
        return;
    }
    lseek(fd_, 0, SEEK_SET);
    std::string pidStr = std::to_string(getpid()) + "\n";
    write(fd_, pidStr.c_str(), pidStr.size());
}

InstanceLock::~InstanceLock() {
    if (fd_ >= 0) {
        flock(fd_, LOCK_UN);
        close(fd_);
        unlink(path_.c_str());
    }
}
