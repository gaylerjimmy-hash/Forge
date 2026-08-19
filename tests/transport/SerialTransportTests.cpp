#include "automation_core/Transport/SerialTransport.h"

#include <chrono>
#include <memory>
#include <utility>
#include <vector>

using namespace automation_core;
namespace {
struct Fake final : ISerialPlatformAdapter {
    bool opened = false, configured = false, fail_open = false, fail_config = false;
    int opens = 0, closes = 0, reads = 0, writes = 0;
    SerialConfiguration seen{};
    std::vector<std::pair<SerialOutcome, std::string>> input;
    SerialOutcome write_result = SerialOutcome::Ok;
    bool open(const SerialConfiguration& c) override { ++opens; seen = c; opened = !fail_open; return opened; }
    bool configure(const SerialConfiguration& c) override { configured = !fail_config; seen = c; return configured; }
    void close() noexcept override { ++closes; opened = false; }
    SerialOutcome read(std::string& b) override {
        ++reads;
        if (input.empty()) return SerialOutcome::Idle;
        const auto x = input.front(); input.erase(input.begin()); b = x.second; return x.first;
    }
    SerialOutcome write(const std::string& b, size_t& n) override {
        ++writes; n = write_result == SerialOutcome::PartialWrite ? b.size() / 2 : b.size(); return write_result;
    }
};
SerialConfiguration configuration() {
    return {"COM9", 115200, 8, SerialParity::None, SerialStopBits::One,
            std::chrono::milliseconds{0}, std::chrono::milliseconds{50}};
}
}

int run_serial_transport_tests() {
    int f = 0;
    auto fake = std::make_shared<Fake>();
    const auto c = configuration();
    SerialTransport t(c, fake);
    if (!t.open() || !fake->configured || fake->seen.port != "COM9" ||
        fake->seen.baud_rate != 115200 || fake->seen.data_bits != 8 ||
        fake->seen.read_timeout != std::chrono::milliseconds{0} ||
        fake->seen.write_timeout != std::chrono::milliseconds{50}) ++f;
    if (!t.send("COM9", "raw") || t.send("other", "raw") || t.last_outcome() != SerialOutcome::WrongConnection) ++f;
    fake->input.push_back({SerialOutcome::Ok, "one"});
    fake->input.push_back({SerialOutcome::Ok, "two"});
    const auto one = t.receive(); const auto two = t.receive(); const auto idle = t.receive();
    if (!one || one->payload != "one" || !two || two->payload != "two" || idle) ++f;

    // A partial write is observable but is not a terminal I/O failure.
    fake->write_result = SerialOutcome::PartialWrite;
    if (t.send("COM9", "raw") || t.last_outcome() != SerialOutcome::PartialWrite || !t.is_open() ||
        t.consume_lifecycle_break()) ++f;

    const auto check_read_failure = [&](SerialOutcome outcome) {
        auto adapter = std::make_shared<Fake>(); SerialTransport subject(c, adapter);
        if (!subject.open()) { ++f; return; }
        adapter->input.push_back({outcome, ""});
        const auto packet = subject.receive();
        const int opens = adapter->opens;
        if (packet || subject.last_outcome() != outcome || subject.is_open() || adapter->closes != 1 ||
            !subject.consume_lifecycle_break() || subject.consume_lifecycle_break() || adapter->opens != opens) ++f;
    };
    check_read_failure(SerialOutcome::ReadFailed);
    check_read_failure(SerialOutcome::ReadDisconnected);
    const auto check_write_failure = [&](SerialOutcome outcome) {
        auto adapter = std::make_shared<Fake>(); SerialTransport subject(c, adapter);
        if (!subject.open()) { ++f; return; }
        adapter->write_result = outcome;
        const int opens = adapter->opens;
        if (subject.send("COM9", "x") || subject.last_outcome() != outcome || subject.is_open() ||
            adapter->closes != 1 || !subject.consume_lifecycle_break() || subject.consume_lifecycle_break() ||
            adapter->opens != opens) ++f;
    };
    check_write_failure(SerialOutcome::WriteFailed);
    check_write_failure(SerialOutcome::WriteDisconnected);

    t.close();
    if (t.is_open() || fake->closes != 1 || !t.consume_lifecycle_break()) ++f;
    const int before_reconnect = fake->opens;
    if (!t.reconnect() || fake->opens != before_reconnect + 1 || !fake->configured) ++f;

    auto positive = c;
    positive.read_timeout = std::chrono::milliseconds{25};
    positive.write_timeout = std::chrono::milliseconds{75};
    auto timing_adapter = std::make_shared<Fake>(); SerialTransport timed(positive, timing_adapter);
    if (!timed.open() || timing_adapter->seen.read_timeout != std::chrono::milliseconds{25} ||
        timing_adapter->seen.write_timeout != std::chrono::milliseconds{75}) ++f;
    for (auto bad : {std::pair<int, int>{-1, 50}, {0, 0},
                     {25 * 60 * 60 * 1000, 50}, {0, 25 * 60 * 60 * 1000}}) {
        auto invalid = c; invalid.read_timeout = std::chrono::milliseconds{bad.first}; invalid.write_timeout = std::chrono::milliseconds{bad.second};
        SerialTransport subject(invalid, std::make_shared<Fake>());
        if (subject.open() || subject.last_outcome() != SerialOutcome::InvalidConfiguration) ++f;
    }
    auto bad = c; bad.port = ""; SerialTransport invalid_port(bad, std::make_shared<Fake>());
    if (invalid_port.open()) ++f;
    bad = c; bad.baud_rate = 0; SerialTransport invalid_baud(bad, std::make_shared<Fake>());
    if (invalid_baud.open()) ++f;
    bad = c; bad.data_bits = 4; SerialTransport invalid_bits(bad, std::make_shared<Fake>());
    if (invalid_bits.open()) ++f;
    auto failed = std::make_shared<Fake>(); failed->fail_open = true; SerialTransport open_failed(c, failed);
    if (open_failed.open() || open_failed.last_outcome() != SerialOutcome::OpenFailed || failed->opens != 1) ++f;
    auto config_failed_adapter = std::make_shared<Fake>(); config_failed_adapter->fail_config = true;
    SerialTransport config_failed(c, config_failed_adapter);
    if (config_failed.open() || config_failed.last_outcome() != SerialOutcome::ConfigurationFailed || config_failed_adapter->closes != 1) ++f;
    return f;
}
