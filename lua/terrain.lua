module(..., package.seeall)

require "terravox.host"
local ffi = require("ffi")
local host = terravox.host.api

-- TODO: Terrain

Terrain = {}
Terrain.__index = Terrain

setmetatable(Terrain, {
  __call = function (cls, ...) return cls.new(...) end
})

function Terrain.new(width, height)
  local handle = ffi.gc(host.terrainCreate(width, height), host.terrainRelease)
  local self = setmetatable({
    handle = handle,
    width = width,
    height = height,
    landform = host.terrainGetLandformData(handle),
    color = host.terrainGetColorData(handle)
  }, Terrain)
  return self
end

function Terrain:getLandform(x, y)
  return self.landform[x + y * self.width]
end
function Terrain:getColor(x, y) 
  return self.color[x + y * self.width]
end
function Terrain:setLandform(x, y, value)
  self.landform[x + y * self.width] = value
  return value
end
function Terrain:setColor(x, y, color)
  self.color[x + y * self.width] = color
  return value
end
