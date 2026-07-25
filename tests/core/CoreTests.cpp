#include "automation_core/Core/Core.h"
#include "automation_core/Transport/ITransport.h"

#include <deque>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace automation_core;

namespace {

struct SentPacket {
    std::string connection_id;
    std::string payload;
};

class FakeTransport final : public ITransport {
public:
    std::optional<TransportPacket> receive() override {
        if (incoming.empty()) {
            return std::nullopt;
        }

        TransportPacket packet = std::move(incoming.front());
        incoming.pop_front();
        return packet;
    }

    bool send(
        const std::string& connection_id,
        const std::string& payload
    ) override {
        sent.push_back({connection_id, payload});
        return send_succeeds;
    }

    std::deque<TransportPacket> incoming;
    std::vector<SentPacket> sent;
    bool send_succeeds{true};
};

int failures = 0;

void expect(const bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "Core: " << message << '\n';
        ++failures;
    }
}

std::string hello(
    const std::string& message_id = "msg-1",
    const std::string& module_id = "scale-01",
    const std::string& session_id = "81A9C5D2",
    const std::string& protocol_version = "1"
) {
    return
        "HELLO\n"
        "MSG=" + message_id + "\n"
        "TYPE=Scale\n"
        "ID=" + module_id + "\n"
        "FW=1.0.0\n"
        "PROTO=" + protocol_version + "\n"
        "SESSION=" + session_id + "\n"
        "END\n";
}

void test_no_packet_is_a_no_op() {
    FakeTransport transport;
    Core core{transport};

    core.poll_once();

    expect(transport.sent.empty(), "empty poll produced a response");
}

void test_valid_hello_produces_ack() {
    FakeTransport transport;
    Core core{transport};
    transport.incoming.push_back({"serial:device-1", hello()});

    core.poll_once();

    expect(transport.sent.size() == 1, "valid HELLO produced no response");

    if (transport.sent.size() == 1) {
        expect(
            transport.sent.front().connection_id == "serial:device-1",
            "response used the wrong transport connection"
        );
        expect(
            transport.sent.front().payload ==
                "HELLO_ACK\n"
                "MSG=msg-1\n"
                "CONNECTION=conn-1\n"
                "STATUS=accepted\n"
                "END\n",
            "valid HELLO produced the wrong response"
        );
    }
}

void test_trace_reports_discovery_lifecycle() {
    FakeTransport transport;
    std::ostringstream trace;
    Core core{transport, trace};
    transport.incoming.push_back({"serial:device-1", hello()});

    core.poll_once();

    const std::string output = trace.str();

    for (const std::string event : {
        "event=connection_opened",
        "event=frame_accepted",
        "event=parse_accepted",
        "event=route_completed",
        "event=response_sent"
    }) {
        expect(
            output.find(event) != std::string::npos,
            "trace is missing " + event
        );
    }
}

void test_transport_connection_is_reused() {
    FakeTransport transport;
    Core core{transport};
    transport.incoming.push_back({"serial:device-1", hello("msg-1")});
    transport.incoming.push_back({"serial:device-1", hello("msg-2")});

    core.poll_once();
    core.poll_once();

    expect(transport.sent.size() == 2, "reconnect responses are missing");

    if (transport.sent.size() == 2) {
        expect(
            transport.sent[1].payload.find("CONNECTION=conn-1\n") !=
                std::string::npos,
            "stable transport endpoint received a new Core connection ID"
        );
    }
}

void test_frame_error_produces_error_response() {
    FakeTransport transport;
    Core core{transport};
    transport.incoming.push_back({
        "serial:device-1",
        "HELLO\nMSG=msg-1\n"
    });

    core.poll_once();

    expect(transport.sent.size() == 1, "frame error produced no response");

    if (transport.sent.size() == 1) {
        expect(
            transport.sent.front().payload.find(
                "CODE=FRAME_TERMINATOR\n"
            ) != std::string::npos,
            "missing terminator produced the wrong error"
        );
    }
}

void test_frame_timeout_produces_error_response() {
    FakeTransport transport;
    Core core{transport};
    transport.incoming.push_back({
        "serial:device-1",
        hello(),
        std::chrono::milliseconds{1000}
    });

    core.poll_once();

    expect(transport.sent.size() == 1, "frame timeout produced no response");

    if (transport.sent.size() == 1) {
        expect(
            transport.sent.front().payload.find(
                "CODE=FRAME_TIMEOUT\n"
            ) != std::string::npos,
            "frame timeout produced the wrong error"
        );
    }
}

void test_parse_error_produces_error_response() {
    FakeTransport transport;
    Core core{transport};
    transport.incoming.push_back({
        "serial:device-1",
        "HEARTBEAT\nMSG=msg-1\nEND\n"
    });

    core.poll_once();

    expect(transport.sent.size() == 1, "parse error produced no response");

    if (transport.sent.size() == 1) {
        expect(
            transport.sent.front().payload.find(
                "CODE=UNKNOWN_MESSAGE_TYPE\n"
            ) != std::string::npos,
            "unknown message type produced the wrong error"
        );
    }
}

void expect_core_error(
    const std::string& payload,
    const std::string& expected_code,
    const std::string& description
) {
    FakeTransport transport;
    Core core{transport};
    transport.incoming.push_back({"serial:device-1", payload});

    core.poll_once();

    expect(
        transport.sent.size() == 1,
        description + " produced no response"
    );

    if (transport.sent.size() == 1) {
        expect(
            transport.sent.front().payload.find(
                "CODE=" + expected_code + "\n"
            ) != std::string::npos,
            description + " produced the wrong error"
        );
    }
}

void test_remaining_frame_failures() {
    expect_core_error("", "FRAME_EMPTY", "empty frame");
    expect_core_error(
        std::string(1025, 'X'),
        "FRAME_TOO_LARGE",
        "oversized frame"
    );
}

void test_remaining_parse_failures() {
    expect_core_error(
        "HELLO\n"
        "MSG\n"
        "END\n",
        "MALFORMED_FIELD",
        "malformed field"
    );

    expect_core_error(
        hello().insert(hello().find("END\n"), "COLOR=blue\n"),
        "UNKNOWN_FIELD",
        "unknown field"
    );

    expect_core_error(
        "HELLO\n"
        "MSG=msg-1\n"
        "MSG=msg-2\n"
        "TYPE=Scale\n"
        "ID=scale-01\n"
        "FW=1.0.0\n"
        "PROTO=1\n"
        "SESSION=81A9C5D2\n"
        "END\n",
        "DUPLICATE_FIELD",
        "duplicate field"
    );

    expect_core_error(
        "HELLO\n"
        "MSG=msg-1\n"
        "TYPE=Scale\n"
        "ID=scale-01\n"
        "FW=1.0.0\n"
        "PROTO=1\n"
        "END\n",
        "MISSING_FIELD",
        "missing field"
    );

    expect_core_error(
        "HELLO\n"
        "MSG=\n"
        "TYPE=Scale\n"
        "ID=scale-01\n"
        "FW=1.0.0\n"
        "PROTO=1\n"
        "SESSION=81A9C5D2\n"
        "END\n",
        "MALFORMED_FIELD",
        "empty value"
    );

    expect_core_error(
        hello() + "GARBAGE\nEND\n",
        "TRAILING_DATA",
        "trailing data"
    );
}

void test_remaining_validation_failures() {
    expect_core_error(
        "HELLO\n"
        "MSG=msg-1\n"
        "TYPE=Scale\n"
        "ID=scale-01\n"
        "FW=1.0.0\n"
        "PROTO=1\n"
        "SESSION=BAD\n"
        "END\n",
        "SESSION",
        "invalid session"
    );

    expect_core_error(
        "HELLO\n"
        "MSG=msg-1\n"
        "TYPE=1Scale\n"
        "ID=scale-01\n"
        "FW=1.0.0\n"
        "PROTO=1\n"
        "SESSION=81A9C5D2\n"
        "END\n",
        "MODULE_TYPE",
        "invalid module type"
    );

    expect_core_error(
        "HELLO\n"
        "MSG=msg-1\n"
        "TYPE=Scale\n"
        "ID=1scale\n"
        "FW=1.0.0\n"
        "PROTO=1\n"
        "SESSION=81A9C5D2\n"
        "END\n",
        "MODULE_ID",
        "invalid module ID"
    );

    expect_core_error(
        "HELLO\n"
        "MSG=msg-1\n"
        "TYPE=Scale\n"
        "ID=scale-01\n"
        "FW=v1\n"
        "PROTO=1\n"
        "SESSION=81A9C5D2\n"
        "END\n",
        "FW",
        "invalid firmware version"
    );

    expect_core_error(
        "HELLO\n"
        "MSG=msg.1\n"
        "TYPE=Scale\n"
        "ID=scale-01\n"
        "FW=1.0.0\n"
        "PROTO=1\n"
        "SESSION=81A9C5D2\n"
        "END\n",
        "MSG",
        "invalid message ID"
    );
}

void test_send_failure_is_traced() {
    FakeTransport transport;
    std::ostringstream trace;
    Core core{transport, trace};
    transport.send_succeeds = false;
    transport.incoming.push_back({"serial:device-1", hello()});

    core.poll_once();

    expect(
        transport.sent.size() == 1,
        "failed send was not attempted"
    );
    expect(
        trace.str().find("event=response_send_failed") !=
            std::string::npos,
        "failed send was not traced"
    );
}

void test_protocol_mismatch_produces_error_response() {
    FakeTransport transport;
    Core core{transport};
    transport.incoming.push_back({
        "serial:device-1",
        hello("msg-1", "scale-01", "81A9C5D2", "2")
    });

    core.poll_once();

    expect(
        transport.sent.size() == 1,
        "protocol mismatch produced no response"
    );

    if (transport.sent.size() == 1) {
        expect(
            transport.sent.front().payload.find(
                "MSG=msg-1\nCODE=PROTO_VERSION\n"
            ) != std::string::npos,
            "protocol mismatch produced the wrong error"
        );
    }
}

void test_duplicate_identity_is_rejected() {
    FakeTransport transport;
    Core core{transport};
    transport.incoming.push_back({
        "serial:device-1",
        hello("msg-1", "scale-01", "81A9C5D2")
    });
    transport.incoming.push_back({
        "serial:device-2",
        hello("msg-2", "scale-01", "AAAAAAAA")
    });

    core.poll_once();
    core.poll_once();

    expect(
        transport.sent.size() == 2,
        "duplicate identity responses are missing"
    );

    if (transport.sent.size() == 2) {
        expect(
            transport.sent[1].connection_id == "serial:device-2",
            "duplicate identity response used the wrong transport connection"
        );
        expect(
            transport.sent[1].payload.find(
                "CODE=DUPLICATE_IDENTITY\n"
            ) != std::string::npos,
            "duplicate identity was not rejected"
        );
    }
}

void test_same_session_reconnects_on_new_transport() {
    FakeTransport transport;
    Core core{transport};
    transport.incoming.push_back({
        "serial:device-1",
        hello("msg-1", "scale-01", "81A9C5D2")
    });
    transport.incoming.push_back({
        "serial:device-2",
        hello("msg-2", "scale-01", "81A9C5D2")
    });

    core.poll_once();
    core.poll_once();

    expect(
        transport.sent.size() == 2,
        "same-session reconnect responses are missing"
    );

    if (transport.sent.size() == 2) {
        expect(
            transport.sent[1].payload.find(
                "HELLO_ACK\nMSG=msg-2\nCONNECTION=conn-2\n"
            ) != std::string::npos,
            "same-session reconnect did not move to the new connection"
        );
    }
}

void test_duplicate_does_not_replace_authoritative_binding() {
    FakeTransport transport;
    Core core{transport};
    transport.incoming.push_back({
        "serial:device-1",
        hello("msg-1", "scale-01", "81A9C5D2")
    });
    transport.incoming.push_back({
        "serial:device-2",
        hello("msg-2", "scale-01", "AAAAAAAA")
    });
    transport.incoming.push_back({
        "serial:device-1",
        hello("msg-3", "scale-01", "81A9C5D2")
    });

    core.poll_once();
    core.poll_once();
    core.poll_once();

    expect(
        transport.sent.size() == 3,
        "authoritative binding verification responses are missing"
    );

    if (transport.sent.size() == 3) {
        expect(
            transport.sent[2].payload.find(
                "HELLO_ACK\nMSG=msg-3\nCONNECTION=conn-1\n"
            ) != std::string::npos,
            "duplicate identity replaced the authoritative binding"
        );
    }
}

void test_quarantined_transport_cannot_reopen() {
    FakeTransport transport;
    Core core{transport};
    transport.incoming.push_back({
        "serial:device-1",
        hello("msg-1", "scale-01", "81A9C5D2")
    });
    transport.incoming.push_back({
        "serial:device-2",
        hello("msg-2", "scale-01", "AAAAAAAA")
    });
    transport.incoming.push_back({
        "serial:device-2",
        hello("msg-3", "flow-01", "BBBBBBBB")
    });

    core.poll_once();
    core.poll_once();
    core.poll_once();

    expect(
        transport.sent.size() == 3,
        "quarantine verification responses are missing"
    );

    if (transport.sent.size() == 3) {
        expect(
            transport.sent[2].payload.find(
                "CODE=CONNECTION_QUARANTINED\n"
            ) != std::string::npos,
            "quarantined transport obtained a new Core connection"
        );
        expect(
            transport.sent[2].payload.find("HELLO_ACK\n") ==
                std::string::npos,
            "quarantined transport registered another module"
        );
    }
}

void test_reconnect_becomes_authoritative_binding() {
    FakeTransport transport;
    Core core{transport};
    transport.incoming.push_back({
        "serial:device-1",
        hello("msg-1", "scale-01", "81A9C5D2")
    });
    transport.incoming.push_back({
        "serial:device-2",
        hello("msg-2", "scale-01", "81A9C5D2")
    });
    transport.incoming.push_back({
        "serial:device-1",
        hello("msg-3", "scale-01", "AAAAAAAA")
    });

    core.poll_once();
    core.poll_once();
    core.poll_once();

    expect(
        transport.sent.size() == 3,
        "reconnect authority responses are missing"
    );

    if (transport.sent.size() == 3) {
        expect(
            transport.sent[2].payload.find(
                "CODE=DUPLICATE_IDENTITY\n"
            ) != std::string::npos,
            "old transport displaced the reconnected authoritative binding"
        );
    }
}

void test_empty_transport_connection_is_ignored() {
    FakeTransport transport;
    Core core{transport};
    transport.incoming.push_back({"", hello()});

    core.poll_once();

    expect(
        transport.sent.empty(),
        "packet without a transport connection received a response"
    );
}

} // namespace

int run_core_tests() {
    failures = 0;

    test_no_packet_is_a_no_op();
    test_valid_hello_produces_ack();
    test_trace_reports_discovery_lifecycle();
    test_transport_connection_is_reused();
    test_frame_error_produces_error_response();
    test_frame_timeout_produces_error_response();
    test_parse_error_produces_error_response();
    test_remaining_frame_failures();
    test_remaining_parse_failures();
    test_remaining_validation_failures();
    test_send_failure_is_traced();
    test_protocol_mismatch_produces_error_response();
    test_duplicate_identity_is_rejected();
    test_same_session_reconnects_on_new_transport();
    test_duplicate_does_not_replace_authoritative_binding();
    test_quarantined_transport_cannot_reopen();
    test_reconnect_becomes_authoritative_binding();
    test_empty_transport_connection_is_ignored();

    return failures;
}
