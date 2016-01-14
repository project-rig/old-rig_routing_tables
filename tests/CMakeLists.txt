include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)

add_executable(test_rig_routing_table_tools
			test_main.cpp
			test_default_routes.cpp
			test_routing_table.cpp
			test_ordered_covering.cpp)

target_link_libraries(test_rig_routing_table_tools gtest)

add_custom_target(run_tests valgrind --leak-check=yes ./test_rig_routing_table_tools DEPENDS test_rig_routing_table_tools)
