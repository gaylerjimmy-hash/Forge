#include "automation_core/Persistence/PersistenceStore.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace automation_core;
namespace {
int failures = 0;
void expect(bool value, const std::string& text) { if (!value) { std::cerr << "Persistence: " << text << '\n'; ++failures; } }

DurableRecoveryState complete_state() {
    DurableRecoveryState state;
    state.revision = 7;
    state.modules = {{"module-a", "session-a"}};
    state.interrupted_processes = {{"process-a", "run-a", "interrupted"}};
    state.completed_processes = {{"process-b", "run-b", "succeeded"}};
    state.pending_commands = {{"tx-a", "module-a", "session-a", "open"}};
    state.non_resettable_faults = {{"fault-a", 1, 2, 0}};
    state.configuration_revision = "configuration-7";
    state.calibrations = {{"calibration-a", "revision-3"}};
    state.persisted_operator_mode = OperatorMode::Automatic;
    return state;
}
std::uint32_t read32(const std::vector<std::uint8_t>& bytes, std::size_t& at) {
    const std::uint32_t value = static_cast<std::uint32_t>(bytes[at]) |
        (static_cast<std::uint32_t>(bytes[at + 1]) << 8) |
        (static_cast<std::uint32_t>(bytes[at + 2]) << 16) |
        (static_cast<std::uint32_t>(bytes[at + 3]) << 24);
    at += 4; return value;
}
void skip_text(const std::vector<std::uint8_t>& bytes, std::size_t& at) { at += read32(bytes, at); }
std::uint32_t checksum(const std::vector<std::uint8_t>& bytes, std::size_t end) { std::uint32_t h=2166136261u; for(std::size_t i=0;i<end;++i) { h^=bytes[i]; h*=16777619u; } return h; }
void put32(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint32_t value) { for(int i=0;i<4;++i) bytes[at+i]=static_cast<std::uint8_t>(value>>(8*i)); }
void refresh_checksum(std::vector<std::uint8_t>& bytes) { put32(bytes, bytes.size()-4, checksum(bytes, bytes.size()-4)); }
// This cursor follows every serialized field boundary.  Mutation fixtures use
// these locations rather than searching encoded text or guessing offsets.
struct ImageCursor {
    explicit ImageCursor(const std::vector<std::uint8_t>& image) : bytes(image), at(10) {}
    const std::vector<std::uint8_t>& bytes;
    std::size_t at;
    std::size_t count() { const std::size_t result=at; read32(bytes, at); return result; }
    std::size_t string_length() { const std::size_t result=at; skip_text(bytes, at); return result; }
    void modules() { const auto n=read32(bytes,at); for(std::uint32_t i=0;i<n;++i){string_length();string_length();} }
    void processes() { const auto n=read32(bytes,at); for(std::uint32_t i=0;i<n;++i)for(int field=0;field<3;++field)string_length(); }
    void pending() { const auto n=read32(bytes,at); for(std::uint32_t i=0;i<n;++i)for(int field=0;field<4;++field)string_length(); }
    std::size_t fault_state() { const auto n=read32(bytes,at); if (!n) return at; string_length(); at += 2; return at; }
};
std::size_t fault_state_offset(const std::vector<std::uint8_t>& bytes) {
    ImageCursor cursor(bytes); cursor.modules(); cursor.processes(); cursor.processes(); cursor.pending(); return cursor.fault_state();
}
std::vector<std::uint8_t> truncated_payload(const std::vector<std::uint8_t>& image, const std::size_t payload_end) {
    std::vector<std::uint8_t> result(image.begin(), image.begin()+static_cast<std::ptrdiff_t>(payload_end));
    result.resize(result.size()+4); refresh_checksum(result); return result;
}
class FailingOps final : public PersistenceFileOps {
public:
    enum class At { Write, Flush, Close, Replace };
    explicit FailingOps(At value) : fail_at(value) {}
    At fail_at;
    bool write_candidate(const std::string&, const std::vector<std::uint8_t>&) override { return fail_at != At::Write; }
    bool flush_candidate(const std::string&) override { return fail_at != At::Flush; }
    bool close_candidate(const std::string&) override { return fail_at != At::Close; }
    bool replace_candidate(const std::string&, const std::string&) override { return fail_at != At::Replace; }
};
void write_image(const std::string& path, const DurableRecoveryState& state) { const auto bytes=PersistenceStore::serialize(state); std::ofstream out(path,std::ios::binary|std::ios::trunc); out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())); }
}
int run_persistence_store_tests() {
    const DurableRecoveryState expected=complete_state(); DurableRecoveryState loaded;
    const auto image=PersistenceStore::serialize(expected);
    expect(PersistenceStore::deserialize(image,loaded)==PersistenceLoadStatus::Ok && loaded.modules.size()==1 && loaded.modules.front().module_id=="module-a" && loaded.interrupted_processes.size()==1 && loaded.completed_processes.size()==1 && loaded.pending_commands.size()==1 && loaded.non_resettable_faults.size()==1 && loaded.configuration_revision=="configuration-7" && loaded.calibrations.size()==1 && loaded.persisted_operator_mode==OperatorMode::Automatic, "complete durable-state round trip");
    expect(PersistenceStore("persistence-missing.bin").load(loaded)==PersistenceLoadStatus::Missing, "missing image");
    expect(PersistenceStore::deserialize({0,1,2,3},loaded)==PersistenceLoadStatus::Malformed, "malformed magic");
    // Boundaries are found by traversing the actual format.  The fixed-width
    // count is cut in the middle; the variable case ends inside module-id data.
    ImageCursor boundaries(image);
    const std::size_t module_count = boundaries.count();
    const std::size_t module_id_length = boundaries.string_length();
    std::vector<std::uint8_t> fixed_truncation(image.begin(), image.begin()+static_cast<std::ptrdiff_t>(module_count + 2));
    expect(PersistenceStore::deserialize(fixed_truncation, loaded)==PersistenceLoadStatus::Truncated, "truncated fixed-width collection count");
    expect(PersistenceStore::deserialize(truncated_payload(image, module_id_length + 4 + 2), loaded)==PersistenceLoadStatus::Truncated, "truncated variable string payload");
    auto version=image; version[4]=2; version[5]=0; refresh_checksum(version); expect(PersistenceStore::deserialize(version,loaded)==PersistenceLoadStatus::UnsupportedVersion, "unsupported schema");
    auto invalid=image; invalid[fault_state_offset(invalid)]=9; refresh_checksum(invalid); expect(PersistenceStore::deserialize(invalid,loaded)==PersistenceLoadStatus::Invalid, "invalid semantic enum");
    auto inconsistent=image; inconsistent[fault_state_offset(inconsistent)]=3; refresh_checksum(inconsistent); expect(PersistenceStore::deserialize(inconsistent,loaded)==PersistenceLoadStatus::Inconsistent, "inconsistent resettable fault");
    auto trailing=image; trailing.insert(trailing.end()-4, 0); refresh_checksum(trailing); expect(PersistenceStore::deserialize(trailing,loaded)==PersistenceLoadStatus::Malformed, "trailing bytes");
    auto oversized_count=image; ImageCursor count_cursor(oversized_count); put32(oversized_count, count_cursor.count(), PersistenceStore::MaxCollectionCount + 1); refresh_checksum(oversized_count);
    expect(PersistenceStore::deserialize(oversized_count, loaded)==PersistenceLoadStatus::Invalid, "bounded collection count");
    auto oversized_string=image; ImageCursor string_cursor(oversized_string); string_cursor.count(); put32(oversized_string, string_cursor.string_length(), PersistenceStore::MaxStringLength + 1); refresh_checksum(oversized_string);
    expect(PersistenceStore::deserialize(oversized_string, loaded)==PersistenceLoadStatus::Invalid, "bounded string length");
    const std::string io_directory="persistence-store-io-directory";
    std::filesystem::create_directory(io_directory);
    expect(PersistenceStore(io_directory).load(loaded)==PersistenceLoadStatus::IoError, "directory load is an I/O error");
    std::filesystem::remove(io_directory);
    const std::string path="persistence-store-test.bin"; write_image(path, expected);
    for (const auto point : {FailingOps::At::Write, FailingOps::At::Flush, FailingOps::At::Close, FailingOps::At::Replace}) { FailingOps ops{point}; PersistenceStore store(path,&ops); expect(store.save(complete_state())!=PersistenceSaveStatus::Ok, "candidate failure retains accepted image"); DurableRecoveryState retained; expect(PersistenceStore(path).load(retained)==PersistenceLoadStatus::Ok && retained.revision==7, "accepted image preserved"); }
    DurableRecoveryState replacement=expected; replacement.revision=8; expect(PersistenceStore(path).save(replacement)==PersistenceSaveStatus::Ok, "existing accepted image replaced"); expect(PersistenceStore(path).load(loaded)==PersistenceLoadStatus::Ok && loaded.revision==8, "replacement readable");
    std::remove(path.c_str()); std::remove((path+".candidate").c_str());
    return failures;
}
