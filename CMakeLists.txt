cmake_minimum_required(VERSION 3.20)
project(tessera CXX C)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.4)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.93.0-docking)
# JSON: shader headers, project files and the data model view
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
# stb_image / stb_image_write: image media and screenshots
FetchContent_Declare(stb GIT_REPOSITORY https://github.com/nothings/stb.git GIT_TAG a7d4c2175630e1ea5b6f8dd00ab96fed0b97b96d)
FetchContent_MakeAvailable(glfw imgui json stb)

find_package(OpenGL REQUIRED)

add_library(glad third_party/glad/src/glad.c)
target_include_directories(glad PUBLIC third_party/glad/include)

add_library(imgui_lib
  ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui_lib PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends ${imgui_SOURCE_DIR}/misc/cpp)
target_link_libraries(imgui_lib PUBLIC glfw OpenGL::GL)

add_library(stb INTERFACE)
target_include_directories(stb SYSTEM INTERFACE ${stb_SOURCE_DIR})

# Everything but main.cpp, shared by the app and the tests.
file(GLOB_RECURSE TESSERA_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.cpp")
list(REMOVE_ITEM TESSERA_SOURCES "${CMAKE_SOURCE_DIR}/src/main.cpp")
add_library(tessera_core STATIC ${TESSERA_SOURCES})
target_include_directories(tessera_core PUBLIC src third_party/stduuid/include)
target_link_libraries(tessera_core PUBLIC imgui_lib glad nlohmann_json::nlohmann_json stb)
if(WIN32)
  # glad includes <windows.h>: keep its min/max macros away from std::min/std::max.
  target_compile_definitions(tessera_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
if(MSVC)
  target_compile_options(tessera_core PUBLIC /W3 /utf-8)
else()
  target_compile_options(tessera_core PUBLIC -Wall -Wextra -Wno-unused-parameter)
endif()

add_executable(tessera src/main.cpp)
target_link_libraries(tessera PRIVATE tessera_core)

# Model tests: no window or GL context needed.
enable_testing()
add_executable(tessera_tests tests/model_tests.cpp)
target_link_libraries(tessera_tests PRIVATE tessera_core)
add_test(NAME model COMMAND tessera_tests)
