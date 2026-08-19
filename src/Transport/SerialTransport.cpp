#include "automation_core/Transport/SerialTransport.h"

#include <limits>
#include <utility>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace automation_core {
namespace {
#ifdef _WIN32
class Win32SerialPlatformAdapter final : public ISerialPlatformAdapter {
public:
    bool open(const SerialConfiguration& configuration) override {
        const std::string name = configuration.port.rfind("\\\\.\\", 0) == 0
            ? configuration.port : "\\\\.\\" + configuration.port;
        handle_ = CreateFileA(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                              OPEN_EXISTING, 0, nullptr);
        return handle_ != INVALID_HANDLE_VALUE;
    }
    bool configure(const SerialConfiguration& c) override {
        DCB dcb{}; dcb.DCBlength = sizeof(dcb);
        if (!GetCommState(handle_, &dcb)) return false;
        dcb.BaudRate = c.baud_rate; dcb.ByteSize = c.data_bits;
        dcb.Parity = c.parity == SerialParity::Odd ? ODDPARITY : c.parity == SerialParity::Even ? EVENPARITY : NOPARITY;
        dcb.fParity = c.parity != SerialParity::None;
        dcb.StopBits = c.stop_bits == SerialStopBits::Two ? TWOSTOPBITS : ONESTOPBIT;
        if (!SetCommState(handle_, &dcb)) return false;
        COMMTIMEOUTS t{};
        // MAXDWORD makes a zero timeout return immediately. Positive values are
        // bounded total constants and never request an infinite operation.
        t.ReadIntervalTimeout = c.read_timeout.count() == 0 ? MAXDWORD : 0;
        t.ReadTotalTimeoutConstant = static_cast<DWORD>(c.read_timeout.count());
        t.WriteTotalTimeoutConstant = static_cast<DWORD>(c.write_timeout.count());
        return SetCommTimeouts(handle_, &t) != FALSE;
    }
    void close() noexcept override { if (handle_ != INVALID_HANDLE_VALUE) { CloseHandle(handle_); handle_ = INVALID_HANDLE_VALUE; } }
    SerialOutcome read(std::string& bytes) override {
        char buffer[4096]; DWORD count = 0;
        if (!ReadFile(handle_, buffer, sizeof(buffer), &count, nullptr))
            return disconnect(GetLastError()) ? SerialOutcome::ReadDisconnected : SerialOutcome::ReadFailed;
        bytes.assign(buffer, count); return count == 0 ? SerialOutcome::Idle : SerialOutcome::Ok;
    }
    SerialOutcome write(const std::string& bytes, size_t& written) override {
        // WriteFile accepts a DWORD count.  Do not narrow a larger C++ buffer
        // and accidentally send an unreported prefix as a complete write.
        if (bytes.size() > static_cast<size_t>((std::numeric_limits<DWORD>::max)())) {
            written = 0;
            return SerialOutcome::PartialWrite;
        }
        DWORD count = 0;
        if (!WriteFile(handle_, bytes.data(), static_cast<DWORD>(bytes.size()), &count, nullptr))
            return disconnect(GetLastError()) ? SerialOutcome::WriteDisconnected : SerialOutcome::WriteFailed;
        written = count; return count == bytes.size() ? SerialOutcome::Ok : SerialOutcome::PartialWrite;
    }
private:
    static bool disconnect(DWORD error) noexcept { return error == ERROR_DEVICE_NOT_CONNECTED || error == ERROR_INVALID_HANDLE || error == ERROR_GEN_FAILURE; }
    HANDLE handle_{INVALID_HANDLE_VALUE};
};
#else
class Win32SerialPlatformAdapter final : public ISerialPlatformAdapter {
public:
    bool open(const SerialConfiguration&) override { return false; }
    bool configure(const SerialConfiguration&) override { return false; }
    void close() noexcept override {}
    SerialOutcome read(std::string&) override { return SerialOutcome::ReadFailed; }
    SerialOutcome write(const std::string&, size_t&) override { return SerialOutcome::WriteFailed; }
};
#endif
} // namespace

SerialTransport::SerialTransport(std::string device_name)
    : configuration_{std::move(device_name)}, adapter_(std::make_shared<Win32SerialPlatformAdapter>()) {}
SerialTransport::SerialTransport(SerialConfiguration configuration, std::shared_ptr<ISerialPlatformAdapter> adapter)
    : configuration_(std::move(configuration)), adapter_(std::move(adapter)) {}
SerialTransport::~SerialTransport() { close(); }

bool SerialTransport::valid_configuration(const SerialConfiguration& c) noexcept {
    constexpr auto maximum_timeout = std::chrono::hours{24};
    return !c.port.empty() && c.baud_rate > 0 && c.baud_rate <= 4000000 &&
        c.data_bits >= 5 && c.data_bits <= 8 &&
        (c.parity == SerialParity::None || c.parity == SerialParity::Odd || c.parity == SerialParity::Even) &&
        (c.stop_bits == SerialStopBits::One || c.stop_bits == SerialStopBits::Two) &&
        c.read_timeout.count() >= 0 && c.write_timeout.count() > 0 &&
        c.read_timeout <= maximum_timeout && c.write_timeout <= maximum_timeout;
}
bool SerialTransport::open() {
    // Reject invalid settings before touching an existing or new device.
    if (!adapter_ || !valid_configuration(configuration_)) {
        last_outcome_ = SerialOutcome::InvalidConfiguration;
        return false;
    }
    close();
    if (!adapter_->open(configuration_)) { last_outcome_ = SerialOutcome::OpenFailed; return false; }
    if (!adapter_->configure(configuration_)) { adapter_->close(); last_outcome_ = SerialOutcome::ConfigurationFailed; return false; }
    open_ = true; last_outcome_ = SerialOutcome::Ok; return true;
}
bool SerialTransport::reconnect() { close(); return open(); }
void SerialTransport::close() noexcept {
    if (adapter_ && open_) {
        adapter_->close();
        lifecycle_break_ = true;
    }
    open_ = false;
}
bool SerialTransport::is_open() const noexcept { return open_; }
SerialOutcome SerialTransport::last_outcome() const noexcept { return last_outcome_; }
const SerialConfiguration& SerialTransport::configuration() const noexcept { return configuration_; }
std::optional<std::string> SerialTransport::consume_lifecycle_break() {
    if (!lifecycle_break_) return std::nullopt;
    lifecycle_break_ = false;
    return configuration_.port;
}
void SerialTransport::terminal_failure(SerialOutcome outcome) noexcept { last_outcome_ = outcome; close(); }
std::optional<TransportPacket> SerialTransport::receive() {
    if (!open_) { last_outcome_ = SerialOutcome::NotOpen; return std::nullopt; }
    std::string bytes; const SerialOutcome result = adapter_->read(bytes); last_outcome_ = result;
    if (result == SerialOutcome::Idle) return std::nullopt;
    if (result != SerialOutcome::Ok) { terminal_failure(result); return std::nullopt; }
    return TransportPacket{configuration_.port, std::move(bytes), std::chrono::milliseconds{0}};
}
bool SerialTransport::send(const std::string& connection_id, const std::string& payload) {
    if (!open_) { last_outcome_ = SerialOutcome::NotOpen; return false; }
    if (connection_id != configuration_.port) { last_outcome_ = SerialOutcome::WrongConnection; return false; }
    size_t written = 0; const SerialOutcome result = adapter_->write(payload, written); last_outcome_ = result;
    if (result == SerialOutcome::Ok && written == payload.size()) return true;
    if (result != SerialOutcome::PartialWrite) terminal_failure(result);
    return false;
}
} // namespace automation_core
