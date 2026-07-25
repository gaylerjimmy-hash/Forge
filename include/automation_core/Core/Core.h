#pragma once

#include "automation_core/Connection/ConnectionManager.h"
#include "automation_core/Frame/FrameAssembler.h"
#include "automation_core/Protocol/MessageSerializer.h"
#include "automation_core/Protocol/Parser.h"
#include "automation_core/Protocol/Validator.h"
#include "automation_core/Registry/ModuleRegistry.h"
#include "automation_core/Response/ResponseBuilder.h"
#include "automation_core/Router/MessageRouter.h"
#include "automation_core/Transport/ITransport.h"

#include <chrono>
#include <functional>
#include <iosfwd>
#include <string>
#include <unordered_map>

namespace automation_core {

class Core {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using NowFunction = std::function<TimePoint()>;

    explicit Core(ITransport& transport);
    Core(ITransport& transport, std::ostream& trace_output);
    Core(ITransport& transport, NowFunction now);
    Core(
        ITransport& transport,
        NowFunction now,
        std::chrono::milliseconds heartbeat_timeout
    );
    Core(
        ITransport& transport,
        std::ostream& trace_output,
        NowFunction now,
        std::chrono::milliseconds heartbeat_timeout =
            std::chrono::milliseconds{3000}
    );

    bool poll_once();

private:
    Core(
        ITransport& transport,
        std::ostream* trace_output,
        NowFunction now,
        std::chrono::milliseconds heartbeat_timeout
    );

    void trace(
        const std::string& event,
        const std::string& detail
    ) const;

    ITransport& transport_;
    std::ostream* trace_output_;
    NowFunction now_;
    std::chrono::milliseconds heartbeat_timeout_;
    ConnectionManager connections_;
    FrameAssembler frames_;
    Parser parser_;
    MessageSerializer serializer_;
    Validator validator_;
    ModuleRegistry registry_;
    ResponseBuilder responses_;
    MessageRouter router_;
    std::unordered_map<std::string, std::string> transport_connections_;
};

} // namespace automation_core
