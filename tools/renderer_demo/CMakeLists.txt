# The renderer demo: Erangel's Island on the device layer (WebGPU - Metal here). Standalone for now:
#   cmake -S tools/renderer_demo -B build_r2 -DDAWN_DIR=<a Dawn release>  &&  cmake --build build_r2
cmake_minimum_required(VERSION 3.20)
project(renderer_demo C CXX OBJC OBJCXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Threads REQUIRED)
find_package(Dawn REQUIRED PATHS ${DAWN_DIR}/lib/cmake/Dawn NO_DEFAULT_PATH)
find_package(SDL2 REQUIRED)
set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
add_executable(renderer_demo main.cpp ${ROOT}/src/Pyros3D/Rendering/Device/WebGPUDevice.cpp)
target_include_directories(renderer_demo PRIVATE ${ROOT}/include ${SDL2_INCLUDE_DIRS})
target_link_libraries(renderer_demo PRIVATE dawn::webgpu_dawn ${SDL2_LIBRARIES})
