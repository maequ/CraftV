# mockhost: a stand-in for the RDR2 plugin (Phase 1). Uses the same redcraft_link library the ASI will.
add_executable(mockhost main.cpp)
target_link_libraries(mockhost PRIVATE redcraft_link winmm)
