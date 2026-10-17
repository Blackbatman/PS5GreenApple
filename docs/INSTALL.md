# Installing PS5GreenApple

This guide covers every supported way to get `ps5greenapple.elf` running on a
jailbroken PS5, from toolchain setup to the final `nc` command.

---

## 0. What you need

- A jailbroken PS5 (any firmware with a working homebrew loader — 3.00 through
  13.60 at the time of writing).
- A loader chain already in place. The two common ones:
  - **etaHEN** (started after your exploit of choice: BD-J, WebKit, or the
    newer kernel chains such as UMTX/Relapse on recent firmwares), or
  - **ps5-payload-dev elfldr**, started through the rudimentary kernel loader
    on port 9020.
- A PC on the same network with `nc` (netcat), or a USB stick for the
  WebKit Autoloader route.
- *(Build only)* Linux/macOS with the ps5-payload-sdk, or just download the
  prebuilt ELF from GitHub Actions artifacts.

---

## 1. Build the ELF yourself

### 1.1 ps5-payload-sdk

```bash
git clone https://github.com/ps5-payload-dev/sdk ps5-sdk
export PS5_PAYLOAD_SDK=$(pwd)/ps5-sdk
export PATH=$PATH:$PS5_PAYLOAD_SDK/bin
```

The SDK ships `toolchain/prospero.mk`, which the project Makefile includes to
get `CC` and friends. No manual compiler flags are needed.

### 1.2 SDL2 for PS5 (one-time)

PS5GreenApple uses SDL2 for video output and DualSense input. Build the
PS5 SDL port and install it into the SDK's `target/` prefix:

```bash
git clone --recursive https://github.com/ps5-payload-dev/SDL
cd SDL
mkdir build && cd build
../configure --host=x86_64-pc-freebsd12-elf \
    --prefix=$PS5_PAYLOAD_SDK/target \
    --disable-shared --enable-static
make -j$(nproc)
make install
```

If your SDL build lands `sdl2.pc` somewhere else, point
`PKG_CONFIG_PATH` at it or add `-I`/`-L` flags to `CFLAGS`/`LDFLAGS` in the
Makefile.

### 1.3 Build

```bash
git clone https://github.com/Blackbatman/PS5GreenApple
cd PS5GreenApple
make                # produces ps5greenapple.elf
```

Useful extras:

```bash
make host           # headless host build (no SDK required)
make shots          # regenerate all README screenshots from the demo script
make clean
```

### 1.4 Or skip the build entirely

Every push to the main branch builds `ps5greenapple.elf` in CI
(`.github/workflows/ci.yml`). Download it from the workflow **Artifacts**
section and jump to step 2.

---

## 2. Send it to the PS5

### 2.1 etaHEN ELF loader

1. Run your firmware's exploit, then etaHEN.
2. Open etaHEN's **toolbox** and start the **ELF loader** (it listens on
   TCP **9021**).
3. From your PC:

   ```bash
   nc -q0 <PS5-IP> 9021 < ps5greenapple.elf
   ```

   (macOS/BSD netcat: `nc <PS5-IP> 9021 < ps5greenapple.elf`)

### 2.2 ps5-payload-dev elfldr

If the kernel exploit only exposes the rudimentary loader on **9020**, start
[elfldr](https://github.com/ps5-payload-dev/elfldr) first:

```bash
export PS5_HOST=<PS5-IP>
wget -q -O - https://github.com/ps5-payload-dev/elfldr/releases/latest/download/elfldr-ps5.elf \
    | nc -q0 $PS5_HOST 9020
```

Then send the payload on **9021**:

```bash
nc -q0 $PS5_HOST 9021 < ps5greenapple.elf
```

The loader stays resident, so you can relaunch the app any time without
re-running the exploit (until reboot).

### 2.3 ps5-payload-websrv (launch from any browser)

1. Start [websrv](https://github.com/ps5-payload-dev/websrv) once:
   ```bash
   wget -q -O - https://github.com/ps5-payload-dev/websrv/releases/latest/download/websrv-ps5.elf \
       | nc -q0 <PS5-IP> 9021
   ```
2. Put the payload where websrv looks for homebrew:
   ```
   /data/homebrew/PS5GreenApple/eboot.elf
   /data/homebrew/PS5GreenApple/sce_sys/icon0.png   (optional 512x512 icon)
   ```
   A USB stick works too: `/mnt/usb0/homebrew/PS5GreenApple/eboot.elf`.
3. Browse to `http://<PS5-IP>:8080/` and click **PS5GreenApple**.
   From the PS5's own WebKit browser use `http://127.0.0.1:8080/`.

### 2.4 WebKit Autoloader tile (no PC on the network)

1. Copy `tile/` from this repo onto a FAT32/exFAT USB stick as
   `PS5/WWW/tile/PS5GreenApple/` and drop `ps5greenapple.elf` next to
   `index.html` (renamed `payload.elf`).
2. Open the PS5 WebKit browser and load the Autoloader host page from the
   stick; pick the tile.
3. Details and file layout: [`tile/README.md`](../tile/README.md).

---

## 3. Data & settings

| Path | Purpose |
|------|---------|
| `/data/PS5GreenApple/settings.ini` | wallpaper, accent, clock options |
| `/data/PS5GreenApple/notes.txt` | TextEdit save target |
| `/data/PS5GreenApple/linux/bzImage` + `initramfs.img` | Linux files detected by Boot Camp |

Settings load at boot; if the directory does not exist the app creates it on
first save.

---

## 4. Troubleshooting

**`nc` connects but nothing happens.**
The ELF loader is not running. Start etaHEN's ELF loader (toolbox) or
ps5-payload-dev/elfldr; confirm with the toolbox status page.

**Payload starts and exits instantly.**
Build a debug variant (`-g -O0`), attach `gdb` via the SDK's debug target, or
check that your SDK version matches the one the payload was built with
(v0.44.x at time of release). Version mismatches between SDL builds are the
most common cause.

**Black screen with audio-less silence.**
The SDL2 port needs the video-out sysmodule; make sure you built SDL from
`ps5-payload-dev/SDL` and linked with `-lSDL2` (the Makefile already does).

**Terminal says `popen failed`.**
Some minimal libc builds disable `popen`. Rebuild with `-DNO_SHELL` to get a
stub that keeps the UI usable and reports the limitation.

**Finder shows `Cannot open /data`.**
You are probably running an older exploit chain without `/data` mounted.
Use the USB favorites (`/mnt/usb0`) instead.

---

## 5. Uninstall

Just don't send the payload — nothing is installed persistently. To wipe
settings: delete `/data/PS5GreenApple/`.
