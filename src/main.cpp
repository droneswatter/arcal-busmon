#include "DecodeChain.h"
#include "LogWriter.h"
#include "StreamPipe.h"
#include "TopicMonitor.h"

#include "uci/base/ExternalizerLoader.h"

#include <dds/dds.h>

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

static TopicMonitor* g_monitor = nullptr;

static void sighandler(int) {
    std::cout << "\n[busmon] stopping...\n";
    if (g_monitor) g_monitor->stop();
}

static void usage(const char* prog) {
    std::cerr << "usage: " << prog
              << " [--log-dir DIR] [--domain ID] [--duration SECONDS]"
              << " [--stream FIFO_PATH]\n";
}

int main(int argc, char** argv) {
    std::string logDir     = "busmon-out";
    int         domainId   = 0;
    int         duration   = 0;
    std::string streamPath;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--log-dir") == 0 && i + 1 < argc) {
            logDir = argv[++i];
        } else if (std::strcmp(argv[i], "--domain") == 0 && i + 1 < argc) {
            domainId = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--duration") == 0 && i + 1 < argc) {
            duration = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--stream") == 0 && i + 1 < argc) {
            streamPath = argv[++i];
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    dds_entity_t participant = dds_create_participant(
        static_cast<dds_domainid_t>(domainId), nullptr, nullptr);
    if (participant < 0) {
        std::cerr << "[busmon] failed to create DDS participant\n";
        return 1;
    }

    auto* loader = uci_getExternalizerLoader();
    auto* jsonExt = loader->getExternalizer("JSON", "2.5.0", "2.5.0");
    if (!jsonExt) {
        std::cerr << "[busmon] JSON externalizer not available\n";
        dds_delete(participant);
        return 1;
    }

    try {
        LogWriter   logWriter(logDir);
        DecodeChain decoder(jsonExt);

        std::unique_ptr<StreamPipe> pipe;
        if (!streamPath.empty()) {
            pipe = std::make_unique<StreamPipe>(streamPath);
            decoder.setStream(pipe.get());
            std::cout << "[busmon] streaming metadata to " << streamPath << "\n";
        }

        TopicMonitor monitor(participant, decoder, logWriter);

        g_monitor = &monitor;
        std::signal(SIGINT,  sighandler);
        std::signal(SIGTERM, sighandler);

        std::cout << "[busmon] domain=" << domainId
                  << " log-dir=" << logDir;
        if (duration > 0) std::cout << " duration=" << duration << "s";
        std::cout << "\n";

        monitor.run(duration);
    } catch (const std::exception& e) {
        std::cerr << "[busmon] fatal: " << e.what() << "\n";
        dds_delete(participant);
        return 1;
    }

    g_monitor = nullptr;
    uci_destroyExternalizerLoader(loader);
    dds_delete(participant);
    return 0;
}
