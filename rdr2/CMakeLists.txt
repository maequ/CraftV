# RedCraft for RDR2: the host plugin.
#   redcraft_host_core  game-agnostic plugin logic (link, story-mode gate, coords, overlay, config)
#   redcraft_rdr2_tests tests of the core against a fake game (no RDR2 needed)
#   RedCraft.asi        the ScriptHookRDR2 plugin; built only when the SDK is present (not in git:
#                       its readme forbids redistribution; get it from https://www.dev-c.com/rdr2/scripthookrdr2/)

add_library(redcraft_host_core STATIC
	src/core/config.cpp
	src/core/host_log.cpp
	src/core/host_plugin.cpp
)
target_include_directories(redcraft_host_core PUBLIC src)
target_link_libraries(redcraft_host_core PUBLIC redcraft_link)

add_executable(redcraft_rdr2_tests
	tests/test_host_plugin.cpp
	${CMAKE_SOURCE_DIR}/protocol/cpp/tests/test_main.cpp
)
target_include_directories(redcraft_rdr2_tests PRIVATE ${CMAKE_SOURCE_DIR}/protocol/cpp/tests)
target_link_libraries(redcraft_rdr2_tests PRIVATE redcraft_host_core)
add_test(NAME redcraft_rdr2_tests COMMAND redcraft_rdr2_tests)

set(REDCRAFT_RDR2_SDK "${CMAKE_SOURCE_DIR}/sdk/ScriptHookRDR2_SDK" CACHE PATH "ScriptHookRDR2 SDK folder (with inc/ and lib/)")
if(EXISTS "${REDCRAFT_RDR2_SDK}/inc/natives.h" AND EXISTS "${REDCRAFT_RDR2_SDK}/lib/ScriptHookRDR2.lib")
	add_library(RedCraft SHARED src/asi/main.cpp src/asi/rdr2_game.cpp)
	set_target_properties(RedCraft PROPERTIES SUFFIX ".asi" OUTPUT_NAME "RedCraft")
	target_include_directories(RedCraft PRIVATE "${REDCRAFT_RDR2_SDK}/inc")
	target_link_libraries(RedCraft PRIVATE redcraft_host_core "${REDCRAFT_RDR2_SDK}/lib/ScriptHookRDR2.lib")
	add_custom_command(TARGET RedCraft POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/RedCraft.ini" "$<TARGET_FILE_DIR:RedCraft>/RedCraft.ini")
	message(STATUS "ScriptHookRDR2 SDK found: building RedCraft.asi")
else()
	message(STATUS "ScriptHookRDR2 SDK not found at ${REDCRAFT_RDR2_SDK}: RedCraft.asi will not be built (tests still are)")
endif()
