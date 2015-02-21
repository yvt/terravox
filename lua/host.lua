module(..., package.seeall)

local ffi = require("ffi")
require "terravox.api" -- This loads FFI definition

api = ffi.cast("struct TerravoxApi*", __terravoxApi)
