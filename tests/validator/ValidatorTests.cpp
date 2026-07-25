#include "automation_core/Protocol/Validator.h"

#include <iostream>
#include <string>
#include <utility>

using namespace automation_core;

namespace {

int failures = 0;

void expect(const bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "Validator: " << message << '\n';
        ++failures;
    }
}

Message make_hello() {
    return Message{
        HelloMessage{
            "msg-1",
            "Scale",
            "scale-01",
            "1.0.0",
            "1",
            "81A9C5D2"
        }
    };
}

HelloMessage& hello(Message& message) {
    return std::get<HelloMessage>(message.payload);
}

void expect_rejected(
    Message message,
    const Validator& validator,
    const std::string& expected_code,
    const std::string& description
) {
    const ValidationResult result = validator.validate(message);

    expect(
        result.status == ValidationStatus::Rejected,
        description + " did not return Rejected"
    );

    expect(
        result.error_code == expected_code,
        description + " returned the wrong error code"
    );

    expect(
        !result.detail.empty(),
        description + " returned an empty error detail"
    );
}

void test_accepts_valid_hello() {
    const Validator validator{"1"};
    const ValidationResult result = validator.validate(make_hello());

    expect(result.valid(), "valid HELLO was rejected");
    expect(result.error_code.empty(), "valid HELLO returned an error code");
    expect(result.detail.empty(), "valid HELLO returned an error detail");
}

void test_rejects_unsupported_message() {
    const Validator validator{"1"};
    const Message message{
        ErrorMessage{
            "msg-1",
            "TEST",
            "Not an inbound HELLO"
        }
    };

    expect_rejected(
        message,
        validator,
        "UNSUPPORTED",
        "unsupported message"
    );
}

void test_validates_heartbeat() {
    const Validator validator{"1"};
    const Message valid{
        HeartbeatMessage{
            "msg-42",
            "scale-01",
            "81A9C5D2",
            42,
            1000,
            ModuleState::Ready,
            0
        }
    };

    expect(
        validator.validate(valid).valid(),
        "valid HEARTBEAT was rejected"
    );

    Message invalid_session = valid;
    std::get<HeartbeatMessage>(
        invalid_session.payload
    ).session_id = "BAD";
    expect_rejected(
        invalid_session,
        validator,
        "SESSION",
        "heartbeat with invalid session"
    );

    Message invalid_module = valid;
    std::get<HeartbeatMessage>(
        invalid_module.payload
    ).module_id = "1scale";
    expect_rejected(
        invalid_module,
        validator,
        "MODULE_ID",
        "heartbeat with invalid module ID"
    );

    Message invalid_message = valid;
    std::get<HeartbeatMessage>(
        invalid_message.payload
    ).message_id = "msg.42";
    expect_rejected(
        invalid_message,
        validator,
        "MSG",
        "heartbeat with invalid message ID"
    );
}

void test_rejects_protocol_mismatch() {
    const Validator validator{"1"};
    Message message = make_hello();
    hello(message).protocol_version = "2";

    const ValidationResult result = validator.validate(message);

    expect(
        result.status == ValidationStatus::VersionMismatch,
        "protocol mismatch did not return VersionMismatch"
    );

    expect(
        result.error_code == "PROTO_VERSION",
        "protocol mismatch returned the wrong error code"
    );
}

void test_validates_session_id() {
    const Validator validator{"1"};

    for (const std::string invalid : {
        "",
        "1234567",
        "123456789",
        "1234567G",
        "12-45678"
    }) {
        Message message = make_hello();
        hello(message).session_id = invalid;

        expect_rejected(
            std::move(message),
            validator,
            "SESSION",
            "invalid session '" + invalid + "'"
        );
    }

    Message lowercase = make_hello();
    hello(lowercase).session_id = "81a9c5d2";

    expect(
        validator.validate(lowercase).valid(),
        "lowercase hexadecimal session was rejected"
    );
}

void test_validates_module_type() {
    const Validator validator{"1"};

    for (const std::string invalid : {
        "",
        "1Scale",
        "-Scale",
        "Scale Type",
        "Scale!"
    }) {
        Message message = make_hello();
        hello(message).module_type = invalid;

        expect_rejected(
            std::move(message),
            validator,
            "MODULE_TYPE",
            "invalid module type '" + invalid + "'"
        );
    }

    for (const std::string valid : {
        "Scale",
        "Scale_2",
        "Scale-Type"
    }) {
        Message message = make_hello();
        hello(message).module_type = valid;

        expect(
            validator.validate(message).valid(),
            "valid module type '" + valid + "' was rejected"
        );
    }
}

void test_validates_module_id() {
    const Validator validator{"1"};

    for (const std::string invalid : {
        "",
        "1scale",
        "-scale",
        "scale 01",
        "scale!"
    }) {
        Message message = make_hello();
        hello(message).module_id = invalid;

        expect_rejected(
            std::move(message),
            validator,
            "MODULE_ID",
            "invalid module id '" + invalid + "'"
        );
    }

    for (const std::string valid : {
        "scale",
        "scale_01",
        "scale-01"
    }) {
        Message message = make_hello();
        hello(message).module_id = valid;

        expect(
            validator.validate(message).valid(),
            "valid module id '" + valid + "' was rejected"
        );
    }
}

void test_validates_firmware_version() {
    const Validator validator{"1"};

    for (const std::string invalid : {
        "",
        "1",
        "1.0",
        "1.0.",
        ".1.0",
        "1..0",
        "1.0.0.0",
        "v1.0.0",
        "1.a.0"
    }) {
        Message message = make_hello();
        hello(message).firmware_version = invalid;

        expect_rejected(
            std::move(message),
            validator,
            "FW",
            "invalid firmware version '" + invalid + "'"
        );
    }

    for (const std::string valid : {
        "0.0.0",
        "1.0.0",
        "10.20.300"
    }) {
        Message message = make_hello();
        hello(message).firmware_version = valid;

        expect(
            validator.validate(message).valid(),
            "valid firmware version '" + valid + "' was rejected"
        );
    }
}

void test_validates_message_id() {
    const Validator validator{"1"};

    for (const std::string invalid : {
        "",
        "msg 1",
        "msg.1",
        "msg!",
        "msg/1"
    }) {
        Message message = make_hello();
        hello(message).message_id = invalid;

        expect_rejected(
            std::move(message),
            validator,
            "MSG",
            "invalid message id '" + invalid + "'"
        );
    }

    for (const std::string valid : {
        "1",
        "msg",
        "msg_1",
        "msg-1"
    }) {
        Message message = make_hello();
        hello(message).message_id = valid;

        expect(
            validator.validate(message).valid(),
            "valid message id '" + valid + "' was rejected"
        );
    }
}

void test_validation_order_is_deterministic() {
    const Validator validator{"1"};
    Message message = make_hello();

    hello(message).protocol_version = "2";
    hello(message).session_id = "bad";
    hello(message).module_type = "";
    hello(message).module_id = "";
    hello(message).firmware_version = "";
    hello(message).message_id = "";

    const ValidationResult result = validator.validate(message);

    expect(
        result.status == ValidationStatus::VersionMismatch,
        "protocol mismatch did not take precedence"
    );

    expect(
        result.error_code == "PROTO_VERSION",
        "validation order returned the wrong first error"
    );
}

void test_validates_capabilities() {
    const Validator validator{"1"};
    const Capability valid{"weight",CapabilityType::Measurement,CapabilityDataType::Float,CapabilityAccess::Read,std::string{"lb"},0.0,100.0,true,true};
    Message message{CapabilitiesMessage{"msg-50","scale-01","81A9C5D2",1,{valid}}};
    expect(validator.validate(message).valid(),"valid capabilities rejected");

    auto duplicate=std::get<CapabilitiesMessage>(message.payload);
    duplicate.items.push_back(valid);
    expect_rejected(Message{duplicate},validator,"DUPLICATE_CAPABILITY","duplicate capability");

    auto missing_type=std::get<CapabilitiesMessage>(message.payload);
    missing_type.items[0].data_type.reset();
    expect_rejected(Message{missing_type},validator,"CAPABILITY_DATA_TYPE","missing data type");

    auto bad_range=std::get<CapabilitiesMessage>(message.payload);
    bad_range.items[0].minimum=101.0;
    expect_rejected(Message{bad_range},validator,"CAPABILITY_RANGE","invalid range");

    Capability command{"tare",CapabilityType::Command,std::nullopt,CapabilityAccess::Command};
    message=Message{CapabilitiesMessage{"msg-51","scale-01","81A9C5D2",2,{command}}};
    expect(validator.validate(message).valid(),"valid command capability rejected");
}

void test_validates_measurement() {
    Validator validator;
    MeasurementMessage value{"70","scale-01","81A9C5D2","weight",1,"42.5",MeasurementQuality::Good};
    expect(validator.validate(Message{value}).valid(),"valid measurement rejected");
    value.uncertainty=-0.1;
    expect_rejected(Message{value},validator,"UNCERTAINTY","negative uncertainty");
    value.uncertainty.reset();
    value.quality=MeasurementQuality::Stale;
    expect_rejected(Message{value},validator,"QUALITY","wire stale quality");
}

} // namespace

int run_validator_tests() {
    failures = 0;

    test_accepts_valid_hello();
    test_validates_heartbeat();
    test_rejects_unsupported_message();
    test_rejects_protocol_mismatch();
    test_validates_session_id();
    test_validates_module_type();
    test_validates_module_id();
    test_validates_firmware_version();
    test_validates_message_id();
    test_validation_order_is_deterministic();
    test_validates_capabilities();
    test_validates_measurement();

    return failures;
}
