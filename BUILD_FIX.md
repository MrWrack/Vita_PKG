# v1.4.2 Build Fix

GitHub Actions error:
`implicit declaration of function 'sceIoMkdir'`

Correct VitaSDK declarations:
- `sceIoMkdir`, `sceIoRmdir`, `sceIoGetstat` -> `<psp2/io/stat.h>`
- `sceIoDopen`, `sceIoDread`, `sceIoDclose` -> `<psp2/io/dirent.h>`
- file open/read/write/remove -> `<psp2/io/fcntl.h>`

v1.4.2 adds the correct `stat.h` include to every affected source file.
