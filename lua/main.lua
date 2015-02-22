----------------------------------------
-- Entrypoint of built-in lua script. --
----------------------------------------

-- Initialize module loader.
table.insert(package.searchers or package.loaders, 2, __qsearcher)

-- Now module loader is ready.

-- (Test API)
require "terravox.host"
-- terravox.host.api.aboutQt()
require "terravox.terrain"
-- terravox.terrain.Terrain(512, 512)

require "terravox.effect"
require "terravox.tooleditor"
require "lclass"

class "TestEffect" (terravox.effect.EffectController) 
function TestEffect:TestEffect()
  terravox.effect.EffectController.EffectController(self)
  self.bias = 0
  self.scale = 1
end
function TestEffect:createEditor()
  local editor = terravox.tooleditor.ToolEditor()

  local biasSlider = editor:addIntegerSlider("Bias", -64, 64, false)
  biasSlider:setValue(self.bias)
  biasSlider.onchange = function(sender)
    self.bias = sender:getValue()
    self:preview()
  end

  local scaleSlider = editor:addRealSlider("Scale", 0.004, 2, 2, true)
  scaleSlider:setValue(self.scale)
  scaleSlider.onchange = function(sender)
    self.scale = sender:getValue()
    self:preview()
  end

  return editor
end
function TestEffect:apply(edit)
  local t = edit.terrain
  local bias = self.bias
  local scale = self.scale

  edit:beginEdit(0, 0, t.width, t.height)
  for y = 0, t.height - 1 do
    for x = 0, t.width - 1 do
      t:setLandform(x, y, t:getLandform(x, y) * scale + bias)
    end
  end
  edit:endEdit()

  -- quantization is done automatically
end

terravox.effect.registerEffect("Global Adjustment", TestEffect)
