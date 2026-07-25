#include "automation_core/Protocol/Message.h"
#include "automation_core/Protocol/Validator.h"
#include "automation_core/Registry/ModuleRegistry.h"
#include "automation_core/Response/ResponseBuilder.h"
#include "automation_core/Router/MessageRouter.h"

#include <iostream>
#include <string>
#include <variant>

using namespace automation_core;

namespace {

int failures = 0;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "MessageRouter: " << message << '\n';
        ++failures;
    }
}

Message make_hello(
    std::string module_id = "scale-01",
    std::string session_id = "81A9C5D2",
    std::string message_id = "msg-1"
) {
    return Message{
        HelloMessage{
            std::move(message_id),
            "Scale",
            std::move(module_id),
            "1.0.0",
            "1",
            std::move(session_id)
        }
    };
}

Message make_heartbeat(
    std::uint32_t sequence = 1,
    std::uint64_t uptime_ms = 1000,
    std::string module_id = "scale-01",
    std::string session_id = "81A9C5D2"
) {
    return Message{
        HeartbeatMessage{
            "heartbeat-1",
            std::move(module_id),
            std::move(session_id),
            sequence,
            uptime_ms,
            ModuleState::Ready,
            0
        }
    };
}

const HelloAckMessage* get_ack(const RouteResult& result) {
    if (!result.response) {
        return nullptr;
    }

    return std::get_if<HelloAckMessage>(&result.response->payload);
}

const ErrorMessage* get_error(const RouteResult& result) {
    if (!result.response) {
        return nullptr;
    }

    return std::get_if<ErrorMessage>(&result.response->payload);
}

void test_routes_valid_hello() {
    Validator validator;
    ModuleRegistry registry;
    ResponseBuilder responses;
    MessageRouter router{validator, registry, responses};

    const RouteResult result =
        router.route("connection-1", make_hello());

    const auto* ack = get_ack(result);
    const auto module = registry.find("scale-01");

    expect(result.has_response(), "valid HELLO produced no response");
    expect(ack != nullptr, "valid HELLO did not produce HELLO_ACK");

    expect(
        ack && ack->message_id == "msg-1",
        "HELLO_ACK did not preserve message ID"
    );

    expect(
        ack && ack->connection_id == "connection-1",
        "HELLO_ACK used wrong connection ID"
    );

    expect(
        module && module->connection_id == "connection-1",
        "valid HELLO was not registered"
    );
}

void test_rejects_invalid_hello() {
    Validator validator;
    ModuleRegistry registry;
    ResponseBuilder responses;
    MessageRouter router{validator, registry, responses};

    Message message = make_hello();
    std::get<HelloMessage>(message.payload).session_id = "bad";

    const RouteResult result =
        router.route("connection-1", message);

    const auto* error = get_error(result);

    expect(error != nullptr, "invalid HELLO did not produce ERROR");

    expect(
        error && error->code == "SESSION",
        "validation error code was not preserved"
    );

    expect(
        error && error->message_id == "msg-1",
        "validation error lost message ID"
    );

    expect(
        registry.list().empty(),
        "invalid HELLO entered the registry"
    );
}

void test_rejects_missing_connection_id() {
    Validator validator;
    ModuleRegistry registry;
    ResponseBuilder responses;
    MessageRouter router{validator, registry, responses};

    const RouteResult result =
        router.route("", make_hello());

    const auto* error = get_error(result);

    expect(
        error && error->code == "CONNECTION",
        "missing connection ID was not rejected"
    );

    expect(
        error && error->message_id == "msg-1",
        "missing connection error lost message ID"
    );

    expect(
        registry.list().empty(),
        "message without connection ID entered registry"
    );
}

void test_reconnects_same_session() {
    Validator validator;
    ModuleRegistry registry;
    ResponseBuilder responses;
    MessageRouter router{validator, registry, responses};

    static_cast<void>(router.route("connection-1", make_hello()));
    static_cast<void>(router.route("connection-2", make_hello()));
    static_cast<void>(router.route("connection-3", make_hello()));

    const RouteResult result =
        router.route("connection-4", make_hello());

    const auto* ack = get_ack(result);
    const auto module = registry.find("scale-01");

    expect(
        ack != nullptr,
        "same-session reconnect did not produce HELLO_ACK"
    );

    expect(
        module && module->connection_id == "connection-4",
        "registry did not retain newest connection"
    );
}

void test_rejects_duplicate_identity() {
    Validator validator;
    ModuleRegistry registry;
    ResponseBuilder responses;
    MessageRouter router{validator, registry, responses};

    static_cast<void>(router.route(
        "connection-1",
        make_hello("scale-01", "81A9C5D2", "msg-1")
    ));

    const RouteResult result = router.route(
        "connection-2",
        make_hello("scale-01", "AAAAAAAA", "msg-2")
    );

    const auto* error = get_error(result);
    const auto module = registry.find("scale-01");

    expect(
        error && error->code == "DUPLICATE_IDENTITY",
        "duplicate identity was not rejected correctly"
    );

    expect(
        module && module->connection_id == "connection-1",
        "duplicate identity replaced authoritative registration"
    );
}

void test_duplicate_after_reconnect() {
    Validator validator;
    ModuleRegistry registry;
    ResponseBuilder responses;
    MessageRouter router{validator, registry, responses};

    static_cast<void>(router.route("connection-1", make_hello()));
    static_cast<void>(router.route("connection-2", make_hello()));

    const RouteResult result =
        router.route(
            "connection-3",
            make_hello(
                "scale-01",
                "AAAAAAAA",
                "msg-2"
            )
        );

    const auto* error = get_error(result);
    const auto module = registry.find("scale-01");

    expect(
        error && error->code == "DUPLICATE_IDENTITY",
        "duplicate after reconnect was accepted"
    );

    expect(
        module && module->connection_id == "connection-2",
        "duplicate after reconnect replaced active binding"
    );
}

void test_rejects_quarantined_identity() {
    Validator validator;
    ModuleRegistry registry;
    ResponseBuilder responses;
    MessageRouter router{validator, registry, responses};

    static_cast<void>(router.route("connection-1", make_hello()));
    registry.quarantine("scale-01");

    const RouteResult result =
        router.route("connection-2", make_hello());

    const auto* error = get_error(result);
    const auto module = registry.find("scale-01");

    expect(
        error && error->code == "QUARANTINED",
        "quarantined identity was not rejected"
    );

    expect(
        module && module->connection_id == "connection-1",
        "quarantined reconnect changed registry"
    );
}

void test_rejects_unsupported_message() {
    Validator validator;
    ModuleRegistry registry;
    ResponseBuilder responses;
    MessageRouter router{validator, registry, responses};

    const Message message{
        ErrorMessage{
            "msg-1",
            "TEST",
            "Not an inbound HELLO"
        }
    };

    const RouteResult result =
        router.route("connection-1", message);

    const auto* error = get_error(result);

    expect(
        error && error->code == "UNSUPPORTED",
        "unsupported message type was not rejected"
    );

    expect(
        error && error->message_id.empty(),
        "unsupported message fabricated a message ID"
    );

    expect(
        registry.list().empty(),
        "unsupported message modified registry"
    );
}

void test_routes_valid_heartbeat_without_ack() {
    Validator validator;
    ModuleRegistry registry;
    ResponseBuilder responses;
    MessageRouter router{validator, registry, responses};
    static_cast<void>(router.route("connection-1", make_hello()));

    const RouteResult result =
        router.route("connection-1", make_heartbeat());
    const auto module = registry.find("scale-01");

    expect(
        !result.has_response(),
        "valid heartbeat produced an acknowledgment"
    );
    expect(
        module &&
        module->has_heartbeat &&
        module->last_heartbeat_sequence == 1,
        "valid heartbeat did not update registry"
    );
}

void test_rejects_unknown_heartbeat_module() {
    Validator validator;
    ModuleRegistry registry;
    ResponseBuilder responses;
    MessageRouter router{validator, registry, responses};

    const RouteResult result = router.route(
        "connection-1",
        make_heartbeat(1, 1000, "missing")
    );
    const auto* error = get_error(result);

    expect(
        error && error->code == "UNKNOWN_MODULE",
        "unknown heartbeat module was not rejected"
    );
    expect(
        error && error->message_id == "heartbeat-1",
        "heartbeat error lost correlation ID"
    );
}

void test_rejects_heartbeat_identity_mismatch() {
    Validator validator;
    ModuleRegistry registry;
    ResponseBuilder responses;
    MessageRouter router{validator, registry, responses};
    static_cast<void>(router.route("connection-1", make_hello()));

    const auto wrong_connection =
        router.route("connection-2", make_heartbeat());
    const auto wrong_session = router.route(
        "connection-1",
        make_heartbeat(1, 1000, "scale-01", "AAAAAAAA")
    );

    expect(
        get_error(wrong_connection) &&
        get_error(wrong_connection)->code == "CONNECTION_MISMATCH",
        "wrong heartbeat connection was not rejected"
    );
    expect(
        get_error(wrong_session) &&
        get_error(wrong_session)->code == "SESSION_MISMATCH",
        "wrong heartbeat session was not rejected"
    );
}

void test_rejects_stale_and_regressed_heartbeat() {
    Validator validator;
    ModuleRegistry registry;
    ResponseBuilder responses;
    MessageRouter router{validator, registry, responses};
    static_cast<void>(router.route("connection-1", make_hello()));
    static_cast<void>(
        router.route("connection-1", make_heartbeat(10, 1000))
    );

    const auto stale =
        router.route("connection-1", make_heartbeat(9, 1100));
    const auto regressed =
        router.route("connection-1", make_heartbeat(11, 999));

    expect(
        get_error(stale) &&
        get_error(stale)->code == "HEARTBEAT_ORDER",
        "stale heartbeat was not rejected"
    );
    expect(
        get_error(regressed) &&
        get_error(regressed)->code == "UPTIME_REGRESSION",
        "uptime regression was not rejected"
    );
}

void test_routes_capabilities() {
    Validator validator;
    ModuleRegistry registry;
    ResponseBuilder responses;
    MessageRouter router{validator, registry, responses};
    static_cast<void>(router.route("connection-1", make_hello()));
    Message document{CapabilitiesMessage{"msg-50","scale-01","81A9C5D2",1,{Capability{"weight",CapabilityType::Measurement,CapabilityDataType::Float,CapabilityAccess::Read}}}};
    const auto result=router.route("connection-1",document);
    const auto* ack=result.response?std::get_if<CapabilitiesAckMessage>(&result.response->payload):nullptr;
    expect(ack&&ack->revision==1,"valid capabilities did not produce ACK");
    const auto module=registry.find("scale-01");
    expect(module&&module->capabilities_available,"routed capabilities not stored");
}

void test_rejects_unknown_capability_module() {
    Validator validator;
    ModuleRegistry registry;
    ResponseBuilder responses;
    MessageRouter router{validator, registry, responses};
    Message document{CapabilitiesMessage{"msg-50","missing","81A9C5D2",1,{Capability{"weight",CapabilityType::Measurement,CapabilityDataType::Float,CapabilityAccess::Read}}}};
    const auto result=router.route("connection-1",document);
    expect(get_error(result)&&get_error(result)->code=="UNKNOWN_MODULE","unknown capability module accepted");
}

void test_invalid_capability_replacement_is_atomic() {
    Validator validator;ModuleRegistry registry;ResponseBuilder responses;MessageRouter router{validator,registry,responses};
    static_cast<void>(router.route("connection-1",make_hello()));
    Message valid{CapabilitiesMessage{"msg-50","scale-01","81A9C5D2",1,{Capability{"weight",CapabilityType::Measurement,CapabilityDataType::Float,CapabilityAccess::Read}}}};
    static_cast<void>(router.route("connection-1",valid));
    Message invalid{CapabilitiesMessage{"msg-51","scale-01","81A9C5D2",2,{Capability{"weight",CapabilityType::Measurement,std::nullopt,CapabilityAccess::Read}}}};
    const auto result=router.route("connection-1",invalid);
    const auto module=registry.find("scale-01");
    expect(get_error(result)&&module&&module->capability_revision==1&&module->capabilities[0].data_type==CapabilityDataType::Float,"invalid replacement changed accepted capabilities");
}

void test_routes_measurements_without_ack() {
    Validator validator;ModuleRegistry registry;ResponseBuilder responses;MessageRouter router{validator,registry,responses};
    static_cast<void>(router.route("connection-1",make_hello()));
    Message caps{CapabilitiesMessage{"msg-50","scale-01","81A9C5D2",1,{Capability{"weight",CapabilityType::Measurement,CapabilityDataType::Float,CapabilityAccess::Read,std::string{"lb"}}}}};
    static_cast<void>(router.route("connection-1",caps));
    MeasurementMessage measurement{"70","scale-01","81A9C5D2","weight",1,"42.5",MeasurementQuality::Good};measurement.unit="lb";
    const auto accepted=router.route("connection-1",Message{measurement});
    expect(!accepted.response&&registry.find("scale-01")->measurements.count("weight")==1,"valid measurement was not silently accepted");
    measurement.sequence=2;measurement.unit="kg";
    const auto rejected=router.route("connection-1",Message{measurement});
    expect(get_error(rejected)&&get_error(rejected)->message_id=="70"&&get_error(rejected)->code=="UNIT_MISMATCH","measurement rejection was not correlated");
}

} // namespace

int run_message_router_tests() {
    failures = 0;

    test_routes_valid_hello();
    test_rejects_invalid_hello();
    test_rejects_missing_connection_id();
    test_reconnects_same_session();
    test_rejects_duplicate_identity();
    test_duplicate_after_reconnect();
    test_rejects_quarantined_identity();
    test_rejects_unsupported_message();
    test_routes_valid_heartbeat_without_ack();
    test_rejects_unknown_heartbeat_module();
    test_rejects_heartbeat_identity_mismatch();
    test_rejects_stale_and_regressed_heartbeat();
    test_routes_capabilities();
    test_rejects_unknown_capability_module();
    test_invalid_capability_replacement_is_atomic();
    test_routes_measurements_without_ack();

    return failures;
}
