cmake_minimum_required(VERSION 3.20)
project(lit CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT WIN32)
    message(FATAL_ERROR "lit is a Windows-only external.")
endif()

set(UI_ROOT   "${CMAKE_CURRENT_SOURCE_DIR}/ui/sweetie")
set(EX_ROOT   "${UI_ROOT}/examples/example_win32_directx11")

# ---- ImGui core + backends (from Sweetie, unmodified) --------------------
set(IMGUI_SRC
    "${UI_ROOT}/imgui.cpp"
    "${UI_ROOT}/imgui_draw.cpp"
    "${UI_ROOT}/imgui_tables.cpp"
    "${UI_ROOT}/imgui_widgets.cpp"
    "${UI_ROOT}/imgui_freetype.cpp"
    "${UI_ROOT}/backends/imgui_impl_win32.cpp"
    "${UI_ROOT}/backends/imgui_impl_dx11.cpp"
)

# ---- UI layer (Sweetie shell + our tabs) ---------------------------------
set(UI_SRC
    "${EX_ROOT}/main.cpp"
    "${EX_ROOT}/gui.cpp"
    "${EX_ROOT}/tab_aimbot.cpp"
    "${EX_ROOT}/tab_visuals.cpp"
    "${EX_ROOT}/tab_world.cpp"
    "${EX_ROOT}/tab_settings.cpp"
)

# ---- Roblox layer --------------------------------------------------------
set(LIT_SRC
    "src/core/process.cpp"
    "src/core/memory.cpp"
    "src/core/net.cpp"
    "src/roblox/roblox.cpp"
    "src/roblox/offsets.cpp"
    "src/features/feature_manager.cpp"
)

add_executable(lit WIN32 ${IMGUI_SRC} ${UI_SRC} ${LIT_SRC})

target_include_directories(lit PRIVATE
    "${UI_ROOT}"
    "${UI_ROOT}/backends"
    "${EX_ROOT}"
    "${UI_ROOT}/examples/freetype/include"
    "${UI_ROOT}/examples/nlohmann"
    "src"
)

target_link_directories(lit PRIVATE
    "${UI_ROOT}/examples/freetype/win64"
)

target_link_libraries(lit PRIVATE
    d3d11 dxgi d3dcompiler dwmapi
    freetype
    user32 gdi32 shell32 advapi32 psapi winhttp
)

target_compile_definitions(lit PRIVATE
    UNICODE _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

if(MSVC)
    target_compile_options(lit PRIVATE /MP /W3 /permissive-)
    set_target_properties(lit PROPERTIES
        LINK_FLAGS "/ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWS"
    )
endif()
