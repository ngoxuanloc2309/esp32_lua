# lua_component
#
# ESP-IDF wrapper component for the Lua VM. This component does not
# contain any Lua source code itself; it only points the build at the
# .c/.h files living in the ../lua git submodule (a clean checkout of
# lua/lua@v5.4.8, kept unmodified). Keeping the submodule untouched
# avoids merge conflicts or accidental loss of local changes on future
# submodule updates.
#
# Only the core files required to embed the Lua VM into a C application
# are built. The following upstream files are intentionally excluded:
#   - lua.c    : entry point of the standalone Lua CLI ("lua script.lua").
#                Not needed because Lua is embedded directly into
#                app_main() rather than run as a separate process.
#   - ltests.c : Lua's internal test suite, used only for developing the
#                Lua language itself.
#   - onelua.c : a convenience amalgamation file (#includes every source
#                file into one translation unit). Not used here so each
#                file can be compiled and debugged independently via
#                CMake.

set(LUA_DIR "${CMAKE_CURRENT_LIST_DIR}/../lua")

set(LUA_SRCS
    "${LUA_DIR}/lapi.c"
    "${LUA_DIR}/lauxlib.c"
    "${LUA_DIR}/lbaselib.c"
    "${LUA_DIR}/lcode.c"
    "${LUA_DIR}/lcorolib.c"
    "${LUA_DIR}/lctype.c"
    "${LUA_DIR}/ldblib.c"
    "${LUA_DIR}/ldebug.c"
    "${LUA_DIR}/ldo.c"
    "${LUA_DIR}/ldump.c"
    "${LUA_DIR}/lfunc.c"
    "${LUA_DIR}/lgc.c"
    "${LUA_DIR}/linit.c"
    "${LUA_DIR}/liolib.c"
    "${LUA_DIR}/llex.c"
    "${LUA_DIR}/lmathlib.c"
    "${LUA_DIR}/lmem.c"
    "${LUA_DIR}/loadlib.c"
    "${LUA_DIR}/lobject.c"
    "${LUA_DIR}/lopcodes.c"
    "${LUA_DIR}/loslib.c"
    "${LUA_DIR}/lparser.c"
    "${LUA_DIR}/lstate.c"
    "${LUA_DIR}/lstring.c"
    "${LUA_DIR}/lstrlib.c"
    "${LUA_DIR}/ltable.c"
    "${LUA_DIR}/ltablib.c"
    "${LUA_DIR}/ltm.c"
    "${LUA_DIR}/lundump.c"
    "${LUA_DIR}/lutf8lib.c"
    "${LUA_DIR}/lvm.c"
    "${LUA_DIR}/lzio.c"
)

idf_component_register(
    SRCS ${LUA_SRCS}
    INCLUDE_DIRS "${LUA_DIR}"
)

# luaconf.h defaults to plain ANSI C when no platform macro
# (LUA_USE_LINUX / LUA_USE_WINDOWS / etc.) is defined, which is exactly
# what allows Lua to be embedded on bare-metal MCU targets (ESP32 now,
# STM32 later). No additional compile definitions are required here;
# we rely on luaconf.h's default configuration.

# Suppress a handful of warnings that originate from upstream Lua source
# (third-party code we do not modify) so they are not promoted to build
# errors by this project's stricter warning configuration.
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-unused-but-set-variable
    -Wno-unused-variable
    -Wno-type-limits
)