#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace uci { namespace base { class Externalizer; } }
class LogWriter;

class DecodeChain {
public:
    // jsonExt must outlive this object.
    explicit DecodeChain(uci::base::Externalizer* jsonExt);

    // Create accessor for tag, CDR-deserialize data, JSON-serialize, write to log.
    // Drops payload with a stderr message if the tag is unknown.
    void decode(const std::string& topicName,
                uint32_t tag,
                const std::vector<uint8_t>& data,
                LogWriter& writer);

private:
    uci::base::Externalizer* jsonExt_;
};
