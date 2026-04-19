#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

class LogWriter {
public:
    explicit LogWriter(std::string logDir);

    // Writes json to <logDir>/<topicName>/<seq>.json.
    // Creates the per-topic directory on first write. Thread-safe.
    void write(const std::string& topicName, const std::string& json);

private:
    static std::string sanitize(const std::string& topicName);

    std::string logDir_;
    std::unordered_map<std::string, uint64_t> seqNums_;
    std::mutex mu_;
};
