#include "StreamPipe.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>

StreamPipe::StreamPipe(const std::string& path) : path_(path) {
    if (::mkfifo(path.c_str(), 0600) < 0 && errno != EEXIST)
        std::cerr << "[busmon] mkfifo " << path << ": " << strerror(errno) << "\n";
}

StreamPipe::~StreamPipe() {
    if (fd_ >= 0) ::close(fd_);
    ::unlink(path_.c_str());
}

void StreamPipe::tryOpen() {
    fd_ = ::open(path_.c_str(), O_WRONLY | O_NONBLOCK);
    // ENXIO = no reader yet; that's expected, stay fd_=-1
}

void StreamPipe::publish(const std::string& line) {
    if (fd_ < 0) tryOpen();
    if (fd_ < 0) return;

    std::string buf = line + "\n";
    const char* ptr = buf.c_str();
    ssize_t     rem = static_cast<ssize_t>(buf.size());

    while (rem > 0) {
        ssize_t n = ::write(fd_, ptr, static_cast<size_t>(rem));
        if (n < 0) {
            if (errno == EINTR) continue;
            // EAGAIN/EWOULDBLOCK = pipe buffer full — drop this message
            // EPIPE/EBADF        = reader disconnected
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                ::close(fd_);
                fd_ = -1;
            }
            return;
        }
        ptr += n;
        rem -= n;
    }
}
