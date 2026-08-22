#ifndef SX_LUA_RUNTIME_H
#define SX_LUA_RUNTIME_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "lua.h"

/* Middle layer of the component -> service -> app chain.
 *
 * This service owns the Lua VM lifecycle (create/open-libs/close) and
 * script execution/error handling. It deliberately knows nothing about
 * GPIO, Modbus, or any other peripheral -- callers in the app layer
 * register whatever C functions they need via
 * sx_lua_runtime_register_binding() (a thin wrapper over the standard
 * Lua C API) before running a script. This is what lets this service
 * be reused unmodified across different apps/boards: the set of
 * bindings a given firmware exposes to Lua is an app-layer decision,
 * not something this service hardcodes. */

typedef struct sx_lua_runtime sx_lua_runtime_t;

struct sx_lua_runtime {
    lua_State *L;
};

/* Creates a new Lua VM and loads the standard library (print, string,
 * math, ...). Returns true on success. On failure, *runtime is left
 * zero-initialized and safe to pass to sx_lua_runtime_deinit(). */
bool sx_lua_runtime_init(sx_lua_runtime_t *runtime);

/* Registers a single C function as a named global in the runtime's Lua
 * state, e.g. sx_lua_runtime_register_function(rt, "my_func", l_my_func)
 * makes my_func() callable from Lua scripts run on this runtime.
 * app-layer binding modules (see app/user/gpio_binding_app) use this
 * to expose their functions without this service needing to know what
 * those functions do. */
void sx_lua_runtime_register_function(sx_lua_runtime_t *runtime,
                                       const char *name,
                                       lua_CFunction fn);

/* Runs the given Lua source string to completion (equivalent to
 * luaL_dostring, but with this service's own error reporting).
 * Returns true if the script ran without a compile or runtime error.
 * On failure, logs the Lua-provided error message and leaves the VM
 * in a valid, reusable state (the failing script's error is popped
 * off the stack). */
bool sx_lua_runtime_run_string(sx_lua_runtime_t *runtime, const char *script);

/* Closes the Lua VM and releases its memory. Safe to call on a
 * runtime that failed sx_lua_runtime_init() or was never initialized
 * (zero-initialized sx_lua_runtime_t). */
void sx_lua_runtime_deinit(sx_lua_runtime_t *runtime);

#ifdef __cplusplus
}
#endif
#endif /* SX_LUA_RUNTIME_H */