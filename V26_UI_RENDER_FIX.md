# v2.6 UI render fix
Hardware test confirms v2.5 launches and remains running.

Fixes the overlapping/duplicated text shown on the Vita:
- clears the framebuffer once before every complete menu redraw
- does not continuously clear/redraw while idle
- keeps the safe single-framebuffer startup
- keeps the v2.5 root debug log
- keeps X Select, Triangle Delete, Square Refresh, Circle Back
