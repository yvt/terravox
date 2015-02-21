----------------------------------------
-- Entrypoint of built-in lua script. --
----------------------------------------

-- Initialize module loader.
table.insert(package.searchers or package.loaders, 1, __qsearcher)

-- Now module loader is ready.

-- Test API
require "terravox.host"
-- terravox.host.api.aboutQt()
