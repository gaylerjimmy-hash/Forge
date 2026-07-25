#include "automation_core/Frame/FrameAssembler.h"
#include "automation_core/Frame/FrameError.h"
#include <algorithm>
namespace automation_core {
FrameAssembler::FrameAssembler(size_t m,std::chrono::milliseconds t):max_(m),timeout_(t){}
static std::string norm(std::string s){
    std::string o;
    for(size_t i=0;i<s.size();++i){
        if(s[i]=='\r'){ if(i+1<s.size()&&s[i+1]=='\n') ++i; o+='\n';}
        else o+=s[i];
    }
    return o;
}
FrameResult FrameAssembler::assemble(
    const std::string& id,
    const std::string& raw,
    const std::chrono::milliseconds assembly_time
) const{
    if(id.empty()||raw.empty()) return {std::nullopt,FrameError::Empty,"empty"};
    if(raw.size()>max_) return {std::nullopt,FrameError::TooLarge,"too large"};
    if(assembly_time >= timeout_)
        return {std::nullopt,FrameError::TimedOut,"frame assembly timed out"};
    auto n=norm(raw);
    while(!n.empty()&&(n.back()=='\n'||n.back()==' '||n.back()=='\t')) n.pop_back();
    if(!(n=="END"||(n.size()>=4&&n.substr(n.size()-4)=="\nEND")))
        return {std::nullopt,FrameError::MissingTerminator,"missing END"};
    return {Frame{id,n+"\n"},FrameError::None,""};
}
}
