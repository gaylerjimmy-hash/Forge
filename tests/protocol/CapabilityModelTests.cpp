#include "automation_core/Protocol/Message.h"

#include <iostream>
#include <variant>

using namespace automation_core;

int run_capability_model_tests() {
    const Message message{
        CapabilitiesMessage{
            "msg-50",
            "scale-01",
            "81A9C5D2",
            1,
            {
                Capability{
                    "weight",
                    CapabilityType::Measurement,
                    CapabilityDataType::Float,
                    CapabilityAccess::Read,
                    std::string{"lb"},
                    0.0,
                    100.0,
                    true,
                    true,
                    std::string{"Reservoir weight"}
                }
            }
        }
    };

    const auto* document =
        std::get_if<CapabilitiesMessage>(&message.payload);

    if (document == nullptr || document->items.size() != 1) {
        std::cerr << "CapabilityModel: document was not retained\n";
        return 1;
    }

    const Capability& capability = document->items.front();
    return capability.name == "weight" &&
        capability.type == CapabilityType::Measurement &&
        capability.data_type == CapabilityDataType::Float &&
        capability.access == CapabilityAccess::Read &&
        capability.supports_quality &&
        capability.supports_calibration
        ? 0
        : 1;
}
