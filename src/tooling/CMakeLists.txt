# ─────────────────────────────────────────────────────────────────────────────
#  Nexus Tooling App (Week 1 scaffold)
# ─────────────────────────────────────────────────────────────────────────────

add_executable(nexus_tooling_app
    src/main.cpp
    src/editor/EditorSeams.cpp
)

target_compile_features(nexus_tooling_app PRIVATE cxx_std_23)

target_compile_definitions(nexus_tooling_app
    PRIVATE NEXUS_MODELING_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(nexus_tooling_app
    PRIVATE nexus_gfx_core
)

target_include_directories(nexus_tooling_app
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/src
)
