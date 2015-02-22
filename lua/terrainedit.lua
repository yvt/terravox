module(..., package.seeall)

require "terravox.host"
local ffi = require("ffi")
local host = terravox.host.api

require "lclass"
require "terravox.terrain"

class "TerrainEdit"

function TerrainEdit:TerrainEdit(handle) -- note: newFromHandle doesn't do ffi.gc!
  self.handle = handle
  self.terrain = terravox.terrain.Terrain(host.terrainEditGetTerrain(handle))
  return self
end

function TerrainEdit:beginEdit(x, y, width, height)
  host.terrainEditBeginEdit(self.handle, x, y, width, height)
end 
function TerrainEdit:endEdit()
  host.terrainEditEndEdit(self.handle)
end 