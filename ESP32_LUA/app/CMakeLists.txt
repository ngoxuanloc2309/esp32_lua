# ESP32_LUA/app  ->  ESP-IDF component name: "app"
#
# Top layer of the component -> service -> app chain. Binds services +
# components together into Lua-callable bindings, one subfolder per
# binding module under user/ (gpio_binding_app, ...).
#
# Single CMakeLists.txt for the whole layer: adding a new binding
# folder under user/<name>/ only requires dropping .c/.h files in --
# the GLOB below picks them up automatically, no new CMakeLists.txt
# needed per folder.

file(GLOB_RECURSE APP_SRCS
    "${CMAKE_CURRENT_LIST_DIR}/user/*.c"
)

idf_component_register(
    SRCS ${APP_SRCS}
    INCLUDE_DIRS
        "user/gpio_binding_app"
        # Add new binding include dirs here, e.g. "user/modbus_binding_app"
    REQUIRES components services driver
)