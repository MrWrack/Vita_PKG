# v1.5.1 build fix

GitHub VitaSDK did not contain `<psp2/debugScreen.h>`, so v1.5 failed before compiling main.c.

v1.5.1 removes that unavailable SDK include and keeps the debug-screen API local to the project,
allowing the source to compile without relying on that missing header.

Note: this is a build compatibility fix. The full graphical UI renderer is still the next hardware-facing UI task.
