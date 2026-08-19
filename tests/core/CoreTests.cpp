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

std::string heartbeat(
    const std::string& message_id = "heartbeat-1",
    const std::string& module_id = "scale-01",
    const std::string& session_id = "81A9C5D2",
    const std::uint32_t sequence = 1,
    const std::uint64_t uptime_ms = 1000
) {
    return
        "HEARTBEAT\n"
        "MSG=" + message_id + "\n"
        "ID=" + module_id + "\n"
        "SESSION=" + session_id + "\n"
        "SEQ=" + std::to_string(sequence) + "\n"
        "UPTIME_MS=" + std::to_string(uptime_ms) + "\n"
        "STATE=ready\n"
        "FAULTS=0\n"
        "END\n";
}

std::string capabilities(
    const std::string& message_id="cap-1",
    const std::uint32_t revision=1
) {
    return "CAPABILITIES\nMSG="+message_id+"\nID=scale-01\nSESSION=81A9C5D2\nREV="+std::to_string(revision)+"\nCOUNT=1\nITEM.0.NAME=weight\nITEM.0.TYPE=measurement\nITEM.0.DATA_TYPE=float\nITEM.0.ACCESS=read\nEND\n";
}

std::string command_capabilities() {
    return "CAPABILITIES\nMSG=command-cap-1\nID=scale-01\nSESSION=81A9C5D2\nREV=2\nCOUNT=1\nITEM.0.NAME=tare\nITEM.0.TYPE=command\nITEM.0.ACCESS=command\nEND\n";
}

std::string orchestration_capabilities() {
    return "CAPABILITIES\nMSG=orchestration-cap-1\nID=scale-01\nSESSION=81A9C5D2\nREV=2\nCOUNT=2\nITEM.0.NAME=tare\nITEM.0.TYPE=command\nITEM.0.ACCESS=command\nITEM.1.NAME=weight\nITEM.1.TYPE=measurement\nITEM.1.DATA_TYPE=float\nITEM.1.ACCESS=read\nEND\n";
}

std::string command_ack(const std::string& transaction_id, const std::string& status = "ACCEPTED") {
    const std::string rejection_code = status == "REJECTED" ? "CODE=COMMAND_DENIED\n" : "";
    return "COMMAND_ACK\nMSG=ack-" + transaction_id + "\nTX=" + transaction_id + "\nID=scale-01\nSESSION=81A9C5D2\nSTATUS=" + status + "\n" + rejection_code + "END\n";
}

std::string command_result(const std::string& transaction_id, const std::string& status = "SUCCESS") {
    return "COMMAND_RESULT\nMSG=result-" + transaction_id + "\nTX=" + transaction_id + "\nID=scale-01\nSESSION=81A9C5D2\nSTATUS=" + status + "\nRESULT=done\nEND\n";
}

CommandMessage tare_command(const std::string& transaction_id) {
    return {"command-" + transaction_id, transaction_id, "scale-01", "81A9C5D2", "tare", 2, "now"};
}

std::string measurement(const std::string& quality="good", const std::string& value="42.5", const std::uint32_t sequence=1) {
    return "MEASUREMENT\nMSG=measure-" + std::to_string(sequence) + "\nID=scale-01\nSESSION=81A9C5D2\nCAP=weight\nSEQ=" + std::to_string(sequence) + "\nVALUE=" + value + "\nQUALITY="+quality+"\nEND\n";
}

void test_no_packet_is_a_no_op() {
    FakeTransport transport;
    Core core{transport};

    core.poll_once();

    expect(transport.sent.empty(), "empty poll produced a response");
}

void test_scheduler_clock_advances_on_idle_poll() {
    FakeTransport transport;
    int clock_reads = 0;
    Core core{
        transport,
        [&clock_reads] {
            ++clock_reads;
            return Core::TimePoint{};
        }
    };

    core.poll_once();
    core.poll_once();

    expect(
        clock_reads == 2,
        "idle polls did not advance the scheduler clock"
    );
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
        "UNKNOWN\nMSG=msg-1\nEND\n"
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
        std::string(16385, 'X'),
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

void test_idle_tick_expires_module_and_requires_rediscovery() {
    FakeTransport transport;
    Core::TimePoint now{};
    Core core{
        transport,
        [&now] { return now; },
        std::chrono::milliseconds{3000}
    };

    transport.incoming.push_back({"serial:device-1", hello()});
    core.poll_once();

    now += std::chrono::seconds{1};
    transport.incoming.push_back({
        "serial:device-1",
        heartbeat()
    });
    core.poll_once();

    now += std::chrono::seconds{3};
    core.poll_once();

    transport.incoming.push_back({
        "serial:device-1",
        heartbeat("heartbeat-2", "scale-01", "81A9C5D2", 2, 4000)
    });
    core.poll_once();

    expect(
        transport.sent.size() == 2,
        "offline heartbeat did not produce one error"
    );

    if (transport.sent.size() == 2) {
        expect(
            transport.sent[1].payload.find(
                "CODE=MODULE_OFFLINE\n"
            ) != std::string::npos,
            "offline module heartbeat was not rejected"
        );
    }

    now += std::chrono::milliseconds{1};
    transport.incoming.push_back({
        "serial:device-1",
        hello("msg-2", "scale-01", "AAAAAAAA")
    });
    core.poll_once();

    expect(
        transport.sent.size() == 3 &&
        transport.sent[2].payload.find("HELLO_ACK\n") !=
            std::string::npos,
        "offline module could not rediscover"
    );
}

void test_valid_and_duplicate_heartbeat_are_silent() {
    FakeTransport transport;
    Core core{transport};
    transport.incoming.push_back({"serial:device-1", hello()});
    transport.incoming.push_back({
        "serial:device-1",
        heartbeat()
    });
    transport.incoming.push_back({
        "serial:device-1",
        heartbeat("heartbeat-2")
    });

    core.poll_once();
    core.poll_once();
    core.poll_once();

    expect(
        transport.sent.size() == 1,
        "valid or duplicate heartbeat produced a wire response"
    );
}

void test_heartbeat_ordering_and_rollover_through_core() {
    FakeTransport stale_transport;
    Core stale_core{stale_transport};
    stale_transport.incoming.push_back({
        "serial:device-1",
        hello()
    });
    stale_transport.incoming.push_back({
        "serial:device-1",
        heartbeat("heartbeat-1", "scale-01", "81A9C5D2", 10, 1000)
    });
    stale_transport.incoming.push_back({
        "serial:device-1",
        heartbeat("heartbeat-2", "scale-01", "81A9C5D2", 9, 1100)
    });

    stale_core.poll_once();
    stale_core.poll_once();
    stale_core.poll_once();

    expect(
        stale_transport.sent.size() == 2 &&
        stale_transport.sent[1].payload.find(
            "CODE=HEARTBEAT_ORDER\n"
        ) != std::string::npos,
        "out-of-order heartbeat was not rejected through Core"
    );

    FakeTransport rollover_transport;
    Core rollover_core{rollover_transport};
    rollover_transport.incoming.push_back({
        "serial:device-1",
        hello()
    });
    rollover_transport.incoming.push_back({
        "serial:device-1",
        heartbeat(
            "heartbeat-1",
            "scale-01",
            "81A9C5D2",
            UINT32_MAX,
            1000
        )
    });
    rollover_transport.incoming.push_back({
        "serial:device-1",
        heartbeat("heartbeat-2", "scale-01", "81A9C5D2", 0, 1100)
    });

    rollover_core.poll_once();
    rollover_core.poll_once();
    rollover_core.poll_once();

    expect(
        rollover_transport.sent.size() == 1,
        "valid heartbeat rollover produced an error"
    );
}

void test_heartbeat_identity_errors_through_core() {
    FakeTransport unknown;
    Core unknown_core{unknown};
    unknown.incoming.push_back({
        "serial:device-1",
        heartbeat()
    });
    unknown_core.poll_once();

    expect(
        unknown.sent.size() == 1 &&
        unknown.sent[0].payload.find("CODE=UNKNOWN_MODULE\n") !=
            std::string::npos,
        "unknown heartbeat module was not rejected through Core"
    );

    FakeTransport mismatch;
    Core mismatch_core{mismatch};
    mismatch.incoming.push_back({"serial:device-1", hello()});
    mismatch.incoming.push_back({
        "serial:device-2",
        heartbeat()
    });
    mismatch.incoming.push_back({
        "serial:device-1",
        heartbeat(
            "heartbeat-2",
            "scale-01",
            "AAAAAAAA"
        )
    });

    mismatch_core.poll_once();
    mismatch_core.poll_once();
    mismatch_core.poll_once();

    expect(
        mismatch.sent.size() == 3 &&
        mismatch.sent[1].payload.find("CODE=CONNECTION_MISMATCH\n") !=
            std::string::npos &&
        mismatch.sent[2].payload.find("CODE=SESSION_MISMATCH\n") !=
            std::string::npos,
        "heartbeat connection or session mismatch was not rejected"
    );
}

void test_invalid_heartbeat_values_through_core() {
    expect_core_error(
        "HEARTBEAT\n"
        "MSG=heartbeat-1\n"
        "ID=scale-01\n"
        "SESSION=81A9C5D2\n"
        "SEQ=1\n"
        "UPTIME_MS=1000\n"
        "STATE=offline\n"
        "FAULTS=0\n"
        "END\n",
        "INVALID_VALUE",
        "invalid heartbeat state"
    );

    expect_core_error(
        "HEARTBEAT\n"
        "MSG=heartbeat-1\n"
        "ID=scale-01\n"
        "SESSION=81A9C5D2\n"
        "SEQ=4294967296\n"
        "UPTIME_MS=1000\n"
        "STATE=ready\n"
        "FAULTS=0\n"
        "END\n",
        "INVALID_VALUE",
        "overflowing heartbeat sequence"
    );

    expect_core_error(
        "HEARTBEAT\n"
        "MSG=heartbeat-1\n"
        "ID=scale-01\n"
        "SESSION=81A9C5D2\n"
        "SEQ=1\n"
        "UPTIME_MS=1000\n"
        "STATE=ready\n"
        "FAULTS=4294967296\n"
        "END\n",
        "INVALID_VALUE",
        "overflowing heartbeat fault count"
    );
}

void test_uptime_regression_and_quarantine_through_core() {
    FakeTransport regression;
    Core regression_core{regression};
    regression.incoming.push_back({"serial:device-1", hello()});
    regression.incoming.push_back({
        "serial:device-1",
        heartbeat("heartbeat-1", "scale-01", "81A9C5D2", 1, 1000)
    });
    regression.incoming.push_back({
        "serial:device-1",
        heartbeat("heartbeat-2", "scale-01", "81A9C5D2", 2, 999)
    });

    regression_core.poll_once();
    regression_core.poll_once();
    regression_core.poll_once();

    expect(
        regression.sent.size() == 2 &&
        regression.sent[1].payload.find("CODE=UPTIME_REGRESSION\n") !=
            std::string::npos,
        "uptime regression was not surfaced through Core"
    );

    FakeTransport quarantine;
    Core quarantine_core{quarantine};
    quarantine.incoming.push_back({
        "serial:device-1",
        hello("msg-1", "scale-01", "81A9C5D2")
    });
    quarantine.incoming.push_back({
        "serial:device-2",
        hello("msg-2", "scale-01", "AAAAAAAA")
    });
    quarantine.incoming.push_back({
        "serial:device-2",
        heartbeat(
            "heartbeat-1",
            "scale-01",
            "AAAAAAAA"
        )
    });

    quarantine_core.poll_once();
    quarantine_core.poll_once();
    quarantine_core.poll_once();

    expect(
        quarantine.sent.size() == 3 &&
        quarantine.sent[2].payload.find(
            "CODE=CONNECTION_QUARANTINED\n"
        ) != std::string::npos,
        "quarantined connection delivered a heartbeat"
    );
}

void test_health_lifecycle_tracing() {
    FakeTransport transport;
    std::ostringstream trace;
    Core::TimePoint now{};
    Core core{
        transport,
        trace,
        [&now] { return now; },
        std::chrono::milliseconds{3000}
    };

    transport.incoming.push_back({"serial:device-1", hello()});
    transport.incoming.push_back({
        "serial:device-1",
        heartbeat()
    });
    transport.incoming.push_back({
        "serial:device-1",
        heartbeat("heartbeat-2")
    });

    static_cast<void>(core.poll_once());
    now += std::chrono::seconds{1};
    static_cast<void>(core.poll_once());
    now += std::chrono::seconds{1};
    static_cast<void>(core.poll_once());
    now += std::chrono::seconds{2};
    static_cast<void>(core.poll_once());

    const std::string output = trace.str();

    for (const std::string event : {
        "event=heartbeat_accepted",
        "event=heartbeat_duplicate",
        "event=module_offline"
    }) {
        expect(
            output.find(event) != std::string::npos,
            "health trace is missing " + event
        );
    }

    expect(
        output.find("state=ready seq=1 uptime_ms=1000 faults=0") !=
            std::string::npos,
        "heartbeat trace omitted health fields"
    );
}

void test_reboot_suspicion_is_traced() {
    FakeTransport transport;
    std::ostringstream trace;
    Core core{transport, trace};
    transport.incoming.push_back({"serial:device-1", hello()});
    transport.incoming.push_back({
        "serial:device-1",
        heartbeat("heartbeat-1", "scale-01", "81A9C5D2", 1, 1000)
    });
    transport.incoming.push_back({
        "serial:device-1",
        heartbeat("heartbeat-2", "scale-01", "81A9C5D2", 2, 999)
    });

    static_cast<void>(core.poll_once());
    static_cast<void>(core.poll_once());
    static_cast<void>(core.poll_once());

    expect(
        trace.str().find("event=module_reboot_suspected") !=
            std::string::npos,
        "uptime regression did not trace suspected reboot"
    );
}

void test_capability_lifecycle_tracing() {
    FakeTransport transport;std::ostringstream trace;Core core{transport,trace};
    transport.incoming.push_back({"serial:device-1",hello()});
    transport.incoming.push_back({"serial:device-1",capabilities()});
    transport.incoming.push_back({"serial:device-1",capabilities("cap-2")});
    core.poll_once();core.poll_once();core.poll_once();
    const auto output=trace.str();
    expect(output.find("event=capabilities_accepted")!=std::string::npos,"accepted capabilities not traced");
    expect(output.find("event=capabilities_idempotent")!=std::string::npos,"idempotent capabilities not traced");
    expect(output.find("revision=1 count=1 names=weight")!=std::string::npos,"capability summary missing");
}

void test_capability_revision_conflict_through_core() {
    FakeTransport transport;Core core{transport};
    transport.incoming.push_back({"serial:device-1",hello()});
    transport.incoming.push_back({"serial:device-1",capabilities()});
    std::string changed=capabilities("cap-2");
    const auto position=changed.find("NAME=weight");
    changed.replace(position,std::string("NAME=weight").size(),"NAME=mass");
    transport.incoming.push_back({"serial:device-1",changed});
    core.poll_once();core.poll_once();core.poll_once();
    expect(transport.sent.size()==3&&transport.sent[2].payload.find("CODE=CAPABILITY_REVISION_CONFLICT\n")!=std::string::npos,"same-revision capability change accepted through Core");
}

void test_measurement_end_to_end() {
    FakeTransport transport;std::ostringstream trace;Core core{transport,trace};
    transport.incoming.push_back({"serial:device-1",hello()});
    transport.incoming.push_back({"serial:device-1",capabilities()});
    transport.incoming.push_back({"serial:device-1",measurement()});
    core.poll_once();core.poll_once();core.poll_once();
    expect(transport.sent.size()==2,"accepted measurement unexpectedly produced an ACK");
    expect(trace.str().find("event=measurement_accepted")!=std::string::npos,"accepted measurement was not traced");

    transport.incoming.push_back({"serial:device-1",measurement("stale")});
    core.poll_once();
    expect(transport.sent.size()==3&&transport.sent.back().payload.find("CODE=INVALID_VALUE")!=std::string::npos,"reserved stale wire quality was not rejected");
}

void test_command_transaction_invalid_lifecycle_transitions() {
    FakeTransport transport;
    std::ostringstream trace;
    Core core{transport, trace};
    transport.incoming.push_back({"serial:device-1", hello()});
    transport.incoming.push_back({"serial:device-1", command_capabilities()});
    core.poll_once(); core.poll_once();

    expect(core.dispatch_command(tare_command("tx-state")) == CommandRejection::None, "state-machine command was not dispatched");
    transport.incoming.push_back({"serial:device-2", command_ack("tx-state")});
    core.poll_once();
    const auto wrong_connection_ack = core.find_command_transaction("tx-state");
    expect(wrong_connection_ack && wrong_connection_ack->state == CommandTransactionState::Dispatched, "non-authoritative ACK changed transaction state");

    transport.incoming.push_back({"serial:device-1", command_result("tx-state")});
    core.poll_once();
    const auto result_before_ack = core.find_command_transaction("tx-state");
    expect(result_before_ack && result_before_ack->state == CommandTransactionState::Dispatched, "RESULT before ACK changed transaction state");

    transport.incoming.push_back({"serial:device-1", command_ack("tx-state")});
    core.poll_once();
    const auto acknowledged = core.find_command_transaction("tx-state");
    expect(acknowledged && acknowledged->state == CommandTransactionState::Acknowledged, "valid ACK did not advance transaction state");

    transport.incoming.push_back({"serial:device-1", command_ack("tx-state")});
    core.poll_once();
    const auto duplicate_ack = core.find_command_transaction("tx-state");
    expect(duplicate_ack && duplicate_ack->state == CommandTransactionState::Acknowledged, "duplicate ACK changed transaction state");
    expect(trace.str().find("event=command_correlation_rejected") != std::string::npos, "Core did not trace rejected non-authoritative response");
    expect(trace.str().find("event=command_lifecycle_rejected") != std::string::npos, "Core did not trace rejected lifecycle transitions");
}

void test_command_transaction_lifecycle_and_rejections() {
    FakeTransport transport;
    std::ostringstream trace;
    Core core{transport, trace};
    transport.incoming.push_back({"serial:device-1", hello()});
    transport.incoming.push_back({"serial:device-1", command_capabilities()});
    core.poll_once(); core.poll_once();

    expect(core.dispatch_command(tare_command("tx-1")) == CommandRejection::None, "accepted command was not dispatched");
    expect(transport.sent.size() == 3 && transport.sent.back().connection_id == "serial:device-1", "command was not routed over authority");
    expect(core.dispatch_command(tare_command("tx-1")) == CommandRejection::Lifecycle, "duplicate transaction was accepted");
    CommandMessage bad_revision = tare_command("tx-bad"); bad_revision.capability_revision = 1;
    expect(core.dispatch_command(bad_revision) == CommandRejection::Capability, "unaccepted capability revision was routed");

    transport.incoming.push_back({"serial:device-2", command_ack("tx-1")});
    transport.incoming.push_back({"serial:device-1", command_result("tx-1")});
    transport.incoming.push_back({"serial:device-1", command_ack("tx-1")});
    transport.incoming.push_back({"serial:device-1", command_ack("tx-1")});
    transport.incoming.push_back({"serial:device-1", command_result("missing")});
    for (int i = 0; i != 5; ++i) core.poll_once();
    const auto acknowledged = core.find_command_transaction("tx-1");
    expect(acknowledged && acknowledged->state == CommandTransactionState::Acknowledged, "wrong-connection RESULT or duplicate ACK changed the transaction");

    transport.incoming.push_back({"serial:device-1", command_result("tx-1")});
    core.poll_once();
    const auto completed = core.find_command_transaction("tx-1");
    expect(completed && completed->state == CommandTransactionState::Succeeded, "correlated RESULT did not complete transaction");

    expect(core.dispatch_command(tare_command("tx-2")) == CommandRejection::None, "second command was not dispatched");
    transport.incoming.push_back({"serial:device-1", command_ack("tx-2", "REJECTED")});
    core.poll_once();
    const auto rejected = core.find_command_transaction("tx-2");
    expect(rejected && rejected->state == CommandTransactionState::Rejected && rejected->rejection == CommandRejection::Lifecycle, "rejected ACK did not record rejection class");
    for (const std::string event : {"event=command_dispatched", "event=command_correlation_rejected", "event=command_lifecycle_rejected", "event=command_result", "event=command_rejected"})
        expect(trace.str().find(event) != std::string::npos, "command trace is missing " + event);
}

void test_command_timeout_and_authority_loss() {
    FakeTransport timeout_transport;
    Core::TimePoint now{};
    Core timeout_core{timeout_transport, [&now] { return now; }, std::chrono::milliseconds{3000}};
    timeout_transport.incoming.push_back({"serial:device-1", hello()});
    timeout_transport.incoming.push_back({"serial:device-1", command_capabilities()});
    timeout_core.poll_once(); timeout_core.poll_once();
    expect(timeout_core.dispatch_command(tare_command("tx-timeout"), std::chrono::milliseconds{10}) == CommandRejection::None, "timeout command was not dispatched");
    now += std::chrono::milliseconds{10}; timeout_core.poll_once();
    const auto timed_out = timeout_core.find_command_transaction("tx-timeout");
    expect(timed_out && timed_out->state == CommandTransactionState::TimedOut && timed_out->rejection == CommandRejection::Timeout, "monotonic deadline did not expire command");

    FakeTransport loss_transport;
    Core::TimePoint loss_now{};
    Core loss_core{loss_transport, [&loss_now] { return loss_now; }, std::chrono::milliseconds{3}};
    loss_transport.incoming.push_back({"serial:device-1", hello()});
    loss_transport.incoming.push_back({"serial:device-1", command_capabilities()});
    loss_core.poll_once(); loss_core.poll_once();
    expect(loss_core.dispatch_command(tare_command("tx-loss")) == CommandRejection::None, "authority-loss command was not dispatched");
    loss_now += std::chrono::milliseconds{3}; loss_core.poll_once();
    const auto lost = loss_core.find_command_transaction("tx-loss");
    expect(lost && lost->state == CommandTransactionState::AuthorityLost && lost->rejection == CommandRejection::AuthorityLoss, "authority loss did not invalidate transaction");
}

void test_process_ordered_end_to_end_and_correlation() {
    FakeTransport transport; std::ostringstream trace; Core core{transport, trace};
    transport.incoming.push_back({"serial:device-1", hello()});
    transport.incoming.push_back({"serial:device-1", orchestration_capabilities()});
    transport.incoming.push_back({"serial:device-1", measurement("good", "0", 1)});
    core.poll_once(); core.poll_once(); core.poll_once();
    ProcessDefinition definition{{"process-ordered"}, {
        {{"command-step"}, ProcessStepKind::Command, std::chrono::milliseconds{100}, tare_command("process-command"), {}},
        {{"wait-step"}, ProcessStepKind::MeasurementCondition, std::chrono::milliseconds{100}, {}, {"scale-01", "weight", "42.5"}}
    }};
    expect(core.start_process(definition, {"run-ordered"}), "ordered process did not start");
    auto run = core.find_process_run({"run-ordered"});
    expect(run && run->current_step == 0 && transport.sent.size() == 3, "later step started before command dispatch");
    core.poll_once();
    run = core.find_process_run({"run-ordered"});
    expect(run && run->current_step == 0, "later step advanced without command completion");
    transport.incoming.push_back({"serial:device-1", command_ack("process-command")}); core.poll_once();
    transport.incoming.push_back({"serial:device-1", command_result("process-command")}); core.poll_once();
    core.poll_once();
    run = core.find_process_run({"run-ordered"});
    expect(run && run->state == ProcessRunState::Running && run->current_step == 1, "successful command did not advance exactly one ordered step");
    core.poll_once();
    run = core.find_process_run({"run-ordered"});
    expect(run && run->current_step == 1, "measurement step advanced before condition became valid");
    transport.incoming.push_back({"serial:device-1", measurement("good", "42.5", 2)}); core.poll_once();
    core.poll_once(); core.poll_once(); core.poll_once();
    run = core.find_process_run({"run-ordered"});
    expect(run && run->state == ProcessRunState::Succeeded, "final valid measurement step did not succeed process");
    const std::string output = trace.str();
    expect(output.find("process=process-ordered run=run-ordered step=command-step command=process-command") != std::string::npos,
           "process, run, step, and command correlation was not traced");
}

void test_process_command_failure_and_authority_loss() {
    FakeTransport transport; Core core{transport};
    transport.incoming.push_back({"serial:device-1", hello()}); transport.incoming.push_back({"serial:device-1", command_capabilities()});
    core.poll_once(); core.poll_once();
    ProcessDefinition failure{{"process-failure"}, {{{"step"}, ProcessStepKind::Command, std::chrono::milliseconds{100}, tare_command("failure-command"), {}}}};
    expect(core.start_process(failure, {"run-failure"}), "failure process did not start");
    transport.incoming.push_back({"serial:device-1", command_ack("failure-command")}); core.poll_once();
    transport.incoming.push_back({"serial:device-1", command_result("failure-command", "FAILURE")}); core.poll_once(); core.poll_once();
    const auto failed = core.find_process_run({"run-failure"});
    expect(failed && failed->state == ProcessRunState::CommandFailed, "failed command result did not terminate process");
    ProcessDefinition rejected{{"process-ack-rejected"}, {{{"step"}, ProcessStepKind::Command, std::chrono::milliseconds{100}, tare_command("rejected-command"), {}}}};
    expect(core.start_process(rejected, {"run-ack-rejected"}), "rejection process did not start");
    transport.incoming.push_back({"serial:device-1", command_ack("rejected-command", "REJECTED")}); core.poll_once(); core.poll_once();
    const auto rejected_run = core.find_process_run({"run-ack-rejected"});
    expect(rejected_run && rejected_run->state == ProcessRunState::CommandRejected, "rejected command ACK did not terminate process");

    FakeTransport loss_transport; Core::TimePoint now{};
    Core loss_core{loss_transport, [&now] { return now; }, std::chrono::milliseconds{3}};
    loss_transport.incoming.push_back({"serial:device-1", hello()}); loss_transport.incoming.push_back({"serial:device-1", command_capabilities()});
    loss_core.poll_once(); loss_core.poll_once();
    ProcessDefinition loss{{"process-loss"}, {{{"step"}, ProcessStepKind::Command, std::chrono::milliseconds{100}, tare_command("loss-command"), {}}}};
    expect(loss_core.start_process(loss, {"run-loss"}), "authority-loss process did not start");
    now += std::chrono::milliseconds{3}; loss_core.poll_once();
    const auto lost = loss_core.find_process_run({"run-loss"});
    expect(lost && lost->state == ProcessRunState::AuthorityLost, "authority loss did not terminate active process");
}

void test_process_timeouts_and_measurement_outcomes() {
    FakeTransport command_transport; Core::TimePoint now{};
    Core command_core{command_transport, [&now] { return now; }, std::chrono::milliseconds{10000}};
    command_transport.incoming.push_back({"serial:device-1", hello()}); command_transport.incoming.push_back({"serial:device-1", command_capabilities()});
    command_core.poll_once(); command_core.poll_once();
    ProcessDefinition command_timeout{{"process-command-timeout"}, {{{"step"}, ProcessStepKind::Command, std::chrono::milliseconds{10}, tare_command("timeout-command"), {}}}};
    expect(command_core.start_process(command_timeout, {"run-command-timeout"}, std::chrono::milliseconds{100}), "command timeout process did not start");
    now += std::chrono::milliseconds{10}; command_core.poll_once();
    expect(command_core.find_process_run({"run-command-timeout"})->state == ProcessRunState::CommandTimedOut, "command timeout did not take precedence at equal step deadline");

    FakeTransport measurement_transport; Core::TimePoint measurement_now{};
    Core measurement_core{measurement_transport, [&measurement_now] { return measurement_now; }, std::chrono::milliseconds{10000}};
    measurement_transport.incoming.push_back({"serial:device-1", orchestration_capabilities()});
    measurement_transport.incoming.push_front({"serial:device-1", hello()});
    measurement_transport.incoming.push_back({"serial:device-1", measurement("good", "0", 1)});
    measurement_core.poll_once(); measurement_core.poll_once(); measurement_core.poll_once();
    ProcessDefinition wait{{"process-step-timeout"}, {{{"wait"}, ProcessStepKind::MeasurementCondition, std::chrono::milliseconds{10}, {}, {"scale-01", "weight", "42.5"}}}};
    expect(measurement_core.start_process(wait, {"run-step-timeout"}, std::chrono::milliseconds{100}), "step timeout process did not start");
    measurement_now += std::chrono::milliseconds{10}; measurement_core.poll_once();
    expect(measurement_core.find_process_run({"run-step-timeout"})->state == ProcessRunState::StepTimedOut, "step timeout was not reachable without delay");
    ProcessDefinition run_wait{{"process-run-timeout"}, {{{"wait"}, ProcessStepKind::MeasurementCondition, std::chrono::milliseconds{100}, {}, {"scale-01", "weight", "42.5"}}}};
    expect(measurement_core.start_process(run_wait, {"run-run-timeout"}, std::chrono::milliseconds{10}), "run timeout process did not start");
    measurement_now += std::chrono::milliseconds{10}; measurement_core.poll_once();
    expect(measurement_core.find_process_run({"run-run-timeout"})->state == ProcessRunState::RunTimedOut, "run timeout was not reachable without delay");

    FakeTransport stale_transport; Core::TimePoint stale_now{};
    Core stale_core{stale_transport, [&stale_now] { return stale_now; }, std::chrono::milliseconds{10000}};
    stale_transport.incoming.push_back({"serial:device-1", hello()}); stale_transport.incoming.push_back({"serial:device-1", orchestration_capabilities()}); stale_transport.incoming.push_back({"serial:device-1", measurement()});
    stale_core.poll_once(); stale_core.poll_once(); stale_core.poll_once();
    ProcessDefinition stale_wait{{"process-stale"}, {{{"wait"}, ProcessStepKind::MeasurementCondition, std::chrono::milliseconds{6000}, {}, {"scale-01", "weight", "0"}}}};
    expect(stale_core.start_process(stale_wait, {"run-stale"}), "stale process did not start");
    stale_now += std::chrono::milliseconds{5000}; stale_core.poll_once();
    expect(stale_core.find_process_run({"run-stale"})->state == ProcessRunState::MeasurementStale, "stale measurement satisfied or became unavailable");
    FakeTransport unavailable_transport; Core unavailable_core{unavailable_transport};
    unavailable_transport.incoming.push_back({"serial:device-1", hello()}); unavailable_transport.incoming.push_back({"serial:device-1", orchestration_capabilities()}); unavailable_core.poll_once(); unavailable_core.poll_once();
    expect(unavailable_core.start_process(wait, {"run-unavailable"}), "unavailable process did not start");
    expect(unavailable_core.find_process_run({"run-unavailable"})->state == ProcessRunState::MeasurementUnavailable, "unavailable measurement did not have distinct outcome");
}

void test_process_rejection_and_abort() {
    FakeTransport transport;
    Core core{transport};
    ProcessDefinition rejected{{"process-reject"}, {{ {"step-1"}, ProcessStepKind::Command, std::chrono::milliseconds{10}, tare_command("process-bad"), {} }}};
    expect(core.start_process(rejected, {"run-reject"}), "process run did not start");
    const auto rejected_run = core.find_process_run({"run-reject"});
    expect(rejected_run && rejected_run->state == ProcessRunState::CommandRejected, "command rejection did not terminate process");

    transport.incoming.push_back({"serial:device-1", hello()});
    transport.incoming.push_back({"serial:device-1", command_capabilities()});
    core.poll_once(); core.poll_once();
    ProcessDefinition active{{"process-abort"}, {{ {"step-2"}, ProcessStepKind::Command, std::chrono::milliseconds{100}, tare_command("process-live"), {} }}};
    expect(core.start_process(active, {"run-abort"}), "active process run did not start");
    expect(core.abort_process({"run-abort"}), "active process did not abort");
    const auto aborted = core.find_process_run({"run-abort"});
    expect(aborted && aborted->state == ProcessRunState::Aborted, "abort did not produce terminal state");
}

} // namespace

int run_core_tests() {
    failures = 0;

    test_no_packet_is_a_no_op();
    test_scheduler_clock_advances_on_idle_poll();
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
    test_idle_tick_expires_module_and_requires_rediscovery();
    test_valid_and_duplicate_heartbeat_are_silent();
    test_heartbeat_ordering_and_rollover_through_core();
    test_heartbeat_identity_errors_through_core();
    test_invalid_heartbeat_values_through_core();
    test_uptime_regression_and_quarantine_through_core();
    test_health_lifecycle_tracing();
    test_reboot_suspicion_is_traced();
    test_capability_lifecycle_tracing();
    test_capability_revision_conflict_through_core();
    test_measurement_end_to_end();
    test_command_transaction_invalid_lifecycle_transitions();
    test_command_transaction_lifecycle_and_rejections();
    test_command_timeout_and_authority_loss();
    test_process_ordered_end_to_end_and_correlation();
    test_process_command_failure_and_authority_loss();
    test_process_timeouts_and_measurement_outcomes();
    test_process_rejection_and_abort();

    return failures;
}
