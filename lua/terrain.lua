module(..., package.seeall)

require "terravox.host"
local api = terravox.host.api

-- TODO: Terrain

Terrain = {}
Terrain.__index = Terrain

setmetatable(Terrain, {
	__call = function (cls, ...) return cls.new(...) end
})

function Terrain.new()
	error "not implemented"
	local self = setmetatable({}, Terrain)
	return self
end function

function Terrain:getLandform(x, y) 
  return self.landform[x + y * self.width]
end
function Terrain:getColor(x, y) 
  return self.color[x + y * self.width]
end
function Terrain:setLandform(x, y, value)
  self.landform[x + y * self.width] = value
end
function Terrain:setColor(x, y, color)
  self.color[x + y * self.width] = color
end
