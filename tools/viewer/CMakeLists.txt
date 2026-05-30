cmake_minimum_required(VERSION 3.28)

find_package(SDL2 REQUIRED)

add_executable(nexus_viewer nexus_viewer.cpp)

target_link_libraries(nexus_viewer
    PRIVATE nexus_softrast SDL2::SDL2
)

target_compile_features(nexus_viewer PRIVATE cxx_std_23)

if(NOT MSVC)
    target_compile_options(nexus_viewer PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# CI smoke test: render 5 frames offscreen, exit 0 = success
add_test(
    NAME ViewerOffscreenSmoke
    COMMAND nexus_viewer --offscreen --frames 5
)
set_tests_properties(ViewerOffscreenSmoke PROPERTIES
    LABELS "viewer;smoke"
    TIMEOUT 30
    ENVIRONMENT "SDL_VIDEODRIVER=offscreen"
)
