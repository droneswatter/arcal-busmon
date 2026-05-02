// Continuous multi-topic UCI traffic generator.
// Keeps one DDS participant alive and publishes several common UCI message
// types at configurable rates so bus monitors can exercise multi-topic streaming.
//
// Usage:
//   uci_traffic_gen [--rate MSG/S] [--position-rate MSG/S]
//                   [--system-rate MSG/S] [--service-rate MSG/S]
//                   [--subsystem-rate MSG/S] [--count N]
//
//   --rate            alias for --position-rate, for compatibility
//   --position-rate   PositionReport messages per second  (default: 10)
//   --system-rate     SystemStatus messages per second    (default: 1)
//   --service-rate    ServiceStatus messages per second   (default: 2)
//   --subsystem-rate  SubsystemStatus messages per second (default: 0.5)
//   --count           total messages across all topics; 0 = run until Ctrl-C

#include "uci/base/AbstractServiceBusConnection.h"
#include "uci/base/UUID.h"
#include "uci/type/PositionReportMT.h"
#include "uci/type/ServiceID_Type.h"
#include "uci/type/ServiceStatusMT.h"
#include "uci/type/SubsystemID_Type.h"
#include "uci/type/SubsystemStateEnum.h"
#include "uci/type/SubsystemStatusMT.h"
#include "uci/type/SystemStatusMT.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

namespace {

constexpr const char* kSystemUuid    = "11111111-1111-1111-1111-111111111111";
constexpr const char* kSystemLabel   = "SENSOR-PLATFORM-ALPHA";
constexpr const char* kServiceUuid   = "22222222-2222-2222-2222-222222222222";
constexpr const char* kServiceLabel  = "BUSMON-DEMO-SERVICE";
constexpr const char* kMissionUuid   = "33333333-3333-3333-3333-333333333333";
constexpr const char* kSubsystemUuid  = "55555555-5555-5555-5555-555555555555";
constexpr const char* kSubsystemLabel = "NAV-SUBSYSTEM";
constexpr double kDegToRad = 0.017453292519943295769;

std::atomic<bool> g_running{true};

void sighandler(int) {
    g_running = false;
}

int64_t nowNs() {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
}

std::chrono::microseconds periodFor(double rate) {
    return std::chrono::microseconds(
        static_cast<int64_t>(1'000'000.0 / std::max(rate, 0.001)));
}

template <typename IdType>
void fillId(IdType& id, const char* uuid, const char* label) {
    id.setUUID(uci::base::UUID(uuid));
    id.enableDescriptiveLabel() = label;
}

template <typename MessageType>
void fillHeader(MessageType& msg) {
    auto& hdr = msg.getMessageHeader();
    fillId(hdr.getSystemID(), kSystemUuid, kSystemLabel);
    hdr.getTimestamp().setValue(nowNs());
    hdr.getSchemaVersion() = "2.5.0";
    hdr.getMode().setValue(uci::type::MessageModeEnum::SIMULATION);
    fillId(hdr.enableServiceID(), kServiceUuid, kServiceLabel);
    hdr.enableMissionID().setUUID(uci::base::UUID(kMissionUuid));
}

void sendPositionReport(uint64_t seq,
                        uci::type::PositionReportMT::Writer& writer,
                        uci::base::AbstractServiceBusConnection* asb) {
    auto& msg = uci::type::PositionReportMT::create(asb);
    fillHeader(msg);

    const int64_t ts = nowNs();
    const double phase = static_cast<double>(seq) * 0.08;

    auto& data = msg.getMessageData();
    fillId(data.getSystemID(), kSystemUuid, kSystemLabel);
    data.enableDisplayName() = "Sensor Platform Alpha";
    data.getSource().setValue(uci::type::SystemSourceEnum::ACTUAL);
    data.getCurrentOperatingDomain().setValue(uci::type::EnvironmentEnum::AIR);
    data.enableTimestamp().setValue(ts);
    data.enableSimulationTargetNumber() = static_cast<int64_t>(seq + 1);

    auto& position = data.getInertialState().getPosition();
    position.getLatitude().setValue((38.85 + 0.01 * std::sin(phase)) * kDegToRad);
    position.getLongitude().setValue((-77.04 + 0.01 * std::cos(phase)) * kDegToRad);
    position.getAltitude().setValue(12000.0 + 100.0 * std::sin(phase * 0.5));
    position.enableAltitudeReference().setValue(uci::type::AltitudeReferenceEnum::WGS_HAE);
    position.getTimestamp().setValue(ts);

    auto& velocity = data.getInertialState().enableDomainVelocity();
    velocity.getNorthSpeed().setValue(115.0 + 3.0 * std::cos(phase));
    velocity.getEastSpeed().setValue(42.0 + 3.0 * std::sin(phase));
    velocity.getDownSpeed().setValue(-0.4 * std::sin(phase * 0.5));
    velocity.enableTimestamp().setValue(ts);

    writer.write(msg);
    uci::type::PositionReportMT::destroy(msg);
}

void sendSystemStatus(uint64_t seq,
                      uci::type::SystemStatusMT::Writer& writer,
                      uci::base::AbstractServiceBusConnection* asb) {
    auto& msg = uci::type::SystemStatusMT::create(asb);
    fillHeader(msg);

    auto& data = msg.getMessageData();
    fillId(data.getSystemID(), kSystemUuid, kSystemLabel);
    data.getSystemState().setValue((seq % 30 == 29)
        ? uci::type::SystemStateEnum::DEGRADED
        : uci::type::SystemStateEnum::OPERATIONAL);
    data.getSource().setValue(uci::type::SystemSourceEnum::ACTUAL);
    data.enableModel() = "DEMO-UAS";

    auto& service = uci::type::ServiceID_Type::create(asb);
    fillId(service, kServiceUuid, kServiceLabel);
    data.getServiceID().push_back(service);
    uci::type::ServiceID_Type::destroy(service);

    auto& subsystem = uci::type::SubsystemID_Type::create(asb);
    fillId(subsystem, kSubsystemUuid, kSubsystemLabel);
    data.getSubsystemID().push_back(subsystem);
    uci::type::SubsystemID_Type::destroy(subsystem);

    data.enablePlatformStatus().enableAirborne() = true;

    writer.write(msg);
    uci::type::SystemStatusMT::destroy(msg);
}

void sendServiceStatus(uint64_t seq,
                       uci::type::ServiceStatusMT::Writer& writer,
                       uci::base::AbstractServiceBusConnection* asb) {
    auto& msg = uci::type::ServiceStatusMT::create(asb);
    fillHeader(msg);

    auto& data = msg.getMessageData();
    fillId(data.getServiceID(), kServiceUuid, kServiceLabel);
    data.getTimeUp().setValue(static_cast<int64_t>(seq + 1) * 1'000'000'000LL);
    data.getServiceState().setValue((seq % 20 == 19)
        ? uci::type::ServiceStateEnum::DEGRADED
        : uci::type::ServiceStateEnum::NORMAL);

    writer.write(msg);
    uci::type::ServiceStatusMT::destroy(msg);
}

void sendSubsystemStatus(uint64_t seq,
                         uci::type::SubsystemStatusMT::Writer& writer,
                         uci::base::AbstractServiceBusConnection* asb) {
    auto& msg = uci::type::SubsystemStatusMT::create(asb);
    fillHeader(msg);

    auto& data = msg.getMessageData();
    fillId(data.getSubsystemID(), kSubsystemUuid, kSubsystemLabel);
    data.getSubsystemState().setValue((seq % 15 == 14)
        ? uci::type::SubsystemStateEnum::DEGRADED
        : uci::type::SubsystemStateEnum::OPERATE);
    data.getAbout().getModel() = "NAV-DEMO";
    data.getAbout().enableSerialNumber() = "SIM-0001";
    data.getAbout().enableSoftwareVersion() = "2.5-demo";

    auto& operate = uci::type::SubsystemStateEnum::create(asb);
    operate.setValue(uci::type::SubsystemStateEnum::OPERATE);
    data.getCommandableSubsystemState().push_back(operate);
    uci::type::SubsystemStateEnum::destroy(operate);

    writer.write(msg);
    uci::type::SubsystemStatusMT::destroy(msg);
}

bool canSend(uint64_t count, uint64_t sent) {
    return count == 0 || sent < count;
}

void usage() {
    std::cerr << "usage: uci_traffic_gen [--rate N] [--position-rate N] "
              << "[--system-rate N] [--service-rate N] [--subsystem-rate N] "
              << "[--count N]\n";
}

} // namespace

int main(int argc, char** argv) {
    double positionRate = 10.0;
    double systemRate = 1.0;
    double serviceRate = 2.0;
    double subsystemRate = 0.5;
    uint64_t count = 0;

    for (int i = 1; i < argc; ++i) {
        if ((std::strcmp(argv[i], "--rate") == 0 ||
             std::strcmp(argv[i], "--position-rate") == 0) && i + 1 < argc) {
            positionRate = std::stod(argv[++i]);
        } else if (std::strcmp(argv[i], "--system-rate") == 0 && i + 1 < argc) {
            systemRate = std::stod(argv[++i]);
        } else if (std::strcmp(argv[i], "--service-rate") == 0 && i + 1 < argc) {
            serviceRate = std::stod(argv[++i]);
        } else if (std::strcmp(argv[i], "--subsystem-rate") == 0 && i + 1 < argc) {
            subsystemRate = std::stod(argv[++i]);
        } else if (std::strcmp(argv[i], "--count") == 0 && i + 1 < argc) {
            count = std::stoull(argv[++i]);
        } else {
            usage();
            return 1;
        }
    }

    if (positionRate <= 0.0 || systemRate <= 0.0 ||
        serviceRate <= 0.0 || subsystemRate <= 0.0) {
        std::cerr << "uci_traffic_gen: rates must be positive\n";
        return 1;
    }

    std::signal(SIGINT, sighandler);
    std::signal(SIGTERM, sighandler);

    auto* asb = uci_getAbstractServiceBusConnection("uci_traffic_gen", "DDS");
    if (!asb) {
        std::cerr << "uci_traffic_gen: failed to get ASB\n";
        return 1;
    }

    auto& positionWriter  = uci::type::PositionReportMT::createWriter("PositionReport", asb);
    auto& systemWriter    = uci::type::SystemStatusMT::createWriter("SystemStatus", asb);
    auto& serviceWriter   = uci::type::ServiceStatusMT::createWriter("ServiceStatus", asb);
    auto& subsystemWriter = uci::type::SubsystemStatusMT::createWriter("SubsystemStatus", asb);

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    const auto positionPeriod  = periodFor(positionRate);
    const auto systemPeriod    = periodFor(systemRate);
    const auto servicePeriod   = periodFor(serviceRate);
    const auto subsystemPeriod = periodFor(subsystemRate);

    auto now          = std::chrono::steady_clock::now();
    auto nextPosition  = now;
    auto nextSystem    = now;
    auto nextService   = now;
    auto nextSubsystem = now;

    uint64_t sent          = 0;
    uint64_t positionSent  = 0;
    uint64_t systemSent    = 0;
    uint64_t serviceSent   = 0;
    uint64_t subsystemSent = 0;

    std::cout << "uci_traffic_gen: PositionReport=" << positionRate << "/s"
              << "  SystemStatus=" << systemRate << "/s"
              << "  ServiceStatus=" << serviceRate << "/s"
              << "  SubsystemStatus=" << subsystemRate << "/s"
              << (count ? "  count=" + std::to_string(count) : "  count=unlimited")
              << "  mode=SIMULATION\n";

    while (g_running && canSend(count, sent)) {
        now = std::chrono::steady_clock::now();

        if (now >= nextPosition && canSend(count, sent)) {
            sendPositionReport(positionSent++, positionWriter, asb);
            ++sent;
            nextPosition += positionPeriod;
        }
        if (now >= nextSystem && canSend(count, sent)) {
            sendSystemStatus(systemSent++, systemWriter, asb);
            ++sent;
            nextSystem += systemPeriod;
        }
        if (now >= nextService && canSend(count, sent)) {
            sendServiceStatus(serviceSent++, serviceWriter, asb);
            ++sent;
            nextService += servicePeriod;
        }
        if (now >= nextSubsystem && canSend(count, sent)) {
            sendSubsystemStatus(subsystemSent++, subsystemWriter, asb);
            ++sent;
            nextSubsystem += subsystemPeriod;
        }

        auto wake = std::min({nextPosition, nextSystem, nextService, nextSubsystem});
        std::this_thread::sleep_until(wake);
    }

    std::cout << "uci_traffic_gen: sent " << sent << " messages"
              << "  PositionReport=" << positionSent
              << "  SystemStatus=" << systemSent
              << "  ServiceStatus=" << serviceSent
              << "  SubsystemStatus=" << subsystemSent << "\n";

    uci::type::PositionReportMT::destroyWriter(positionWriter);
    uci::type::SystemStatusMT::destroyWriter(systemWriter);
    uci::type::ServiceStatusMT::destroyWriter(serviceWriter);
    uci::type::SubsystemStatusMT::destroyWriter(subsystemWriter);
    asb->shutdown();
    uci_destroyAbstractServiceBusConnection(asb);
    return 0;
}
