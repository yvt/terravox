module(..., package.seeall)

require "terravox.host"
local ffi = require("ffi")
local host = terravox.host.api

require "json"

groups = {}

local Group = {}
function Group:__index(key)
  local ret = ffi.string(host.settingsGetValue(self.__group, key))
  if ret ~= "" then
    return json.decode(ret)
  else
    return nil
  end
  return self:getValueOrDefault(key, nil)
end
function Group:__newindex(key, value)
  host.settingsSetValue(self.__group, key, json.encode(value))
end

setmetatable(groups, {
  __index = function(self, key)
    local group = { __group = host.settingsOpen(key) };
    setmetatable(group, Group)
    rawset(self, key, group)
    return group
  end,
  __newindex = function() error("newindex now allowed") end
})
