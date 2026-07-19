# cursor-switcher

Made for Desktop XR and MSFS2024. Clicking back button on the mouse _when Flightsimulator2024.exe_ is running will toggle the cursor between primary and secondary monitor breaking the MSFS2024 cursor lock. MSFS on monitor 0.

It runs in the background, just add it as a startup app, running as admin:

<img width="284" height="169" alt="image" src="https://github.com/user-attachments/assets/572d91ba-6004-4238-9cf4-2db8929123b6" />

<img width="326" height="274" alt="image" src="https://github.com/user-attachments/assets/de01052f-62ef-47dc-b6be-f9bd731e7ddd" />

## Build

```
cl cursor-switcher.c /link user32.lib gdi32.lib /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup
```
