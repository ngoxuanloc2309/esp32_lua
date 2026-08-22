# services/lua_runtime
#
# Middle layer of the component -> service -> app chain. Wraps the Lua
# VM lifecycle (create, open standard libs, run, close) and generic
# C-function binding registration. Depends on lua_component (the raw
# Lua VM build), but must never depend on anything under
# components/peripherals or app/ -- this service has no knowledge of
# GPIO or any other specific hardware binding.

idf_component_register(
    SRCS "sx_lua_runtime.c"
    INCLUDE_DIRS "."
    REQUIRES lua_component
)