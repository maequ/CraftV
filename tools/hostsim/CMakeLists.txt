# hostsim: the GTA V plugin core (host/) with a simulated game, for end-to-end tests against real Minecraft.
add_executable(hostsim main.cpp ../mockhost/fake_terrain.cpp)
target_link_libraries(hostsim PRIVATE craftv_host_core winmm)
