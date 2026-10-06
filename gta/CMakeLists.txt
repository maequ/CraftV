# CraftV for GTA V: the Script Hook V plugin on top of the shared host core (host/).
#   CraftV.asi   built only when the Script Hook V SDK is present (not in git: get it from
#                http://www.dev-c.com/gtav/scripthookv/ and unzip it to sdk/ScriptHookV_SDK)

set(CRAFTV_GTA_SDK "${CMAKE_SOURCE_DIR}/sdk/ScriptHookV_SDK" CACHE PATH "Script Hook V SDK folder (with inc/ and lib/)")
if(EXISTS "${CRAFTV_GTA_SDK}/inc/natives.h" AND EXISTS "${CRAFTV_GTA_SDK}/lib/ScriptHookV.lib")
	add_library(CraftV SHARED src/main.cpp src/gta_game.cpp src/compositor.cpp)
	set_target_properties(CraftV PROPERTIES SUFFIX ".asi" OUTPUT_NAME "CraftV")
	target_include_directories(CraftV PRIVATE "${CRAFTV_GTA_SDK}/inc")
	# The passthrough's compositor is a ReShade add-on: built when ReShade's add-on headers (crosire/reshade v6.8.0,
	# BSD-3-Clause, include/) are in sdk/reshade-src. Without them CraftV.asi works, minus the Minecraft view.
	set(CRAFTV_RESHADE_INCLUDE "${CMAKE_SOURCE_DIR}/sdk/reshade-src/include" CACHE PATH "ReShade add-on API headers (reshade.hpp)")
	if(EXISTS "${CRAFTV_RESHADE_INCLUDE}/reshade.hpp")
		target_include_directories(CraftV PRIVATE "${CRAFTV_RESHADE_INCLUDE}")
		target_compile_definitions(CraftV PRIVATE CRAFTV_HAS_RESHADE=1 NOMINMAX)
		message(STATUS "ReShade add-on headers found: CraftV.asi includes the passthrough compositor")
	else()
		target_compile_definitions(CraftV PRIVATE CRAFTV_HAS_RESHADE=0)
		message(STATUS "ReShade add-on headers not found at ${CRAFTV_RESHADE_INCLUDE}: no passthrough compositor")
	endif()
	target_link_libraries(CraftV PRIVATE craftv_host_core "${CRAFTV_GTA_SDK}/lib/ScriptHookV.lib")
	add_custom_command(TARGET CraftV POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/CraftV.ini" "$<TARGET_FILE_DIR:CraftV>/CraftV.ini"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/shaders/CraftV.fx" "$<TARGET_FILE_DIR:CraftV>/CraftV.fx")
	message(STATUS "Script Hook V SDK found: building CraftV.asi")
else()
	message(STATUS "Script Hook V SDK not found at ${CRAFTV_GTA_SDK}: CraftV.asi will not be built (tests still are)")
endif()
