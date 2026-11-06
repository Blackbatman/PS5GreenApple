# PS5GreenApple - macOS Sonoma-style desktop environment for PS5
#
# Targets:
#   make        - build ps5greenapple.elf with ps5-payload-sdk (PS5)
#   make host   - build the headless dump binary for the host (testing)
#   make shots  - run the screenshot script and render PNGs into shots/
#   make clean
#
# PS5 prerequisites:
#   export PS5_PAYLOAD_SDK=/path/to/ps5-payload-sdk
#   SDL2 for PS5 must be installed into the SDK (see docs/INSTALL.md).

PS5_HOST ?= ps5
PS5_PORT ?= 9021

NAME := ps5greenapple
SRC_DIR := src

# ------------------------- PS5 build (default) -------------------------
ifdef PS5_PAYLOAD_SDK
include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk

CFLAGS  += -O2 -std=gnu11 -Wall -Wextra -Wno-unused-parameter \
	   -D__PS5__ \
	   -I$(SRC_DIR) -Ithird_party \
	   -I$(PS5_PAYLOAD_SDK)/target/include \
	   -I$(PS5_PAYLOAD_SDK)/target/include/SDL2

PS5_SRCS := $(filter-out $(SRC_DIR)/backend_dump.c,$(wildcard $(SRC_DIR)/*.c) $(SRC_DIR)/fonts.S)
PS5_OBJS := $(patsubst $(SRC_DIR)/%.c,build/ps5/%.o,$(filter %.c,$(PS5_SRCS))) \
	    build/ps5/fonts.o

LDFLAGS += -Wl,--allow-shlib-undefined -L$(PS5_PAYLOAD_SDK)/target/lib
# SDL2's PS5 video/keyboard/audio backends pull in IME dialog, USB keyboard,
# system-service and audio-out stubs on top of the core video/pad/user set.
LDLIBS  := -lSDL2 -lSceVideoOut -lSceSysmodule -lSceUserService -lScePad \
	   -lSceImeDialog -lSceKeyboard -lSceSystemService -lSceAudioOut \
	   -lc -lkernel

all: $(NAME).elf

build/ps5:
	@mkdir -p $@

build/ps5/%.o: $(SRC_DIR)/%.c | build/ps5
	$(CC) $(CFLAGS) -c -o $@ $<

build/ps5/%.o: $(SRC_DIR)/%.S | build/ps5
	$(CC) $(CFLAGS) -c -o $@ $<

$(NAME).elf: $(PS5_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

deploy: $(NAME).elf
	$(PS5_DEPLOY) -h $(PS5_HOST) -p $(PS5_PORT) $^

else
all:
	@echo "PS5_PAYLOAD_SDK is undefined - building host target instead."
	@echo "Set PS5_PAYLOAD_SDK to build the PS5 ELF (see docs/INSTALL.md)."
	@$(MAKE) host
endif

# ------------------------- host test build -------------------------
HOST_CC   ?= gcc
HOST_CFLAGS := -O2 -std=gnu11 -Wall -Wextra -Wno-unused-parameter \
	       -I$(SRC_DIR) -Ithird_party

HOST_SRCS := $(filter-out $(SRC_DIR)/backend_sdl2.c,$(wildcard $(SRC_DIR)/*.c) $(SRC_DIR)/fonts.S)

host:
	@mkdir -p build/host
	$(HOST_CC) $(HOST_CFLAGS) -o build/host/$(NAME)-dump $(HOST_SRCS) -lm

shots: host
	./build/host/$(NAME)-dump --script scripts/demo-script.txt --outdir shots

clean:
	rm -rf build $(NAME).elf shots

.PHONY: all host sim shots clean deploy

# ------------------------- desktop sim (macOS / Linux) -------------------------
# Builds the real SDL2 windowed simulation (backend_sdl2.c) without any PS5
# SDK. Needs SDL2 development files:
#   macOS:  brew install sdl2     (sdl2-config ships with the formula)
#   Debian: apt install libsdl2-dev
SIM_SRCS := $(filter-out $(SRC_DIR)/backend_dump.c,$(wildcard $(SRC_DIR)/*.c) $(SRC_DIR)/fonts.S)

sim:
	@command -v sdl2-config >/dev/null 2>&1 || { \
	        echo "sdl2-config not found - install SDL2 dev files first:"; \
	        echo "  macOS:  brew install sdl2"; \
	        echo "  Debian: apt install libsdl2-dev"; exit 1; }
	@mkdir -p build/sim
	$(HOST_CC) $(shell sdl2-config --cflags) -O2 -std=gnu11 -Wall -Wextra \
	        -Wno-unused-parameter -I$(SRC_DIR) -Ithird_party \
	        -o build/sim/$(NAME)-sim $(SIM_SRCS) $(shell sdl2-config --libs) -lm

