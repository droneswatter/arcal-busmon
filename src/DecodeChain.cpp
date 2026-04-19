#include "DecodeChain.h"
#include "LogWriter.h"
#include "StreamPipe.h"

#include "arcal/AccessorFactory.h"
#include "arcal/CdrBridge.h"
#include "uci/base/Externalizer.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>

DecodeChain::DecodeChain(uci::base::Externalizer* jsonExt) : jsonExt_(jsonExt) {}

void DecodeChain::decode(const std::string& topicName,
                         uint32_t tag,
                         const std::vector<uint8_t>& data,
                         LogWriter& writer) {
    uci::base::Accessor* acc = arcal::arcalCreateAccessor(tag);
    if (!acc) {
        std::cerr << "[busmon] unknown type tag 0x" << std::hex << std::setw(8)
                  << std::setfill('0') << tag << " on topic=" << topicName << "\n";
        return;
    }

    arcal::cdrDeserialize(tag, data, *acc);

    std::string json;
    jsonExt_->write(*acc, json);

    const std::string typeName = acc->typeName();

    std::cout << "[" << typeName << "]"
              << "  topic=" << topicName
              << "  bytes=" << data.size()
              << "  tag=0x" << std::hex << std::setw(8) << std::setfill('0') << tag
              << std::dec << "\n";

    const uint64_t seq = writer.write(topicName, json);

    if (stream_) {
        const int64_t ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        std::ostringstream oss;
        oss << "{\"topic\":\"" << topicName << "\""
            << ",\"type\":\""  << typeName  << "\""
            << ",\"seq\":"     << seq
            << ",\"bytes\":"   << data.size()
            << ",\"tag\":\"0x" << std::hex << std::setw(8) << std::setfill('0') << tag << "\""
            << std::dec
            << ",\"ts_ms\":"   << ts_ms
            << "}";
        stream_->publish(oss.str());
    }

    arcal::arcalDestroyAccessor(acc);
}
