#include "automation_core/Persistence/PersistenceStore.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#endif

namespace automation_core {
namespace {
constexpr std::uint8_t Magic[] = {'A','C','D','R'};

std::uint32_t checksum(const std::vector<std::uint8_t>& data, std::size_t end) {
    std::uint32_t h = 2166136261u;
    for (std::size_t i=0; i<end; ++i) { h ^= data[i]; h *= 16777619u; }
    return h;
}
void u8(std::vector<std::uint8_t>& b, std::uint8_t v) { b.push_back(v); }
void u16(std::vector<std::uint8_t>& b, std::uint16_t v) { u8(b, v&255); u8(b, v>>8); }
void u32(std::vector<std::uint8_t>& b, std::uint32_t v) { for (int i=0;i<4;++i) u8(b, static_cast<std::uint8_t>(v>>(8*i))); }
void text(std::vector<std::uint8_t>& b, const std::string& s) { u32(b,static_cast<std::uint32_t>(s.size())); b.insert(b.end(),s.begin(),s.end()); }

class Reader {
public:
    explicit Reader(const std::vector<std::uint8_t>& data): d(data) {}
    bool byte(std::uint8_t& x) { if (p>=d.size()) return false; x=d[p++]; return true; }
    bool word(std::uint16_t& x) { std::uint8_t a,b; if(!byte(a)||!byte(b)) return false; x=static_cast<std::uint16_t>(a|(b<<8)); return true; }
    bool number(std::uint32_t& x) { std::uint8_t a,b,c,e; if(!byte(a)||!byte(b)||!byte(c)||!byte(e)) return false; x=static_cast<std::uint32_t>(a)|(static_cast<std::uint32_t>(b)<<8)|(static_cast<std::uint32_t>(c)<<16)|(static_cast<std::uint32_t>(e)<<24); return true; }
    PersistenceLoadStatus string(std::string& x) { std::uint32_t n; if(!number(n)) return PersistenceLoadStatus::Truncated; if(n>PersistenceStore::MaxStringLength) return PersistenceLoadStatus::Invalid; if(n>d.size()-p) return PersistenceLoadStatus::Truncated; x.assign(reinterpret_cast<const char*>(d.data()+p),n); p+=n; return PersistenceLoadStatus::Ok; }
    std::size_t pos() const{return p;}
private: const std::vector<std::uint8_t>& d; std::size_t p{};
};
PersistenceLoadStatus count(Reader& r, std::uint32_t& n) { if(!r.number(n)) return PersistenceLoadStatus::Truncated; return n>PersistenceStore::MaxCollectionCount ? PersistenceLoadStatus::Invalid : PersistenceLoadStatus::Ok; }

class NativeOps final : public PersistenceFileOps {
public:
 bool write_candidate(const std::string& p, const std::vector<std::uint8_t>& d) override {
     candidate_.open(p, std::ios::binary | std::ios::trunc);
     if (!candidate_) return false;
     candidate_.write(reinterpret_cast<const char*>(d.data()), static_cast<std::streamsize>(d.size()));
     return static_cast<bool>(candidate_);
 }
 bool flush_candidate(const std::string&) override {
     candidate_.flush();
     return static_cast<bool>(candidate_);
 }
 bool close_candidate(const std::string&) override {
     candidate_.close();
     return !candidate_.fail();
 }
 bool replace_candidate(const std::string& c, const std::string& p) override {
#ifdef _WIN32
     // filesystem::rename does not replace a destination on Windows. MoveFileEx
     // provides one atomic replacement operation and never deletes the accepted
     // destination before the candidate can replace it.
     return ::MoveFileExA(c.c_str(), p.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
     std::error_code ec;
     std::filesystem::rename(c, p, ec);
     return !ec;
#endif
 }
private:
 std::ofstream candidate_;
};
}

PersistenceStore::PersistenceStore(std::string path, PersistenceFileOps* ops):path_(std::move(path)),file_ops_(ops) {}

std::vector<std::uint8_t> PersistenceStore::serialize(const DurableRecoveryState& s) {
 std::vector<std::uint8_t> b; b.insert(b.end(),std::begin(Magic),std::end(Magic)); u16(b,SchemaVersion); u32(b,s.revision);
 auto modules=[&]{u32(b,static_cast<std::uint32_t>(s.modules.size()));for(auto& x:s.modules){text(b,x.module_id);text(b,x.session_id);}}; modules();
 auto processes=[&](const std::vector<ProcessEvidence>& v){u32(b,static_cast<std::uint32_t>(v.size()));for(auto& x:v){text(b,x.process_id);text(b,x.run_id);text(b,x.outcome);}}; processes(s.interrupted_processes); processes(s.completed_processes);
 u32(b,static_cast<std::uint32_t>(s.pending_commands.size()));for(auto& x:s.pending_commands){text(b,x.transaction_id);text(b,x.module_id);text(b,x.session_id);text(b,x.command);}
 u32(b,static_cast<std::uint32_t>(s.non_resettable_faults.size()));for(auto& x:s.non_resettable_faults){text(b,x.fault_id);u8(b,x.classification);u8(b,x.source);u8(b,x.state);}
 text(b,s.configuration_revision);u32(b,static_cast<std::uint32_t>(s.calibrations.size()));for(auto& x:s.calibrations){text(b,x.calibration_id);text(b,x.revision);}u8(b,static_cast<std::uint8_t>(s.persisted_operator_mode));u32(b,checksum(b,b.size()));return b;
}

PersistenceLoadStatus PersistenceStore::deserialize(const std::vector<std::uint8_t>& d, DurableRecoveryState& out) {
 if(d.size()<4) return PersistenceLoadStatus::Truncated;
 for(unsigned i=0;i<4;++i)if(d[i]!=Magic[i])return PersistenceLoadStatus::Malformed;
 if(d.size()<10) return PersistenceLoadStatus::Truncated;
 if(d.size()<14) return PersistenceLoadStatus::Truncated; // fixed header plus checksum
 const std::size_t payload_end=d.size()-4; std::uint32_t saved=static_cast<std::uint32_t>(d[payload_end])|(static_cast<std::uint32_t>(d[payload_end+1])<<8)|(static_cast<std::uint32_t>(d[payload_end+2])<<16)|(static_cast<std::uint32_t>(d[payload_end+3])<<24);
 if(checksum(d,payload_end)!=saved)return PersistenceLoadStatus::Invalid;
 Reader r(d); std::uint8_t m;for(int i=0;i<4;++i)r.byte(m);std::uint16_t version;if(!r.word(version))return PersistenceLoadStatus::Truncated;if(version!=SchemaVersion)return PersistenceLoadStatus::UnsupportedVersion;
 DurableRecoveryState x;if(!r.number(x.revision))return PersistenceLoadStatus::Truncated; std::uint32_t n; PersistenceLoadStatus st;
 if((st=count(r,n))!=PersistenceLoadStatus::Ok)return st; for(std::uint32_t i=0;i<n;++i){ModuleEvidence e;if((st=r.string(e.module_id))!=PersistenceLoadStatus::Ok)return st;if((st=r.string(e.session_id))!=PersistenceLoadStatus::Ok)return st;x.modules.push_back(std::move(e));}
 auto read_processes=[&](std::vector<ProcessEvidence>& v)->PersistenceLoadStatus {std::uint32_t c;if(auto z=count(r,c);z!=PersistenceLoadStatus::Ok)return z;for(std::uint32_t i=0;i<c;++i){ProcessEvidence e;for(std::string* q:{&e.process_id,&e.run_id,&e.outcome})if(auto z=r.string(*q);z!=PersistenceLoadStatus::Ok)return z;v.push_back(std::move(e));}return PersistenceLoadStatus::Ok;};
 if((st=read_processes(x.interrupted_processes))!=PersistenceLoadStatus::Ok)return st;if((st=read_processes(x.completed_processes))!=PersistenceLoadStatus::Ok)return st;
 if((st=count(r,n))!=PersistenceLoadStatus::Ok)return st;for(std::uint32_t i=0;i<n;++i){PendingCommandEvidence e;for(std::string* q:{&e.transaction_id,&e.module_id,&e.session_id,&e.command})if((st=r.string(*q))!=PersistenceLoadStatus::Ok)return st;x.pending_commands.push_back(std::move(e));}
 if((st=count(r,n))!=PersistenceLoadStatus::Ok)return st;for(std::uint32_t i=0;i<n;++i){FaultEvidence e;if((st=r.string(e.fault_id))!=PersistenceLoadStatus::Ok)return st;if(!r.byte(e.classification)||!r.byte(e.source)||!r.byte(e.state))return PersistenceLoadStatus::Truncated;if(e.classification>1||e.source>4||e.state>3)return PersistenceLoadStatus::Invalid;if(e.state==3)return PersistenceLoadStatus::Inconsistent;x.non_resettable_faults.push_back(std::move(e));}
 if((st=r.string(x.configuration_revision))!=PersistenceLoadStatus::Ok)return st;if((st=count(r,n))!=PersistenceLoadStatus::Ok)return st;for(std::uint32_t i=0;i<n;++i){CalibrationMetadata e;if((st=r.string(e.calibration_id))!=PersistenceLoadStatus::Ok)return st;if((st=r.string(e.revision))!=PersistenceLoadStatus::Ok)return st;x.calibrations.push_back(std::move(e));}std::uint8_t mode;if(!r.byte(mode))return PersistenceLoadStatus::Truncated;if(mode>1)return PersistenceLoadStatus::Invalid;x.persisted_operator_mode=static_cast<OperatorMode>(mode);
 if(r.pos()!=payload_end)return PersistenceLoadStatus::Malformed;
 std::set<std::string> ids;for(const auto& e:x.modules)if(e.module_id.empty()||e.session_id.empty()||!ids.insert(e.module_id).second)return PersistenceLoadStatus::Inconsistent;for(const auto& e:x.interrupted_processes)if(e.process_id.empty()||e.run_id.empty())return PersistenceLoadStatus::Inconsistent;
 out=std::move(x);return PersistenceLoadStatus::Ok;
}

PersistenceLoadStatus PersistenceStore::load(DurableRecoveryState& out) const { std::ifstream f(path_,std::ios::binary);if(!f){std::error_code e;if(!std::filesystem::exists(path_,e)&&!e)return PersistenceLoadStatus::Missing;return PersistenceLoadStatus::IoError;}std::vector<std::uint8_t>d((std::istreambuf_iterator<char>(f)),{});if(f.bad())return PersistenceLoadStatus::IoError;return deserialize(d,out); }
PersistenceSaveStatus PersistenceStore::save(const DurableRecoveryState& s) const { auto image=serialize(s); DurableRecoveryState checked;if(deserialize(image,checked)!=PersistenceLoadStatus::Ok)return PersistenceSaveStatus::IoError; NativeOps native;auto& ops=file_ops_?*file_ops_:native;const std::string candidate=path_+".candidate";if(!ops.write_candidate(candidate,image))return PersistenceSaveStatus::WriteError;if(!ops.flush_candidate(candidate)){std::remove(candidate.c_str());return PersistenceSaveStatus::FlushError;}if(!ops.close_candidate(candidate)){std::remove(candidate.c_str());return PersistenceSaveStatus::CloseError;}if(!ops.replace_candidate(candidate,path_)){std::remove(candidate.c_str());return PersistenceSaveStatus::ReplaceError;}return PersistenceSaveStatus::Ok; }
} // namespace automation_core
