# ESP32_LUA/components  ->  ESP-IDF component name: "components"
#
# Lowest layer of the component -> service -> app chain. Hardware
# peripheral wrappers (gpio, uart, modbus, ...), one subfolder per
# peripheral under peripherals/. Depends only on ESP-IDF's own drivers
# (driver, esp_driver_gpio, ...); must never depend on "lua_component"
# or anything under services/ or app/.
#
# Single CMakeLists.txt for the whole layer: adding a new peripheral
# folder under peripherals/<name>/ only requires dropping .c/.h files
# in -- the GLOB below picks them up automatically, no new
# CMakeLists.txt needed per folder.

file(GLOB_RECURSE COMPONENTS_SRCS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/peripherals/*.c"
)

idf_component_register(
    SRCS ${COMPONENTS_SRCS}
    INCLUDE_DIRS
        "peripherals/gpio"
        # Add new peripheral include dirs here, e.g. "peripherals/uart"
    REQUIRES driver
)