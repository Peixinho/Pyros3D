# VisualCheck - visual regression over the DemoLauncher (macOS only: it
# captures the demo's window with CoreGraphics and sizes it with yabai or
# System Events). Built with the demos; it does not link the engine, only
# reads its JSON and stb headers.

add_executable(VisualCheck ${CMAKE_CURRENT_SOURCE_DIR}/VisualCheck.mm)
target_include_directories(VisualCheck PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(VisualCheck PRIVATE PYROS_REPO_DIR="${CMAKE_SOURCE_DIR}")
target_compile_options(VisualCheck PRIVATE -fobjc-arc)
target_link_libraries(VisualCheck PRIVATE "-framework Foundation" "-framework CoreGraphics" "-framework ImageIO")
