#pragma once
#include <cstdint>
#include <string>

// Non-blocking FIFO writer for live metadata streaming.
// Creates the named pipe on construction; publish() drops silently if no
// reader is connected or the pipe buffer is full.
class StreamPipe {
public:
    explicit StreamPipe(const std::string& path, bool debug = false);
    ~StreamPipe();

    // Write one NDJSON line.  Never blocks; drops on EAGAIN or EPIPE.
    bool publish(const std::string& line);

private:
    void tryOpen();
    void debugLog(const std::string& message) const;
    void debugSummary();

    std::string path_;
    bool        debug_{false};
    int         fd_{-1};

    uint64_t openAttempts_{0};
    uint64_t openSuccesses_{0};
    uint64_t openFailures_{0};
    uint64_t publishAttempts_{0};
    uint64_t publishSuccesses_{0};
    uint64_t dropNoReader_{0};
    uint64_t dropWouldBlock_{0};
    uint64_t dropDisconnected_{0};
    uint64_t dropOtherError_{0};
    uint64_t partialWrites_{0};
};
