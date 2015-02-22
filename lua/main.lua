----------------------------------------
-- Entrypoint of built-in lua script. --
----------------------------------------

-- Initialize module loader.
table.insert(package.searchers or package.loaders, 2, __qsearcher)

-- Now module loader is ready.

-- Test API
require "terravox.host"
-- terravox.host.api.aboutQt()
require "terravox.terrain"
terravox.terrain.Terrain(512, 512)

