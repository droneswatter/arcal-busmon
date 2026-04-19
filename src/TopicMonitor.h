#pragma once

#include <atomic>
#include <string>
#include <unordered_map>

#include <dds/dds.h>

class DecodeChain;
class LogWriter;

// Subscribes to DDS_BUILTIN_TOPIC_DCPSPUBLICATION, creates an arcal_payload
// reader for each discovered arcal topic, and drives the decode pipeline.
// All work runs on the thread that calls run().
class TopicMonitor {
public:
    TopicMonitor(dds_entity_t participant, DecodeChain& decoder, LogWriter& writer);
    ~TopicMonitor();

    // Block until durationSecs elapses (0 = run until stop()).
    void run(int durationSecs);
    void stop();

private:
    void pollBuiltin();
    void ensureReader(const std::string& topicName);
    void drainPayloadReader(dds_entity_t reader, const std::string& topicName);

    dds_entity_t                              participant_;
    dds_entity_t                              builtinReader_;
    dds_entity_t                              builtinCond_;
    dds_entity_t                              waitset_;
    DecodeChain&                              decoder_;
    LogWriter&                                writer_;
    std::atomic<bool>                         running_{false};

    // topic name → payload reader entity
    std::unordered_map<std::string, dds_entity_t> readers_;
    // condition entity → topic name (for dispatch after waitset_wait)
    std::unordered_map<dds_entity_t, std::string>  condTopics_;
    // topic name → message count (for stdout summary)
    std::unordered_map<std::string, uint64_t>       msgCounts_;
};
