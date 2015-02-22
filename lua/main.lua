----------------------------------------
-- Entrypoint of built-in lua script. --
----------------------------------------

-- Initialize module loader.
table.insert(package.searchers or package.loaders, 2, __qsearcher)

-- Now module loader is ready.
