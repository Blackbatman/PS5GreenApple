# WebKit Autoloader tile — PS5GreenApple

This folder is a [PS5 WebKit Autoloader](https://github.com/EchoStretch/WebKit-Autoloader)
"tile" that lets you launch PS5GreenApple straight from the PS5's embedded
WebKit browser — no PC required at launch time.

## USB layout

Copy this folder to a FAT32/exFAT USB stick exactly like this:

```
USB0:\
└── PS5\
    └── WWW\
        └── tile\
            └── PS5GreenApple\
                ├── index.html      <- this file
                ├── payload.elf     <- ps5greenapple.elf renamed
                └── icon.png        (optional, shown by the tile list)
```

## Usage

1. Jailbreak the console with your WebKit entry point of choice and run
   etaHEN (the Autoloader expects etaHEN's ELF loader / debugger services).
2. Insert the USB stick, open the PS5 **Web Browser** ("User's Guide" trick
   or any opened browser entry).
3. Load the Autoloader page hosted from the stick and select the
   **PS5GreenApple** tile. `index.html` fetches `payload.elf` and hands it to
   the loader chain; the desktop boots a few seconds later.

## Notes

- `index.html` uses feature detection: if the Autoloader JS bridge is
  present it launches automatically; otherwise it shows these instructions
  instead of failing silently.
- Firmware availability of the WebKit entry point varies (WebKit user
  patches exist for several firmwares; on 13.60+ prefer the Relapse/UMTX
  chains and the etaHEN ELF loader route from `docs/INSTALL.md`).
