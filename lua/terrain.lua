module(..., package.seeall)

require "terravox.host"
local ffi = require("ffi")
local host = terravox.host.api

require "lclass"

class "Terrain"

function Terrain.newFromHandle(handle) -- note: newFromHandle doesn't do ffi.gc!
end
function Terrain:Terrain(width, height)
  local handle
  if height then   -- Terrain(width, height)
    handle = ffi.gc(host.terrainCreate(width, height), host.terrainRelease)
  else             -- Terrain(handle)
    handle = width
  end

  local dims = ffi.new("int[2]")
  host.terrainGetSize(handle, dims)

  self.handle = handle
  self.width = dims[0]
  self.height = dims[1]
  self.landform = host.terrainGetLandformData(handle)
  self.color = host.terrainGetColorData(handle)
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
function Terrain:clone()
  return Terrain.newFromHandle(host.terrainClone(self.handle))
end
