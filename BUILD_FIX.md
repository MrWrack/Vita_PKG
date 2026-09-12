# v1.4.3 Build Fix

GitHub Actions error:
- incompatible pointer type in `sceSysmoduleLoadModuleInternalWithArg`
- incompatible pointer type in `sceSysmoduleUnloadModuleInternalWithArg`

Current VitaSDK signature expects:
`const SceSysmoduleOpt *option`

Fix:
- `load_paf()` now passes `NULL` for the option argument
- `unload_paf()` now passes `NULL` for the option argument
- old `uint32_t` option buffer removed

The PAF argument array itself is kept.
