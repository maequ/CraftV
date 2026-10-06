# framedump: reads Minecraft's passthrough frame (PROTOCOL.md §11) and writes PNGs, so the export can be checked
# without GTA V.
add_executable(framedump main.cpp)
target_link_libraries(framedump PRIVATE craftv_link)
