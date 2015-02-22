module(..., package.seeall)

-- Effect Framework

require "terravox.host"
local ffi = require("ffi")
local host = terravox.host.api

require "lclass"
require "terravox.terrainedit"

function registerEffect(name, factory)
  host.registerEffect(host.host, name, function(fxApi, sctrl)
    ctrler = factory()
    sctrl[0] = ctrler:initializeEffectController(fxApi)
  end)
end

class "EffectController"

function EffectController:EffectController()
  self.fxInterface = nil
  self.fxApi = nil
  self.fxEditor = nil
end

function EffectController:apply(terrainEdit) -- abstract
  error("not overriden")
end
function EffectController:createEditor()
  return nil
end
function EffectController:destroyed()
end

function EffectController:preview()
  self.fxApi.preview(self.fxApi.state)
end

function EffectController:initializeEffectController(fxApi)
  self.fxEditor = self:createEditor() -- make sure reference to editor is preserved...
  self.fxApi = fxApi
  self.fxInterface = ffi.new("struct TerravoxEffectController", {
    editor = self.fxEditor and self.fxEditor.handle,
    apply = function(terrainEdit) 
      self:apply(terravox.terrainedit.TerrainEdit(terrainEdit))
    end,
    destroy = function()
      self.fxInterface.apply:free()
      self.fxInterface.destroy:free()
      self:destroyed()
    end
  })
  return self.fxInterface
end
