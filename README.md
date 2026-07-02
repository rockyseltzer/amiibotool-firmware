# Pixl.js for Amiibotool/Evergenesis/nfctool/iNFC

This repo is a forked version of the Pixl.js firmware by solosky. The modified firmware flips the OLED screen 180 degrees and adds back-button functionality not present in the official firmware.

## Images

![image](https://github.com/rockyseltzer/amiibotool-firmware/blob/main/assets/amiibotool.png)

## Updating Firmware

* [https://thegecko.github.io/web-bluetooth-dfu/examples/web.html](https://thegecko.github.io/web-bluetooth-dfu/examples/web.html)

1) **Browser check first:** This website needs Web Bluetooth. Use Chrome or Edge (Firefox and Safari won't work) and make sure the laptop's Bluetooth is on.
2) **Load the package:** Drag the firmware .zip file onto "Choose a firmware package," or click it and select the file. Load the zip before selecting the device — the tool reads the manifest.json inside to know what it's flashing.
3) **Put the device in DFU mode:** On the device, Settings → Firmware Update. It reboots and starts advertising as a DFU target.
4) **Click "Select Device":** Chrome opens its Bluetooth chooser. Pick the DFU device (look for a name like "pixl dfu"). The transfer starts automatically and you'll see progress; the device reboots into the new firmware when it completes.
