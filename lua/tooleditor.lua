module(..., package.seeall)

-- ToolEditor
-- Used to build the editor of a tool or effect

require "terravox.host"
local ffi = require("ffi")
local host = terravox.host.api

require "lclass"

class "ToolEditor"
class "Control"

function ToolEditor:ToolEditor(handle) -- note: newFromHandle doesn't do ffi.gc!
  if not handle then
    handle = ffi.gc(host.toolEditorCreate(host.host), host.toolEditorRelease)
  end

  self.handle = handle
  self.callback = nil
  self.controls = {}

  -- Register finalizer to reclaim resources when this is no longer used
  self.finalizer = ffi.cast("void(*)()", function() self:destroyed() end)
  host.toolEditorAddFinalizer(self.handle, self.finalizer)

  -- Setup control event handler
  local function cbfunc(id)
    local ctrl = self.controls[id]
    local cb = ctrl.onchange
    if cb then
      cb(ctrl)
    end
  end
  self.callback = ffi.cast("ToolEditorControlEventHandler", cbfunc)

end

function ToolEditor:destroyed()
  self.callback:free()
  self.finalizer:free()
end

function ToolEditor:addIntegerSlider(text, min, max, log)
  local ctrl = Control(self, host.toolEditorAddIntegerSlider(self.handle,
    self.callback, text, min, max, log))
  self.controls[ctrl.id] = ctrl
  return ctrl
end

function ToolEditor:addRealSlider(text, min, max, digits, log)
  local ctrl = Control(self, host.toolEditorAddRealSlider(self.handle,
    self.callback, text, min, max, digits, log))
  self.controls[ctrl.id] = ctrl
  return ctrl
end

function Control:Control(editor, id)
  self.editor = editor
  self.id = id
  self.onchange = nil
  return self
end
function Control:getValue()
  return host.toolEditorGetValue(self.editor.handle, self.id)
end
function Control:setValue(newValue)
  host.toolEditorSetValue(self.editor.handle, self.id, newValue)
end

