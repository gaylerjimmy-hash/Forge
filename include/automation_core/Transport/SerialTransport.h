#pragma once

#include "automation_core/Transport/ITransport.h"

#include <chrono>
#include <memory>
#include <optional>
#include <string>

namespace automation_core {

enum class SerialParity { None, Odd, Even };
enum class SerialStopBits { One, Two };

struct SerialConfiguration {
    std::string port;
    unsigned long baud_rate{9600};
    unsigned char data_bits{8};
    SerialParity parity{SerialParity::None};
    SerialStopBits stop_bits{SerialStopBits::One};
    std::chrono::milliseconds read_timeout{0};
    std::chrono::milliseconds write_timeout{1000};
};

enum class SerialOutcome {
    Ok, Idle, PartialWrite, NotOpen, InvalidConfiguration, OpenFailed,
    ConfigurationFailed, ReadFailed, ReadDisconnected, WriteFailed,
    WriteDisconnected, WrongConnection
};

// Deliberately contains no platform types.  Test adapters model one bounded,
// raw operation; SerialTransport owns lifecycle and failure policy.
class ISerialPlatformAdapter {
public:
    virtual ~ISerialPlatformAdapter() = default;
    virtual bool open(const SerialConfiguration& configuration) = 0;
    virtual bool configure(const SerialConfiguration& configuration) = 0;
    virtual void close() noexcept = 0;
    virtual SerialOutcome read(std::string& bytes) = 0;
    virtual SerialOutcome write(const std::string& bytes, size_t& written) = 0;
};

class SerialTransport final : public ITransport {
public:
    explicit SerialTransport(std::string device_name);
    SerialTransport(SerialConfiguration configuration,
                    std::shared_ptr<ISerialPlatformAdapter> adapter);
    ~SerialTransport() override;

    bool open();
    bool reconnect();
    void close() noexcept;
    [[nodiscard]] bool is_open() const noexcept;
    [[nodiscard]] SerialOutcome last_outcome() const noexcept;
    [[nodiscard]] const SerialConfiguration& configuration() const noexcept;

    [[nodiscard]] std::optional<TransportPacket> receive() override;
    bool send(const std::string& connection_id, const std::string& payload) override;
    [[nodiscard]] std::optional<std::string> consume_lifecycle_break() override;

private:
    static bool valid_configuration(const SerialConfiguration& configuration) noexcept;
    void terminal_failure(SerialOutcome outcome) noexcept;

    SerialConfiguration configuration_;
    std::shared_ptr<ISerialPlatformAdapter> adapter_;
    bool open_{false};
    bool lifecycle_break_{false};
    SerialOutcome last_outcome_{SerialOutcome::NotOpen};
};

} // namespace automation_core
