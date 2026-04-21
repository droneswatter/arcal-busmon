// Rich ActionCommandMT publisher — fills 3+ levels of nesting so busmon shows
// structured JSON output.  Run alongside arcal-busmon to observe decoded payload.
//
//   CYCLONEDDS_URI=file://... ./pub_rich

#include "uci/type/ActionCommandMT.h"
#include "uci/type/ActionCommandType.h"
#include "uci/base/AbstractServiceBusConnection.h"
#include <chrono>
#include <iostream>
#include <thread>

int main() {
    auto* asb = uci_getAbstractServiceBusConnection("pub_rich", "DDS");
    if (!asb) { std::cerr << "pub_rich: failed to get ASB\n"; return 1; }

    auto& writer = uci::type::ActionCommandMT::createWriter("ActionCommand", asb);

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    uci::type::ActionCommandMT msg;

    // --- MessageHeader (depth 1) ---
    auto& hdr = msg.getMessageHeader();

    // hdr.SystemID.UUID  (depth 3: msg → MessageHeader → SystemID → UUID)
    hdr.getSystemID().getUUID().setValue("11111111-1111-1111-1111-111111111111");
    hdr.getSystemID().enableDescriptiveLabel().setValue("SENSOR-PLATFORM-ALPHA");

    // hdr.Timestamp  (depth 2)
    hdr.getTimestamp().setValue("2026-04-19T12:00:00Z");

    // hdr.SchemaVersion  (depth 2)
    hdr.getSchemaVersion().setValue("2.5.0");

    // hdr.Mode  (depth 2)
    hdr.getMode().setValue(uci::type::MessageModeEnum::EXERCISE);

    // optional ServiceID  (depth 3: msg → MessageHeader → ServiceID → UUID)
    hdr.enableServiceID().getUUID().setValue("22222222-2222-2222-2222-222222222222");
    hdr.getServiceID().enableDescriptiveLabel().setValue("FIRE-CONTROL-SERVICE");

    // optional MissionID  (depth 3)
    hdr.enableMissionID().getUUID().setValue("33333333-3333-3333-3333-333333333333");

    // --- MessageData.Command[0] — chooseCapability (depth 4+) ---
    uci::type::ActionCommandType cmdEntry;
    auto& cap = cmdEntry.chooseCapability();

    // CapabilityID.UUID  (depth 4: msg → MessageData → Command[0] → Capability → CapabilityID → UUID)
    cap.getCapabilityID().getUUID().setValue("44444444-4444-4444-4444-444444444444");
    cap.getCapabilityID().enableDescriptiveLabel().setValue("LONG-RANGE-TRACK");

    // Ranking  (depth 4: … → Capability → Ranking → Rank → Priority)
    cap.getRanking().getRank().getPriority().setValue(1);
    cap.getRanking().getRank().enablePrecedenceWithinPriority().setValue(2);
    cap.getRanking().enableInterruptOtherActivities() = true;

    // ActionID  (depth 4: … → Capability → ActionID → UUID)
    cap.getActionID().getUUID().setValue("55555555-5555-5555-5555-555555555555");
    cap.getActionID().enableVersion() = 3;

    msg.getMessageData().getCommand().push_back(cmdEntry);

    writer.write(msg);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    writer.close();
    uci::type::ActionCommandMT::destroyWriter(writer);
    asb->shutdown();
    uci_destroyAbstractServiceBusConnection(asb);

    std::cout << "pub_rich: sent rich ActionCommandMT\n";
    return 0;
}
