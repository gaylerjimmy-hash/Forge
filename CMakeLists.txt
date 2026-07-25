cmake_minimum_required(VERSION 3.16)

project(
    automation_core
    VERSION 0.1.0
    DESCRIPTION "Automation Core discovery protocol reference implementation"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(AUTOMATION_CORE_BUILD_TESTS "Build Automation Core tests" ON)
option(AUTOMATION_CORE_BUILD_EXAMPLES "Build Automation Core examples" ON)

add_library(automation_core
    src/ModuleState.cpp
    src/Connection/ConnectionManager.cpp
    src/Frame/FrameAssembler.cpp
    src/Protocol/Message.cpp
    src/Protocol/Capability.cpp
    src/Protocol/Parser.cpp
    src/Protocol/Validator.cpp
    src/Registry/ModuleRegistry.cpp
    src/Response/ResponseBuilder.cpp
    src/Router/MessageRouter.cpp
    src/Transport/ConsoleTransport.cpp
    src/Transport/SerialTransport.cpp
    src/Core/Core.cpp
    src/Protocol/MessageSerializer.cpp
)

target_include_directories(
    automation_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(automation_core PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(automation_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(automation_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(automation-core-cli apps/core_cli/main.cpp)
target_link_libraries(automation-core-cli PRIVATE automation_core)

if(AUTOMATION_CORE_BUILD_EXAMPLES)
    add_executable(hello-module-example examples/hello_module/main.cpp)
    target_link_libraries(hello-module-example PRIVATE automation_core)
endif()

if(AUTOMATION_CORE_BUILD_TESTS)
    enable_testing()

    add_executable(automation-core-tests
        tests/test_main.cpp
        tests/connection/ConnectionManagerTests.cpp
        tests/core/CoreTests.cpp
        tests/frame/FrameAssemblerTests.cpp
        tests/parser/ParserTests.cpp
        tests/protocol/HeartbeatMessageTests.cpp
        tests/protocol/CapabilityModelTests.cpp
        tests/transport/ConsoleTransportTests.cpp
        tests/validator/ValidatorTests.cpp
        tests/serializer/MessageSerializerTests.cpp
        tests/registry/ModuleRegistryTests.cpp
        tests/router/MessageRouterTests.cpp
    )

    target_link_libraries(automation-core-tests PRIVATE automation_core)
    add_test(NAME automation-core-tests COMMAND automation-core-tests)
endif()
