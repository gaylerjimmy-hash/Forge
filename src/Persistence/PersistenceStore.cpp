#include "automation_core/Persistence/PersistenceStore.h"

#include <array>
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
constexpr std::array<unsigned char, 4> magic{{'A','C','D','R'}};
constexpr std::size_t header_size = 4 + 2 + 8;
PersistenceStore::SaveFailure save_failure_for_testing = PersistenceStore::SaveFailure::None;

bool replace_candidate(const std::string& candidate, const std::string& destination) {
#ifdef _WIN32
    const auto source = std::filesystem::path(candidate).wstring();
    const auto target = std::filesystem::path(destination).wstring();
    if (std::filesystem::exists(destination))
        return ReplaceFileW(target.c_str(), source.c_str(), nullptr, 0, nullptr, nullptr) != 0;
    return MoveFileExW(source.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::error_code ec;
    std::filesystem::rename(candidate, destination, ec);
    return !ec;
#endif
}
std::uint32_t crc32(const std::vector<unsigned char>& bytes) { std::uint32_t c=0xffffffffU; for(auto x:bytes){c^=x;for(int n=0;n<8;++n)c=(c>>1)^((c&1U)?0xedb88320U:0U);} return ~c; }
void u16(std::vector<unsigned char>& b,std::uint16_t x){b.push_back(static_cast<unsigned char>(x));b.push_back(static_cast<unsigned char>(x>>8));}
void u32(std::vector<unsigned char>& b,std::uint32_t x){for(int n=0;n<4;++n)b.push_back(static_cast<unsigned char>(x>>(8*n)));}
void u64(std::vector<unsigned char>& b,std::uint64_t x){for(int n=0;n<8;++n)b.push_back(static_cast<unsigned char>(x>>(8*n)));}
void text(std::vector<unsigned char>& b,const std::string& x){u32(b,static_cast<std::uint32_t>(x.size()));b.insert(b.end(),x.begin(),x.end());}
struct Reader { const std::vector<unsigned char>& b; std::size_t p{}; bool truncated{};
 bool byte(unsigned char& x){if(p>=b.size()){truncated=true;return false;}x=b[p++];return true;}
 bool n(std::uint64_t& x,int count){x=0;if(b.size()-p<static_cast<std::size_t>(count)){truncated=true;return false;}for(int i=0;i<count;++i)x|=static_cast<std::uint64_t>(b[p++])<<(8*i);return true;}
 bool str(std::string& x){std::uint64_t z;if(!n(z,4))return false;if(z>PersistenceStore::max_string)return false;if(z>b.size()-p){truncated=true;return false;}x.assign(reinterpret_cast<const char*>(b.data()+p),static_cast<std::size_t>(z));p+=static_cast<std::size_t>(z);return true;}
 bool count(std::uint64_t& x){return n(x,4)&&x<=PersistenceStore::max_count;}
};
bool valid(const DurableState& s,bool consistency) {
 if(static_cast<unsigned>(s.configured_operator_mode)>1||s.configuration_revision.size()>PersistenceStore::max_string||s.modules.size()>PersistenceStore::max_count||s.pending_commands.size()>PersistenceStore::max_count||s.non_resettable_faults.size()>PersistenceStore::max_count||s.calibration.size()>PersistenceStore::max_count)return false;
 std::set<std::string> mids,cids,fids,keys;
 for(const auto& x:s.modules)if(x.module_id.empty()||x.session_id.empty()||x.module_id.size()>PersistenceStore::max_string||x.session_id.size()>PersistenceStore::max_string||!mids.insert(x.module_id).second)return false;
 for(const auto& x:s.pending_commands)if(x.transaction_id.empty()||x.module_id.empty()||x.session_id.empty()||x.transaction_id.size()>PersistenceStore::max_string||x.module_id.size()>PersistenceStore::max_string||x.session_id.size()>PersistenceStore::max_string||!cids.insert(x.transaction_id).second)return false;
 for(const auto& x:s.non_resettable_faults)if(x.fault_id.empty()||x.fault_id.size()>PersistenceStore::max_string||x.classification>1||x.source>4||x.state>3||!fids.insert(x.fault_id).second)return false;
 for(const auto& x:s.calibration)if(x.key.empty()||x.key.size()>PersistenceStore::max_string||x.metadata.size()>PersistenceStore::max_string||!keys.insert(x.key).second)return false;
 auto proc=[](const ProcessEvidence& x){return !x.process_id.empty()&&!x.run_id.empty()&&x.process_id.size()<=PersistenceStore::max_string&&x.run_id.size()<=PersistenceStore::max_string;};
 return (!s.has_interrupted_process||proc(s.interrupted_process))&&(!s.has_last_completed_process||proc(s.last_completed_process))&&(!consistency||!s.has_interrupted_process||!s.has_last_completed_process||s.interrupted_process.run_id!=s.last_completed_process.run_id);
}
void process(std::vector<unsigned char>& b,const ProcessEvidence& p){text(b,p.process_id);text(b,p.run_id);}
}
PersistenceStore::PersistenceStore(std::string path):path_(std::move(path)){}
const std::string& PersistenceStore::path() const noexcept{return path_;}
void PersistenceStore::set_save_failure_for_testing(SaveFailure failure) noexcept { save_failure_for_testing=failure; }

bool PersistenceStore::save(const DurableState& s) const {
 if(!valid(s,true))return false;
 std::vector<unsigned char>b;b.insert(b.end(),magic.begin(),magic.end());u16(b,schema_version);u64(b,s.revision);
 u32(b,static_cast<std::uint32_t>(s.modules.size()));for(const auto&x:s.modules){text(b,x.module_id);text(b,x.session_id);} b.push_back(s.has_interrupted_process);if(s.has_interrupted_process)process(b,s.interrupted_process);b.push_back(s.has_last_completed_process);if(s.has_last_completed_process)process(b,s.last_completed_process);
 u32(b,static_cast<std::uint32_t>(s.pending_commands.size()));for(const auto&x:s.pending_commands){text(b,x.transaction_id);text(b,x.module_id);text(b,x.session_id);}u32(b,static_cast<std::uint32_t>(s.non_resettable_faults.size()));for(const auto&x:s.non_resettable_faults){text(b,x.fault_id);b.push_back(x.classification);b.push_back(x.source);b.push_back(x.state);}text(b,s.configuration_revision);u32(b,static_cast<std::uint32_t>(s.calibration.size()));for(const auto&x:s.calibration){text(b,x.key);text(b,x.metadata);}b.push_back(static_cast<unsigned char>(s.configured_operator_mode));u32(b,crc32(b));
 const std::string candidate=path_+".candidate";const SaveFailure injected=std::exchange(save_failure_for_testing,SaveFailure::None);
 {std::ofstream out(candidate,std::ios::binary|std::ios::trunc);if(!out)return false;if(injected==SaveFailure::Write){out.close();std::remove(candidate.c_str());return false;}out.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()));if(!out||injected==SaveFailure::Flush){out.close();std::remove(candidate.c_str());return false;}out.flush();if(!out){out.close();std::remove(candidate.c_str());return false;}out.close();if(!out||injected==SaveFailure::Close){std::remove(candidate.c_str());return false;}}
 if(injected==SaveFailure::Replace||!replace_candidate(candidate,path_)){std::remove(candidate.c_str());return false;}return true;
}

LoadStatus PersistenceStore::load(DurableState& destination) const {
 std::ifstream in(path_,std::ios::binary);if(!in)return std::filesystem::exists(path_)?LoadStatus::IoError:LoadStatus::Missing;std::vector<unsigned char>raw((std::istreambuf_iterator<char>(in)),{});if(in.bad())return LoadStatus::IoError;if(raw.size()<header_size)return LoadStatus::Truncated;for(std::size_t i=0;i<magic.size();++i)if(raw[i]!=magic[i])return LoadStatus::Malformed;
 Reader h{raw,4};std::uint64_t version,revision;if(!h.n(version,2)||!h.n(revision,8))return LoadStatus::Truncated;if(version!=schema_version)return LoadStatus::UnsupportedVersion;if(raw.size()<header_size+4)return LoadStatus::Truncated;std::uint32_t actual=static_cast<std::uint32_t>(raw[raw.size()-4])|(static_cast<std::uint32_t>(raw[raw.size()-3])<<8)|(static_cast<std::uint32_t>(raw[raw.size()-2])<<16)|(static_cast<std::uint32_t>(raw[raw.size()-1])<<24);raw.resize(raw.size()-4);if(crc32(raw)!=actual)return LoadStatus::Malformed;
 Reader r{raw,header_size};DurableState s;s.revision=revision;std::uint64_t n;unsigned char flag,e;if(!r.count(n))return r.truncated?LoadStatus::Truncated:LoadStatus::Invalid;for(;n--;){ModuleEvidence x;if(!r.str(x.module_id)||!r.str(x.session_id))return r.truncated?LoadStatus::Truncated:LoadStatus::Invalid;s.modules.push_back(std::move(x));}if(!r.byte(flag))return LoadStatus::Truncated;if(flag>1)return LoadStatus::Invalid;s.has_interrupted_process=flag;if(flag&&(!r.str(s.interrupted_process.process_id)||!r.str(s.interrupted_process.run_id)))return r.truncated?LoadStatus::Truncated:LoadStatus::Invalid;if(!r.byte(flag))return LoadStatus::Truncated;if(flag>1)return LoadStatus::Invalid;s.has_last_completed_process=flag;if(flag&&(!r.str(s.last_completed_process.process_id)||!r.str(s.last_completed_process.run_id)))return r.truncated?LoadStatus::Truncated:LoadStatus::Invalid;
 if(!r.count(n))return r.truncated?LoadStatus::Truncated:LoadStatus::Invalid;for(;n--;){PendingCommandEvidence x;if(!r.str(x.transaction_id)||!r.str(x.module_id)||!r.str(x.session_id))return r.truncated?LoadStatus::Truncated:LoadStatus::Invalid;s.pending_commands.push_back(std::move(x));}if(!r.count(n))return r.truncated?LoadStatus::Truncated:LoadStatus::Invalid;for(;n--;){SupervisoryFaultEvidence x;if(!r.str(x.fault_id)||!r.byte(x.classification)||!r.byte(x.source)||!r.byte(x.state))return r.truncated?LoadStatus::Truncated:LoadStatus::Invalid;s.non_resettable_faults.push_back(std::move(x));}if(!r.str(s.configuration_revision)||!r.count(n))return r.truncated?LoadStatus::Truncated:LoadStatus::Invalid;for(;n--;){CalibrationEvidence x;if(!r.str(x.key)||!r.str(x.metadata))return r.truncated?LoadStatus::Truncated:LoadStatus::Invalid;s.calibration.push_back(std::move(x));}if(!r.byte(e))return LoadStatus::Truncated;s.configured_operator_mode=static_cast<OperatorMode>(e);if(r.p!=raw.size()||e>1)return LoadStatus::Invalid;for(const auto&f:s.non_resettable_faults)if(f.classification>1||f.source>4||f.state>3)return LoadStatus::Invalid;if(!valid(s,true))return LoadStatus::Inconsistent;destination=std::move(s);return LoadStatus::Ok;
}
} // namespace automation_core
