# mockhost: a stand-in for the GTA V plugin (Phase 1). Uses the same craftv_link library the ASI will.
add_executable(mockhost main.cpp fake_terrain.cpp)
target_link_libraries(mockhost PRIVATE craftv_link winmm)
