# v2.9 Promoter fix

Hardware error fixed: 0x80022005.

VitaSDK identifies 0x80022005 as SCE_KERNEL_ERROR_INVALID_MEMORY_ACCESS.

Cause in the installer:
The PAF internal sysmodule loader was called with NULL for its option/result
buffer. VitaShell's working implementation supplies a four-word result buffer.

v2.9:
- uses the VitaShell-style PAF load result buffer
- uses the matching unload result buffer
- keeps current VitaSDK function signatures via explicit SceSysmoduleOpt casts
- cleans up PromoterUtil only after successful initialization
- preserves the v2.8 clean UI and MRW-PKG installer
