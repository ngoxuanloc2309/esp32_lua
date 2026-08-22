# ESP32_LUA/services  ->  ESP-IDF component name: "services"
#
# Middle layer of the component -> service -> app chain. Generic
# services with no hardware knowledge (lua_runtime, ...), one
# subfolder per service. Depends on lua_component (raw Lua VM build);
# must never depend on anything under components/ or app/.
#
# Single CMakeLists.txt for the whole layer: adding a new service
# folder under services/<name>/ only requires dropping .c/.h files
# in -- the GLOB below picks them up automatically, no new
# CMakeLists.txt needed per folder.

file(GLOB_RECURSE SERVICES_SRCS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/lua_runtime/*.c"
)

idf_component_register(
    SRCS ${SERVICES_SRCS}
    INCLUDE_DIRS
        "lua_runtime"
        # Add new service include dirs here, e.g. "mqtt_runtime"
    REQUIRES lua_component
)