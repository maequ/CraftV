# The game-agnostic half of the host plugins (GTA V now, the parked RDR2 one too): link, story-mode
# gate, coordinates, terrain scanner, friends, overlay, config. Games plug in through core/game_api.h.
#   craftv_host_core   the library both ASIs link
#   craftv_host_tests  tests of the core against a fake game (no game needed)

add_library(craftv_host_core STATIC
	core/config.cpp
	core/host_log.cpp
	core/host_plugin.cpp
	core/materials.cpp
	core/terrain_scanner.cpp
)
target_include_directories(craftv_host_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(craftv_host_core PUBLIC craftv_link)

add_executable(craftv_host_tests
	tests/test_host_plugin.cpp
	tests/test_terrain_scanner.cpp
	${CMAKE_SOURCE_DIR}/protocol/cpp/tests/test_main.cpp
)
target_include_directories(craftv_host_tests PRIVATE ${CMAKE_SOURCE_DIR}/protocol/cpp/tests)
target_link_libraries(craftv_host_tests PRIVATE craftv_host_core)
add_test(NAME craftv_host_tests COMMAND craftv_host_tests)
