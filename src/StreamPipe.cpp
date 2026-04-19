#include "StreamPipe.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>
#include <sstream>

StreamPipe::StreamPipe(const std::string& path, bool debug)
    : path_(path)
    , debug_(debug) {
    if (::mkfifo(path.c_str(), 0600) < 0 && errno != EEXIST)
        std::cerr << "[busmon] mkfifo " << path << ": " << strerror(errno) << "\n";
    if (debug_)
        debugLog("created path=" + path_);
}

StreamPipe::~StreamPipe() {
    debugSummary();
    if (fd_ >= 0) ::close(fd_);
    ::unlink(path_.c_str());
}

void StreamPipe::tryOpen() {
    ++openAttempts_;
    fd_ = ::open(path_.c_str(), O_WRONLY | O_NONBLOCK);
    if (fd_ >= 0) {
        ++openSuccesses_;
        debugLog("open ok fd=" + std::to_string(fd_));
        return;
    }

    ++openFailures_;
    if (errno == ENXIO) {
        // ENXIO = no reader yet; that's expected, stay fd_=-1.
        debugLog("open no-reader errno=ENXIO attempts=" + std::to_string(openAttempts_));
    } else {
        debugLog(std::string("open failed errno=") + std::to_string(errno) +
                 " error=" + strerror(errno));
    }
}

bool StreamPipe::publish(const std::string& line) {
    ++publishAttempts_;
    if (fd_ < 0) tryOpen();
    if (fd_ < 0) {
        ++dropNoReader_;
        debugSummary();
        return false;
    }

    std::string buf = line + "\n";
    const char* ptr = buf.c_str();
    ssize_t     rem = static_cast<ssize_t>(buf.size());
    ssize_t     totalWritten = 0;

    while (rem > 0) {
        ssize_t n = ::write(fd_, ptr, static_cast<size_t>(rem));
        if (n < 0) {
            if (errno == EINTR) continue;
            // EAGAIN/EWOULDBLOCK = pipe buffer full — drop this message
            // EPIPE/EBADF        = reader disconnected
            const int savedErrno = errno;
            if (savedErrno == EAGAIN || savedErrno == EWOULDBLOCK) {
                ++dropWouldBlock_;
            } else if (savedErrno == EPIPE || savedErrno == EBADF) {
                ++dropDisconnected_;
            } else {
                ++dropOtherError_;
            }
            debugLog(std::string("write failed fd=") + std::to_string(fd_) +
                     " total_written=" + std::to_string(totalWritten) +
                     " remaining=" + std::to_string(rem) +
                     " errno=" + std::to_string(savedErrno) +
                     " error=" + strerror(savedErrno));
            if (savedErrno != EAGAIN && savedErrno != EWOULDBLOCK) {
                ::close(fd_);
                fd_ = -1;
            }
            debugSummary();
            return false;
        }
        if (n < rem)
            ++partialWrites_;
        ptr += n;
        rem -= n;
        totalWritten += n;
    }

    ++publishSuccesses_;
    debugLog("write ok fd=" + std::to_string(fd_) +
             " bytes=" + std::to_string(totalWritten) +
             " successes=" + std::to_string(publishSuccesses_));
    debugSummary();
    return true;
}

void StreamPipe::debugLog(const std::string& message) const {
    if (!debug_) return;
    std::cerr << "[busmon:stream] " << message << "\n";
}

void StreamPipe::debugSummary() {
    if (!debug_) return;

    static auto lastSummary = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    if (now - lastSummary < std::chrono::seconds(1))
        return;
    lastSummary = now;

    std::ostringstream oss;
    oss << "summary"
        << " open_attempts=" << openAttempts_
        << " open_ok=" << openSuccesses_
        << " open_fail=" << openFailures_
        << " publish_attempts=" << publishAttempts_
        << " publish_ok=" << publishSuccesses_
        << " drop_no_reader=" << dropNoReader_
        << " drop_would_block=" << dropWouldBlock_
        << " drop_disconnected=" << dropDisconnected_
        << " drop_other=" << dropOtherError_
        << " partial_writes=" << partialWrites_
        << " fd=" << fd_;
    debugLog(oss.str());
}
