# v3.3 Build Fix
Fixes the main.c brace/scope error introduced in v3.2.

GitHub Actions was compiling shutdown statements as if they were outside `main()`,
which caused:
- expected declaration specifiers before '&'
- conflicting types for psvDebugScreenShutdown
- expected identifier before return / '}'

v3.3 normalizes the end of `main()` so the loop closes once and shutdown/save
calls remain inside the function.

Premium v3.2 UI, GitHub/MrWrack branding, background, and convert/install loading
screens are retained.
