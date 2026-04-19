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

    std::cout << "[" << acc->typeName() << "]"
              << "  topic=" << topicName
              << "  bytes=" << data.size()
              << "  tag=0x" << std::hex << std::setw(8) << std::setfill('0') << tag
              << std::dec << "\n";

    writer.write(topicName, json);
    arcal::arcalDestroyAccessor(acc);
}
