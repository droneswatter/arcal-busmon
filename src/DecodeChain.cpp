#include "DecodeChain.h"
#include "LogWriter.h"

#include "arcal/AccessorFactory.h"
#include "arcal/CdrBridge.h"
#include "uci/base/Externalizer.h"

#include <cstdint>
#include <iostream>
#include <iomanip>
#include <string>

DecodeChain::DecodeChain(uci::base::Externalizer* jsonExt) : jsonExt_(jsonExt) {}

void DecodeChain::decode(const std::string& topicName,
                         const std::vector<uint8_t>& payload,
                         LogWriter& writer) {
    if (payload.size() < 4) {
        std::cerr << "[busmon] payload too short on topic=" << topicName << "\n";
        return;
    }

    const uint32_t tag = static_cast<uint32_t>(payload[0])
                       | static_cast<uint32_t>(payload[1]) << 8
                       | static_cast<uint32_t>(payload[2]) << 16
                       | static_cast<uint32_t>(payload[3]) << 24;

    uci::base::Accessor* acc = arcal::arcalCreateAccessor(tag);
    if (!acc) {
        std::cerr << "[busmon] unknown type tag 0x" << std::hex << std::setw(8)
                  << std::setfill('0') << tag << " on topic=" << topicName << "\n";
        return;
    }

    arcal::cdrDeserialize(payload, *acc);

    std::string json;
    jsonExt_->write(*acc, json);

    std::cout << "[" << acc->typeName() << "]"
              << "  topic=" << topicName
              << "  bytes=" << payload.size()
              << "  tag=0x" << std::hex << std::setw(8) << std::setfill('0') << tag
              << std::dec << "\n";

    writer.write(topicName, json);
    arcal::arcalDestroyAccessor(acc);
}
