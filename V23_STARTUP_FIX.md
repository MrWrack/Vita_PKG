# v2.3 startup / auto-close fix

Hardware symptom:
- LiveArea opens correctly.
- Starting the app immediately returns to LiveArea / closes.

Fixes:
1. BrowserList moved from main() stack to static global storage.
   - BrowserList is roughly 170-180 KB.
   - Keeping it on the Vita main-thread stack risks immediate stack overflow.

2. Settings state also moved to static storage.

3. Controller input is flushed for about 0.5 seconds after launch.
   - stale/held launch input cannot immediately trigger an action.

4. START only exits from the Home screen.

5. Renderer swaps a completed back buffer immediately after VBlank.
   - avoids queued-frame alternation.

All v2.2 LiveArea format fixes remain intact.
