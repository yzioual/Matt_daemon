#ifndef INSTANCE_LOCK_HPP
#define INSTANCE_LOCK_HPP

#include <string>

// RAII guard around a PID lock file, used to enforce that only one
// Matt_daemon instance runs at a time (flock, not LOCK_SH per subject).
class InstanceLock {
public:
    InstanceLock() = default;
    ~InstanceLock();

    InstanceLock(const InstanceLock&) = delete;
    InstanceLock& operator=(const InstanceLock&) = delete;

    // Opens/creates path and takes an exclusive, non-blocking flock.
    // Returns false (and prints to stderr) if another instance already
    // holds the lock, or on I/O error.
    bool acquire(const std::string& path);

    // Rewrites the PID currently stored in the lock file with getpid().
    // Called once on acquire() and again after fork()+setsid(), so the
    // file ends up holding the real daemon PID rather than the parent's
    // (which has already exited by then).
    void refreshPid();

private:
    int fd_ = -1;
    std::string path_;
};

#endif
