Plugin Loader Strategy (Pseudocode)

Goal
- Discover, validate, and load plugins from `plugins/`.

Pseudocode
1. `pluginRoot = ./plugins`
2. `for each dir in listDirs(pluginRoot)`
3.   `if dir == "_template" continue`
4.   `manifestPath = join(dir, "plugin.json")`
5.   `if not exists(manifestPath) continue`
6.   `manifest = readJson(manifestPath)`
7.   `validate(manifest, plugins/plugin.schema.json)`
8.   `if !satisfies(hostVersion, manifest.engines.fieldEngine) skip`
9.   `entryPath = resolve(join(dir, manifest.entry))`
10.  `module = dynamicImport(entryPath)`
11.  `if module.init exists then module.init(context)`
12.  `registerPlugin(manifest.name, module)`

Notes
- `dynamicImport` can be implemented in your host language (e.g., Node.js `import()` / `require`, Python `importlib`, .NET `AssemblyLoadContext`).
- Keep a registry of loaded plugins keyed by `manifest.name`.
- Consider sandboxing or capability checks based on `manifest.capabilities`.

