#include "automation_core/Core/Core.h"
#include "automation_core/Transport/ConsoleTransport.h"

#include <iostream>

int main() {
    automation_core::ConsoleTransport transport;
    automation_core::Core core(transport, std::cerr);

    core.poll_once();

    return 0;
}
