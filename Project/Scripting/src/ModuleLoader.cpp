// Implementation of custom require loader and package.loaded management.
// - Includes: resolution algorithm (script root, embedded assets, asset bundles), caching semantics, and optional bytecode loader path.
// - Contains: helper to flush package.loaded safely and to re-require modules during hot reload.
// - Use cases: called whenever a script requires a module or the engine wants to force-reload a module.
