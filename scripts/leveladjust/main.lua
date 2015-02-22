----------------------------------------
-- Script: Level Adjustment Filter    --
----------------------------------------

require "terravox.settings"
require "terravox.effect"
require "terravox.tooleditor"
require "lclass"

class "LevelAdjustmentEffectController" (terravox.effect.EffectController) 
function LevelAdjustmentEffectController:LevelAdjustmentEffectController()
  terravox.effect.EffectController.EffectController(self)
  self.settings = terravox.settings.groups["org.terraworks.leveladjustment"]
  self.bias = self.settings.bias or 0
  self.scale = self.settings.scale or 1
end
function LevelAdjustmentEffectController:createEditor()
  local editor = terravox.tooleditor.ToolEditor()

  local biasSlider = editor:addIntegerSlider("Bias", -64, 64, false)
  biasSlider:setValue(self.bias)
  biasSlider.onchange = function(sender)
    self.bias = sender:getValue()
    self.settings.bias = self.bias
    self:preview()
  end

  local scaleSlider = editor:addRealSlider("Scale", 0.004, 2, 2, true)
  scaleSlider:setValue(self.scale)
  scaleSlider.onchange = function(sender)
    self.scale = sender:getValue()
    self.settings.scale = self.scale
    self:preview()
  end

  return editor
end
function LevelAdjustmentEffectController:apply(edit)
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

terravox.effect.registerEffect("Level Adjustment", LevelAdjustmentEffectController)
