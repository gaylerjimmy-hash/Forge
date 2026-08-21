#include "automation_core/Persistence/PersistenceStore.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
using namespace automation_core;
namespace {
int failures=0; void expect(bool x,const char* m){if(!x){std::cerr<<"Persistence: "<<m<<'\n';++failures;}}
std::vector<unsigned char> read(const std::string& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void write(const std::string&p,const std::vector<unsigned char>&v){std::ofstream f(p,std::ios::binary|std::ios::trunc);f.write(reinterpret_cast<const char*>(v.data()),static_cast<std::streamsize>(v.size()));}
std::uint32_t crc(const std::vector<unsigned char>&v){std::uint32_t c=0xffffffffU;for(auto x:v){c^=x;for(int n=0;n<8;++n)c=(c>>1)^((c&1U)?0xedb88320U:0U);}return ~c;}
std::vector<unsigned char> image_with_crc(std::vector<unsigned char> payload){const auto c=crc(payload);for(int n=0;n<4;++n)payload.push_back(static_cast<unsigned char>(c>>(8*n)));return payload;}
std::vector<unsigned char> slice(const std::vector<unsigned char>& v,size_t n){return std::vector<unsigned char>(v.begin(),v.begin()+n);}
void checksum(std::vector<unsigned char>&v){v=image_with_crc(std::vector<unsigned char>(v.begin(),v.end()-4));}
// This cursor derives every target from the serialized layout; it never scans
// for encoded text or assumes an offset beyond the fixed header.
struct Layout {const std::vector<unsigned char>&v;size_t p=14; size_t u32(){const auto q=p;p+=4;return q;} size_t str(){const auto q=u32();const auto n=static_cast<size_t>(v[q])|(static_cast<size_t>(v[q+1])<<8)|(static_cast<size_t>(v[q+2])<<16)|(static_cast<size_t>(v[q+3])<<24);p+=n;return q;} void skipstr(){static_cast<void>(str());} size_t byte(){return p++;}};
DurableState full(){DurableState s;s.revision=9;s.modules={{"scale-01","81A9C5D2"}};s.has_interrupted_process=true;s.interrupted_process={"fill","run-1"};s.has_last_completed_process=true;s.last_completed_process={"wash","run-2"};s.pending_commands={{"tx-1","scale-01","81A9C5D2"}};s.non_resettable_faults={{"fault-1",1,2,3}};s.configuration_revision="config-9";s.calibration={{"scale","cal-9"}};s.configured_operator_mode=OperatorMode::Automatic;return s;}
bool equal(const DurableState&a,const DurableState&b){return a.revision==b.revision&&a.modules.size()==b.modules.size()&&a.modules[0].module_id==b.modules[0].module_id&&a.modules[0].session_id==b.modules[0].session_id&&a.has_interrupted_process==b.has_interrupted_process&&a.interrupted_process.process_id==b.interrupted_process.process_id&&a.interrupted_process.run_id==b.interrupted_process.run_id&&a.has_last_completed_process==b.has_last_completed_process&&a.last_completed_process.process_id==b.last_completed_process.process_id&&a.last_completed_process.run_id==b.last_completed_process.run_id&&a.pending_commands.size()==b.pending_commands.size()&&a.pending_commands[0].transaction_id==b.pending_commands[0].transaction_id&&a.pending_commands[0].module_id==b.pending_commands[0].module_id&&a.pending_commands[0].session_id==b.pending_commands[0].session_id&&a.non_resettable_faults.size()==b.non_resettable_faults.size()&&a.non_resettable_faults[0].fault_id==b.non_resettable_faults[0].fault_id&&a.non_resettable_faults[0].classification==b.non_resettable_faults[0].classification&&a.non_resettable_faults[0].source==b.non_resettable_faults[0].source&&a.non_resettable_faults[0].state==b.non_resettable_faults[0].state&&a.configuration_revision==b.configuration_revision&&a.calibration.size()==b.calibration.size()&&a.calibration[0].key==b.calibration[0].key&&a.calibration[0].metadata==b.calibration[0].metadata&&a.configured_operator_mode==b.configured_operator_mode;}
void test_store(){const std::string p="persistence-store-test.bin";std::remove(p.c_str());PersistenceStore store(p);DurableState source=full(),out;expect(store.load(out)==LoadStatus::Missing,"Missing");expect(store.save(source),"save complete");expect(store.load(out)==LoadStatus::Ok&&equal(source,out),"complete durable-state round trip");auto image=read(p);
 auto malformed=image;malformed[0]='X';write(p,malformed);expect(store.load(out)==LoadStatus::Malformed,"Malformed");
 auto unsupported=image;unsupported[4]=2;checksum(unsupported);write(p,unsupported);expect(store.load(out)==LoadStatus::UnsupportedVersion,"UnsupportedVersion");
 Layout bounds{image};const auto count=bounds.u32();const auto string_length=bounds.p;bounds.skipstr();const auto string_payload_end=bounds.p;
 write(p,image_with_crc(slice(image,count+2)));expect(store.load(out)==LoadStatus::Truncated,"Truncated fixed field");
 write(p,image_with_crc(slice(image,string_length+2)));expect(store.load(out)==LoadStatus::Truncated,"Truncated string length");
 write(p,image_with_crc(slice(image,string_length+4+2)));expect(store.load(out)==LoadStatus::Truncated,"Truncated string payload");
 auto oversized=image;oversized[count]=1;oversized[count+1]=1;checksum(oversized);write(p,oversized);expect(store.load(out)==LoadStatus::Invalid,"bounded count");
 auto oversized_string=image;for(int n=0;n<4;++n)oversized_string[string_length+n]=0xff;checksum(oversized_string);write(p,oversized_string);expect(store.load(out)==LoadStatus::Invalid,"bounded string");
 auto semantic=image;Layout q{semantic};q.u32();q.skipstr();q.skipstr();q.byte();q.skipstr();q.skipstr();q.byte();q.skipstr();q.skipstr();q.u32();q.skipstr();q.skipstr();q.skipstr();q.u32();q.skipstr();const auto classification=q.byte();semantic[classification]=9;checksum(semantic);write(p,semantic);expect(store.load(out)==LoadStatus::Invalid&&equal(out,source),"checksum-valid semantic enum leaves destination unchanged");
 auto inconsistent=image;Layout r{inconsistent};r.u32();r.skipstr();r.skipstr();r.byte();r.skipstr();const auto interrupted_run_length=r.str();r.byte();r.skipstr();const auto completed_run_length=r.str();const auto run_size=static_cast<size_t>(inconsistent[interrupted_run_length]);for(size_t i=0;i<run_size;++i)inconsistent[completed_run_length+4+i]=inconsistent[interrupted_run_length+4+i];checksum(inconsistent);write(p,inconsistent);expect(store.load(out)==LoadStatus::Inconsistent&&equal(out,source),"Inconsistent leaves every durable category unchanged");
 auto trailing=std::vector<unsigned char>{image.begin(),image.end()-4};trailing.push_back(0);trailing=image_with_crc(std::move(trailing));write(p,trailing);expect(store.load(out)==LoadStatus::Invalid,"trailing bytes");
 PersistenceStore directory_store(".");expect(directory_store.load(out)==LoadStatus::IoError,"IoError");
 // B must replace A normally before failures are injected against that accepted B image.
 expect(store.save(source),"save accepted A");DurableState replacement=source;replacement.revision=10;expect(store.save(replacement),"atomic accepted replacement B");expect(store.load(out)==LoadStatus::Ok&&out.revision==10,"replacement loads B");const auto accepted=read(p);
 for(auto f:{PersistenceStore::SaveFailure::Write,PersistenceStore::SaveFailure::Flush,PersistenceStore::SaveFailure::Close,PersistenceStore::SaveFailure::Replace}){PersistenceStore::set_save_failure_for_testing(f);expect(!store.save(source),"injected save failure");expect(read(p)==accepted,"failure preserved byte-identical accepted image");expect(store.load(out)==LoadStatus::Ok&&out.revision==10,"candidate junk was not accepted");}
 std::remove(p.c_str());}
}
int run_persistence_store_tests(){test_store();return failures;}
