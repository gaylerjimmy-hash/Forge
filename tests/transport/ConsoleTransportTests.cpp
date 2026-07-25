#include "automation_core/Transport/ConsoleTransport.h"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace automation_core;

namespace {

int failures = 0;

void expect(const bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "ConsoleTransport: " << message << '\n';
        ++failures;
    }
}

void test_receives_complete_frame() {
    std::istringstream input{
        "HELLO\n"
        "MSG=msg-1\n"
        "END\n"
    };
    std::ostringstream output;
    ConsoleTransport transport{input, output};

    const auto packet = transport.receive();

    expect(packet.has_value(), "complete frame was not received");
    expect(
        packet && packet->connection_id == "console",
        "default connection ID was not used"
    );
    expect(
        packet &&
            packet->payload == "HELLO\nMSG=msg-1\nEND\n",
        "complete frame payload changed"
    );
}

void test_normalizes_crlf() {
    std::istringstream input{"HELLO\r\nMSG=msg-1\r\nEND\r\n"};
    std::ostringstream output;
    ConsoleTransport transport{input, output};

    const auto packet = transport.receive();

    expect(
        packet &&
            packet->payload == "HELLO\nMSG=msg-1\nEND\n",
        "CRLF input was not normalized"
    );
}

void test_receives_one_frame_per_call() {
    std::istringstream input{
        "HELLO\nMSG=msg-1\nEND\n"
        "HELLO\nMSG=msg-2\nEND\n"
    };
    std::ostringstream output;
    ConsoleTransport transport{input, output, "stdin-1"};

    const auto first = transport.receive();
    const auto second = transport.receive();
    const auto exhausted = transport.receive();

    expect(
        first && first->payload.find("MSG=msg-1\n") != std::string::npos,
        "first frame was not returned first"
    );
    expect(
        second && second->payload.find("MSG=msg-2\n") != std::string::npos,
        "second frame was not retained for the next call"
    );
    expect(
        first && second &&
            first->connection_id == "stdin-1" &&
            second->connection_id == "stdin-1",
        "configured connection ID was not retained"
    );
    expect(!exhausted, "exhausted input produced a packet");
}

void test_returns_partial_frame_at_eof() {
    std::istringstream input{"HELLO\nMSG=msg-1\n"};
    std::ostringstream output;
    ConsoleTransport transport{input, output};

    const auto packet = transport.receive();

    expect(packet.has_value(), "partial frame was discarded at EOF");
    expect(
        packet && packet->payload == "HELLO\nMSG=msg-1\n",
        "partial frame payload changed"
    );
}

void test_sends_to_matching_connection() {
    std::istringstream input;
    std::ostringstream output;
    ConsoleTransport transport{input, output, "stdin-1"};

    expect(
        transport.send("stdin-1", "HELLO_ACK\nEND\n"),
        "send to matching connection failed"
    );
    expect(
        output.str() == "HELLO_ACK\nEND\n",
        "send wrote the wrong payload"
    );
}

void test_rejects_wrong_connection() {
    std::istringstream input;
    std::ostringstream output;
    ConsoleTransport transport{input, output, "stdin-1"};

    expect(
        !transport.send("stdin-2", "ERROR\nEND\n"),
        "send accepted a different connection"
    );
    expect(output.str().empty(), "rejected send wrote output");
}

void test_rejects_empty_connection_id() {
    std::istringstream input;
    std::ostringstream output;
    bool rejected = false;

    try {
        ConsoleTransport transport{input, output, ""};
        static_cast<void>(transport);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }

    expect(rejected, "empty connection ID was accepted");
}

} // namespace

int run_console_transport_tests() {
    failures = 0;

    test_receives_complete_frame();
    test_normalizes_crlf();
    test_receives_one_frame_per_call();
    test_returns_partial_frame_at_eof();
    test_sends_to_matching_connection();
    test_rejects_wrong_connection();
    test_rejects_empty_connection_id();

    return failures;
}
