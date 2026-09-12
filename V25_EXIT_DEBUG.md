# v2.5 Exit Debug Fix

- Log moved to `ux0:/MrWrack-startup.log`
- START is ignored and cannot close the app
- Circle on Home is ignored and cannot close the app
- X / Triangle / Square / Circle presses are logged
- Main-loop heartbeat is logged
- Normal shutdown is logged before process exit

If the app closes again, open `ux0:/MrWrack-startup.log` in VitaShell and send
a photo of the last lines.
