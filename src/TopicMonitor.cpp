#include "TopicMonitor.h"
#include "DecodeChain.h"
#include "LogWriter.h"

#include "arcal_payload.h"  // arcal_dds_OpaquePayload + descriptor

#include <chrono>
#include <cstring>
#include <iostream>
#include <vector>

static constexpr const char* kArcalTypeName = "arcal_dds::OpaquePayload";

TopicMonitor::TopicMonitor(dds_entity_t participant,
                           DecodeChain& decoder,
                           LogWriter& writer)
    : participant_(participant)
    , decoder_(decoder)
    , writer_(writer)
{
    builtinReader_ = dds_create_reader(participant_,
                                       DDS_BUILTIN_TOPIC_DCPSPUBLICATION,
                                       nullptr, nullptr);
    if (builtinReader_ < 0)
        throw std::runtime_error("TopicMonitor: failed to create builtin reader");

    builtinCond_ = dds_create_readcondition(builtinReader_, DDS_ANY_STATE);

    waitset_ = dds_create_waitset(participant_);
    dds_waitset_attach(waitset_, builtinCond_,
                       static_cast<dds_attach_t>(builtinReader_));
}

TopicMonitor::~TopicMonitor() {
    dds_delete(waitset_);
    for (auto& [name, reader] : readers_) dds_delete(reader);
    dds_delete(builtinCond_);
    dds_delete(builtinReader_);
}

void TopicMonitor::stop() {
    running_ = false;
}

void TopicMonitor::run(int durationSecs) {
    running_ = true;

    auto deadline = durationSecs > 0
        ? std::chrono::steady_clock::now() + std::chrono::seconds(durationSecs)
        : std::chrono::steady_clock::time_point::max();

    constexpr int kMaxTriggers = 32;
    dds_attach_t triggered[kMaxTriggers];

    while (running_) {
        if (std::chrono::steady_clock::now() >= deadline) break;

        int n = dds_waitset_wait(waitset_, triggered, kMaxTriggers,
                                 DDS_MSECS(500));
        if (n < 0) break; // participant deleted or error

        for (int i = 0; i < n; ++i) {
            auto reader = static_cast<dds_entity_t>(triggered[i]);
            if (reader == builtinReader_) {
                pollBuiltin();
            } else {
                auto it = condTopics_.find(reader);
                if (it != condTopics_.end())
                    drainPayloadReader(reader, it->second);
            }
        }
    }
}

void TopicMonitor::pollBuiltin() {
    static constexpr int kBatch = 16;
    void* ptrs[kBatch] = {};
    dds_sample_info_t info[kBatch];

    int n = dds_read(builtinReader_, ptrs, info, kBatch, kBatch);
    for (int i = 0; i < n; ++i) {
        if (!info[i].valid_data) continue;
        auto* ep = static_cast<dds_builtintopic_endpoint_t*>(ptrs[i]);
        if (ep->topic_name && ep->type_name
                && std::strcmp(ep->type_name, kArcalTypeName) == 0) {
            std::cout << "[busmon] discovered topic=" << ep->topic_name << "\n";
            ensureReader(ep->topic_name);
        }
    }
    dds_return_loan(builtinReader_, ptrs, n);
}

void TopicMonitor::ensureReader(const std::string& topicName) {
    if (readers_.count(topicName)) return;

    dds_entity_t topic = dds_create_topic(participant_,
                                           &arcal_dds_OpaquePayload_desc,
                                           topicName.c_str(),
                                           nullptr, nullptr);
    if (topic < 0) {
        std::cerr << "[busmon] failed to create topic for " << topicName << "\n";
        return;
    }

    dds_entity_t reader = dds_create_reader(participant_, topic,
                                             nullptr, nullptr);
    dds_delete(topic); // reader holds a reference; topic handle no longer needed
    if (reader < 0) {
        std::cerr << "[busmon] failed to create reader for " << topicName << "\n";
        return;
    }

    dds_entity_t cond = dds_create_readcondition(reader, DDS_ANY_STATE);
    dds_waitset_attach(waitset_, cond, static_cast<dds_attach_t>(cond));

    readers_[topicName]    = reader;
    condTopics_[cond]      = topicName;
    msgCounts_[topicName]  = 0;
}

void TopicMonitor::drainPayloadReader(dds_entity_t reader,
                                      const std::string& topicName) {
    static constexpr int kBatch = 16;
    void* ptrs[kBatch] = {};
    dds_sample_info_t info[kBatch];

    int n = dds_take(reader, ptrs, info, kBatch, kBatch);
    for (int i = 0; i < n; ++i) {
        if (!info[i].valid_data) continue;
        auto* sample = static_cast<arcal_dds_OpaquePayload*>(ptrs[i]);
        std::vector<uint8_t> bytes(
            sample->data._buffer,
            sample->data._buffer + sample->data._length);
        decoder_.decode(topicName, bytes, writer_);
        ++msgCounts_[topicName];
    }
    dds_return_loan(reader, ptrs, n);
}
