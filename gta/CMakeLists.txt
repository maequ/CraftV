# CraftV for GTA V: the Script Hook V plugin on top of the shared host core (host/).
#   CraftV.asi   built only when the Script Hook V SDK is present (not in git: get it from
#                http://www.dev-c.com/gtav/scripthookv/ and unzip it to sdk/ScriptHookV_SDK)

set(CRAFTV_GTA_SDK "${CMAKE_SOURCE_DIR}/sdk/ScriptHookV_SDK" CACHE PATH "Script Hook V SDK folder (with inc/ and lib/)")
if(EXISTS "${CRAFTV_GTA_SDK}/inc/natives.h" AND EXISTS "${CRAFTV_GTA_SDK}/lib/ScriptHookV.lib")
	add_library(CraftV SHARED src/main.cpp src/gta_game.cpp)
	set_target_properties(CraftV PROPERTIES SUFFIX ".asi" OUTPUT_NAME "CraftV")
	target_include_directories(CraftV PRIVATE "${CRAFTV_GTA_SDK}/inc")
	target_link_libraries(CraftV PRIVATE craftv_host_core "${CRAFTV_GTA_SDK}/lib/ScriptHookV.lib")
	add_custom_command(TARGET CraftV POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/CraftV.ini" "$<TARGET_FILE_DIR:CraftV>/CraftV.ini")
	message(STATUS "Script Hook V SDK found: building CraftV.asi")
else()
	message(STATUS "Script Hook V SDK not found at ${CRAFTV_GTA_SDK}: CraftV.asi will not be built (tests still are)")
endif()
