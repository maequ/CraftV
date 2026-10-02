# redcraft_link: the C++ side of docs/PROTOCOL.md, shared by the mock host and the RDR2 plugin.
add_library(redcraft_link STATIC
	src/clock.cpp
	src/codec.cpp
	src/endpoint.cpp
	src/mapping.cpp
)
target_include_directories(redcraft_link PUBLIC include)
target_link_libraries(redcraft_link PUBLIC advapi32)

add_executable(redcraft_link_tests
	tests/test_main.cpp
	tests/test_codec.cpp
	tests/test_ring.cpp
	tests/test_mapping.cpp
	tests/test_endpoint.cpp
	tests/test_stress.cpp
)
target_link_libraries(redcraft_link_tests PRIVATE redcraft_link)
target_compile_definitions(redcraft_link_tests PRIVATE
	REDCRAFT_GOLDEN_FILE="${CMAKE_SOURCE_DIR}/protocol/golden/golden_vectors.txt")
add_test(NAME redcraft_link_tests COMMAND redcraft_link_tests)

# Writes protocol/golden/golden_vectors.txt from the documented values (PROTOCOL.md §10).
add_executable(redcraft_golden_gen tests/golden_gen.cpp tests/golden_values.cpp)
target_link_libraries(redcraft_golden_gen PRIVATE redcraft_link)
target_sources(redcraft_link_tests PRIVATE tests/golden_values.cpp)
