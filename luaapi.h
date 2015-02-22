#ifndef LUAAPI_H
#define LUAAPI_H

#include "lua/api.h"
#include <functional>

class LuaInterface;

struct LuaHost
{
    LuaInterface *interface;
    std::function<bool(std::function<void ()>)> pcall;
};

class LuaScriptInterface
{
    LuaHost host_;
    TerravoxApi api_;
public:
    LuaScriptInterface(LuaInterface *, std::function<bool(std::function<void ()>)> errorProtector);
    TerravoxApi *api() { return &api_; }
};

#endif // LUAAPI_H
