#pragma once
#include <string>

// Non-blocking FIFO writer for live metadata streaming.
// Creates the named pipe on construction; publish() drops silently if no
// reader is connected or the pipe buffer is full.
class StreamPipe {
public:
    explicit StreamPipe(const std::string& path);
    ~StreamPipe();

    // Write one NDJSON line.  Never blocks; drops on EAGAIN or EPIPE.
    void publish(const std::string& line);

private:
    void tryOpen();

    std::string path_;
    int         fd_{-1};
};
