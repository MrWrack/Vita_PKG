# v3.7 Build Fix

Fixes GitHub Actions compile errors:
- `static declaration of 'ui_crown' follows non-static declaration`
- `static declaration of 'ui_github_octocat_crowned' follows non-static declaration`
- `static declaration of 'ui_mrwrack_logo' follows non-static declaration`

Cause:
Those helper functions were called before the compiler had seen their `static`
definitions, so C created implicit non-static declarations.

Fix:
Added explicit static forward declarations before the first call.

All v3.6 UI changes are retained:
- crowned Octocat-style logo
- crowned MRWRACK branding
- rebuilt six menu icons
- cleaned right-side Vita panel
- convert/install loading screens
