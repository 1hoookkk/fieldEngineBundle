Plugins Overview

- Location: Each plugin lives under `plugins/<plugin-name>/`.
- Manifest: Each plugin must include a `plugin.json` following `plugins/plugin.schema.json`.
- Entry: The engine loads the module at `plugin.json:entry` relative to the plugin root.
- Isolation: Keep dependencies scoped to the plugin folder or your workspace tooling.

Recommended Layout
- `plugins/<name>/plugin.json` — plugin manifest
- `plugins/<name>/src/` — plugin source
- `plugins/<name>/tests/` — tests for the plugin
- `plugins/<name>/config.schema.json` — optional config schema
- `plugins/<name>/README.md` — plugin docs

Versioning & Compatibility
- Use SemVer for `version`.
- Use a SemVer range in `engines.fieldEngine` to declare compatibility with the host.

Discovery Strategy (Host)
- Enumerate all directories under `plugins/` (excluding `_template`).
- Read and validate `plugin.json` against `plugins/plugin.schema.json`.
- Check `engines.fieldEngine` satisfies the host version.
- Resolve and load the `entry` module.

Tips
- Keep side effects in your entry minimal; expose an init/activate function.
- Validate and document your plugin’s configuration.
- Prefer small, focused plugins over monoliths.

