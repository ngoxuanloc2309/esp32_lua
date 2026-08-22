# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# Extra component search paths, beyond the default "components/" dir.
# components/ (default) holds only the raw Lua VM build (lua submodule
# + lua_component wrapper) -- generic, reusable as-is on STM32 later.
# Everything platform/application-specific lives under ESP32_LUA/,
# organized as a layered component -> service -> app dependency chain:
#   ESP32_LUA/components/...   (peripherals: hardware, no Lua knowledge)
#   ESP32_LUA/services/...     (e.g. lua_runtime: VM lifecycle, no HW knowledge)
#   ESP32_LUA/app/user/...     (binds services + components together)
# main/ (app_main orchestrator) requires these components by name, so
# they must be discoverable here or the build fails at component
# resolution time.
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/ESP32_LUA/components"
    "${CMAKE_CURRENT_LIST_DIR}/ESP32_LUA/services"
    "${CMAKE_CURRENT_LIST_DIR}/ESP32_LUA/app"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_lua)