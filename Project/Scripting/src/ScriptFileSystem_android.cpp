// Android-specific implementation using AAssetManager and app file storage.
// - Includes: AAssetManager usage for reading packaged assets, fallback to app internal storage for writable scripts, and integration notes for AssetManager lifetime.
// - Contains: how to wire into ANativeActivity or engine initialization to get the AAssetManager pointer.
// - Use cases: compiled only for Android build; used by ScriptingRuntime to load scripts from APK or app storage.
