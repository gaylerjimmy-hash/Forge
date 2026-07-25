#pragma once

#include "automation_core/Protocol/Message.h"
#include "automation_core/Protocol/Validator.h"
#include "automation_core/Registry/ModuleRegistry.h"
#include "automation_core/Response/ResponseBuilder.h"
#include "automation_core/Router/RouteResult.h"

#include <string>

namespace automation_core {

class MessageRouter {
public:
    MessageRouter(
        Validator& validator,
        ModuleRegistry& registry,
        ResponseBuilder& responses
    );

    [[nodiscard]] RouteResult route(
        const std::string& connection_id,
        const Message& message,
        ModuleRegistry::TimePoint now =
            ModuleRegistry::Clock::now()
    );

private:
    Validator& validator_;
    ModuleRegistry& registry_;
    ResponseBuilder& responses_;
};

} // namespace automation_core
