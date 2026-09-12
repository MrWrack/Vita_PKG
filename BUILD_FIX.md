# v1.4.1 Build Fix

GitHub Actions reported:

`implicit declaration of function 'sceIoMkdir'`

Fix:
- added `#include <psp2/io/dirent.h>` to every source file that uses
  `sceIoMkdir`, `sceIoDopen`, `sceIoDread`, `sceIoDclose`, or `sceIoRmdir`.

This is the VitaSDK header that provides the directory I/O declarations.
