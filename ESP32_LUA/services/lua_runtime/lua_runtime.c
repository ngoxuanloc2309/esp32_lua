#include "sx_lua_runtime.h"

#include <string.h>

#include "esp_log.h"

#include "lauxlib.h"
#include "lualib.h"

static const char *TAG = "sx_lua_runtime";

bool sx_lua_runtime_init(sx_lua_runtime_t *runtime)
{
    memset(runtime, 0, sizeof(*runtime));

    runtime->L = luaL_newstate();
    if (runtime->L == NULL) {
        ESP_LOGE(TAG, "luaL_newstate() failed (out of memory?)");
        return false;
    }

    luaL_openlibs(runtime->L);
    return true;
}

void sx_lua_runtime_register_function(sx_lua_runtime_t *runtime,
                                       const char *name,
                                       lua_CFunction fn)
{
    if (runtime->L == NULL) {
        ESP_LOGE(TAG, "register_function(\"%s\") called on an uninitialized runtime", name);
        return;
    }
    lua_pushcfunction(runtime->L, fn);
    lua_setglobal(runtime->L, name);
}

bool sx_lua_runtime_run_string(sx_lua_runtime_t *runtime, const char *script)
{
    if (runtime->L == NULL) {
        ESP_LOGE(TAG, "run_string() called on an uninitialized runtime");
        return false;
    }

    int status = luaL_dostring(runtime->L, script);
    if (status != LUA_OK) {
        const char *err_msg = lua_tostring(runtime->L, -1);
        ESP_LOGE(TAG, "Lua script failed: %s",
                 err_msg != NULL ? err_msg : "(no error message)");
        lua_pop(runtime->L, 1); /* remove error message from the stack */
        return false;
    }

    return true;
}

void sx_lua_runtime_deinit(sx_lua_runtime_t *runtime)
{
    if (runtime->L != NULL) {
        lua_close(runtime->L);
        runtime->L = NULL;
    }
}