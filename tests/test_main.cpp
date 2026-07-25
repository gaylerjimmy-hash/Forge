#include <iostream>

int run_frame_tests();
int run_heartbeat_message_tests();
int run_capability_model_tests();
int run_parser_tests();
int run_validator_tests();
int run_message_router_tests();
int run_connection_manager_tests();
int run_console_transport_tests();
int run_core_tests();
int run_module_registry_tests();
int run_message_serializer_tests();

int main()
{
    const int failures =
	    + run_message_serializer_tests()
        + run_frame_tests()
        + run_heartbeat_message_tests()
        + run_capability_model_tests()
        + run_parser_tests()
        + run_validator_tests()
        + run_connection_manager_tests()
        + run_console_transport_tests()
        + run_core_tests()
        + run_module_registry_tests()
        + run_message_router_tests();

    if (failures != 0)
    {
        std::cerr << failures << " failures\n";
        return 1;
    }

    std::cout << "All tests passed\n";
    return 0;
}
