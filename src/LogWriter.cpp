#include "LogWriter.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

LogWriter::LogWriter(std::string logDir) : logDir_(std::move(logDir)) {
    std::filesystem::create_directories(logDir_);
}

uint64_t LogWriter::write(const std::string& topicName, const std::string& json) {
    std::lock_guard<std::mutex> lk(mu_);

    const std::string safe = sanitize(topicName);
    const uint64_t seq = ++seqNums_[safe];

    const std::filesystem::path dir = std::filesystem::path(logDir_) / safe;
    std::filesystem::create_directories(dir);

    std::ostringstream filename;
    filename << std::setw(10) << std::setfill('0') << seq << ".json";

    std::ofstream out(dir / filename.str(), std::ios::binary);
    if (!out) throw std::runtime_error("LogWriter: cannot open " + filename.str());
    out << json;
    return seq;
}

std::string LogWriter::sanitize(const std::string& topicName) {
    std::string s = topicName;
    for (char& c : s) {
        if (c == '/' || c == ':' || c == '\\') c = '_';
    }
    return s;
}
