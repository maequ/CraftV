# mockhost: a stand-in for the RDR2 plugin (Phase 1). Uses the same craftv_link library the ASI will.
add_executable(mockhost main.cpp)
target_link_libraries(mockhost PRIVATE craftv_link winmm)
