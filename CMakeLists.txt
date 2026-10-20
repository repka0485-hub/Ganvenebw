cmake_minimum_required(VERSION 3.22)
project(espmod LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# --- Dobby (static) ---------------------------------------------------------
# Dobby declares an old cmake_minimum_required; CMake >= 4 rejects it without this.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set(DOBBY_DEBUG OFF CACHE BOOL "" FORCE)
set(DOBBY_GENERATE_SHARED OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/Dobby dobby)
if(TARGET dobby_static)
  set(DOBBY_LIB dobby_static)
else()
  set(DOBBY_LIB dobby)
endif()

# --- Dear ImGui: core + OpenGL3 backend only (no platform backend, no input) -
set(IMGUI_DIR third_party/imgui)
add_library(imgui STATIC
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_ES3)

# --- libespmod.so -----------------------------------------------------------
add_library(espmod SHARED src/main.cpp src/esp.cpp)
target_include_directories(espmod PRIVATE src third_party/Dobby/include)
target_compile_options(espmod PRIVATE -Wall -Wextra -ffunction-sections -fdata-sections)
target_link_libraries(espmod PRIVATE imgui ${DOBBY_LIB} log EGL GLESv3 dl)
target_link_options(espmod PRIVATE -Wl,--gc-sections -Wl,--exclude-libs,ALL)
