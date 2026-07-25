#include "automation_core/Frame/FrameAssembler.h"

#include <chrono>

using namespace automation_core;
int run_frame_tests(){
 int f=0;
 FrameAssembler a(64,std::chrono::milliseconds{100});
 if(!a.assemble("c","HELLO\nEND\n").ok()) ++f;
 if(a.assemble("","HELLO\nEND\n").ok()) ++f;
 if(a.assemble("c","HELLO").ok()) ++f;
 if(a.assemble(
        "c",
        "HELLO\nEND\n",
        std::chrono::milliseconds{99}
    ).error != FrameError::None) ++f;
 if(a.assemble(
        "c",
        "HELLO\nEND\n",
        std::chrono::milliseconds{100}
    ).error != FrameError::TimedOut) ++f;
 return f;
}
