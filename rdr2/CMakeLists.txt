# CraftV for RDR2: the host plugin.
#   craftv_host_core  game-agnostic plugin logic (link, story-mode gate, coords, overlay, config)
#   craftv_rdr2_tests tests of the core against a fake game (no RDR2 needed)
#   CraftV_RDR2.asi   the ScriptHookRDR2 plugin; built only when the SDK is present (not in git:
#                       its readme forbids redistribution; get it from https://www.dev-c.com/rdr2/scripthookrdr2/)

add_library(craftv_host_core STATIC
	src/core/config.cpp
	src/core/host_log.cpp
	src/core/host_plugin.cpp
)
target_include_directories(craftv_host_core PUBLIC src)
target_link_libraries(craftv_host_core PUBLIC craftv_link)

add_executable(craftv_rdr2_tests
	tests/test_host_plugin.cpp
	${CMAKE_SOURCE_DIR}/protocol/cpp/tests/test_main.cpp
)
target_include_directories(craftv_rdr2_tests PRIVATE ${CMAKE_SOURCE_DIR}/protocol/cpp/tests)
target_link_libraries(craftv_rdr2_tests PRIVATE craftv_host_core)
add_test(NAME craftv_rdr2_tests COMMAND craftv_rdr2_tests)

set(CRAFTV_RDR2_SDK "${CMAKE_SOURCE_DIR}/sdk/ScriptHookRDR2_SDK" CACHE PATH "ScriptHookRDR2 SDK folder (with inc/ and lib/)")
if(EXISTS "${CRAFTV_RDR2_SDK}/inc/natives.h" AND EXISTS "${CRAFTV_RDR2_SDK}/lib/ScriptHookRDR2.lib")
	add_library(CraftV_RDR2 SHARED src/asi/main.cpp src/asi/rdr2_game.cpp)
	set_target_properties(CraftV_RDR2 PROPERTIES SUFFIX ".asi" OUTPUT_NAME "CraftV_RDR2")
	target_include_directories(CraftV_RDR2 PRIVATE "${CRAFTV_RDR2_SDK}/inc")
	target_link_libraries(CraftV_RDR2 PRIVATE craftv_host_core "${CRAFTV_RDR2_SDK}/lib/ScriptHookRDR2.lib")
	add_custom_command(TARGET CraftV_RDR2 POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/CraftV_RDR2.ini" "$<TARGET_FILE_DIR:CraftV_RDR2>/CraftV_RDR2.ini")
	message(STATUS "ScriptHookRDR2 SDK found: building CraftV_RDR2.asi")
else()
	message(STATUS "ScriptHookRDR2 SDK not found at ${CRAFTV_RDR2_SDK}: CraftV_RDR2.asi will not be built (tests still are)")
endif()
