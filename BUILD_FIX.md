# v1.4.4 Workflow Fix

The `Build VPK` step is now succeeding.

The previous failure happened only because `Verify VPK exists` was still checking
an older hardcoded VPK filename.

v1.4.4 fixes this permanently:
- finds the generated `build/*.vpk` dynamically
- verifies that it exists
- stores the actual path in `GITHUB_ENV`
- uploads that exact VPK

Future version-name changes should no longer break Verify/Upload.
