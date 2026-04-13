# sillydoom — Nintendo Switch build (id DOOM engine)
#
# Cross-compiles the id DOOM engine as a static library, links with
# isilly, bundles scripts + WAD into romfs, packages as NRO.
#
# Usage:
#   make          Build NRO
#   make deploy   Send to Switch via nxlink
#   make clean    Remove build artifacts

APP_NAME    := sillydoom
APP_TITLE   := sillydoom
APP_AUTHOR  := allbrance real
APP_VERSION := 1.0.0

DEVKITPRO  ?= /opt/devkitpro
SILLYSTATE ?= ../../../sillystate

# Toolchain
DEVKITA64   := $(DEVKITPRO)/devkitA64
CC          := $(DEVKITA64)/bin/aarch64-none-elf-gcc
AR          := $(DEVKITA64)/bin/aarch64-none-elf-ar

# Paths
ISILLY_ROOT     := $(SILLYSTATE)/isilly
SWITCH_BUILD    := $(SILLYSTATE)/build-switch
ISILLY_FRONTEND := $(SWITCH_BUILD)/isilly/libisilly_frontend.a
EXT_SRC         := ../ext
DOOM_SRC        := ../ext/doom_engine

# Common CFLAGS
BASE_CFLAGS := -O2 -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE \
               -D__SWITCH__ \
               -I$(ISILLY_ROOT)/sdk/include \
               -I$(ISILLY_ROOT)/include \
               -I$(SILLYSTATE)/arm64/include \
               -I$(ISILLY_ROOT)/src/ext/audio \
               -I$(DEVKITPRO)/libnx/include

# MUS parser + FM synth for music
MUS_SRC   := $(EXT_SRC)/doom_audio

# Extra flags for the id DOOM engine (C89 code with implicit ints etc.)
DOOM_CFLAGS := $(BASE_CFLAGS) \
               -std=gnu11 \
               -I$(DOOM_SRC) \
               -I$(MUS_SRC) \
               -DNORMALUNIX -DLINUX \
               -Wno-implicit-function-declaration \
               -Wno-implicit-int \
               -Wno-int-conversion \
               -Wno-incompatible-pointer-types \
               -Wno-pointer-sign \
               -Wno-return-type \
               -Wno-unused-result \
               -Wno-parentheses \
               -Wno-dangling-else \
               -Wno-shift-negative-value \
               -Wno-format

# All id DOOM engine .c files + MUS/synth from doom_audio
DOOM_SRCS := $(wildcard $(DOOM_SRC)/*.c)
DOOM_OBJS := $(patsubst $(DOOM_SRC)/%.c,build/doom_%.o,$(DOOM_SRCS))
MUS_SRCS  := $(MUS_SRC)/mus_parser.c $(MUS_SRC)/midi_synth.c $(MUS_SRC)/synth_fm.c
MUS_OBJS  := build/mus_parser.o build/midi_synth.o build/synth_fm.o

.PHONY: all clean deploy romfs check_deps

all: check_deps build/doom_engine.a sillydoom_switch.elf romfs $(APP_NAME).nro

check_deps:
	@if [ ! -f "$(ISILLY_FRONTEND)" ]; then \
		echo "ERROR: isilly Switch frontend not found at $(ISILLY_FRONTEND)"; \
		echo "Build sillystate first: cd $(SILLYSTATE) && DEVKITPRO=$(DEVKITPRO) ./build.sh --switch"; \
		exit 1; \
	fi

build:
	@mkdir -p build

# Compile each doom engine .c file
build/doom_%.o: $(DOOM_SRC)/%.c | build
	$(CC) $(DOOM_CFLAGS) -c -o $@ $<

# Bridge: registers doom_engine into isilly's require() system
build/doom_ext_bridge.o: doom_ext_bridge.c | build
	$(CC) $(BASE_CFLAGS) -c -o $@ $<

# MUS/synth objects
build/mus_parser.o: $(MUS_SRC)/mus_parser.c | build
	$(CC) $(BASE_CFLAGS) -I$(MUS_SRC) -c -o $@ $<

build/midi_synth.o: $(MUS_SRC)/midi_synth.c | build
	$(CC) $(BASE_CFLAGS) -I$(MUS_SRC) -c -o $@ $<

build/synth_fm.o: $(MUS_SRC)/synth_fm.c | build
	$(CC) $(BASE_CFLAGS) -I$(MUS_SRC) -c -o $@ $<

# Static library: all doom engine objects + MUS/synth + bridge
build/doom_engine.a: $(DOOM_OBJS) $(MUS_OBJS) build/doom_ext_bridge.o
	$(AR) rcs $@ $^
	@echo "Built doom_engine.a ($(words $(DOOM_OBJS)) engine files + bridge)"

# Link: custom main (doom_ext_bridge) + doom_engine + isilly frontend
sillydoom_switch.elf: build/doom_engine.a
	$(CC) -march=armv8-a+crc+crypto+lse -mtune=cortex-a57 -mtp=soft -fPIE \
		-D__SWITCH__ -O3 -DNDEBUG \
		-specs=$(DEVKITPRO)/libnx/switch.specs \
		build/doom_engine.a \
		$(SWITCH_BUILD)/isilly/libisilly_frontend.a \
		build/doom_engine.a \
		$(SWITCH_BUILD)/isilly/libisilly_stub.a \
		$(SWITCH_BUILD)/arm64/libmetal_backend_stub.a \
		$(DEVKITPRO)/libnx/lib/libnx.a \
		-lm \
		$(DEVKITPRO)/libnx/lib/libdeko3d.a \
		$(DEVKITPRO)/libnx/lib/libnx.a \
		-o $@
	@echo "Built sillydoom_switch.elf"

romfs:
	@mkdir -p romfs/src romfs/assets
	cp -r ../src/*.is romfs/src/
	cp ../.isconfig romfs/ 2>/dev/null || true
ifdef EMBED_WAD
	@# Embed WAD in romfs (makes NRO ~13MB but self-contained)
	@[ -d ../assets ] && cp -r ../assets/* romfs/assets/ 2>/dev/null || true
	@cd romfs/assets && for f in *.WAD; do \
		lc=$$(echo "$$f" | tr 'A-Z' 'a-z'); \
		[ "$$f" != "$$lc" ] && mv "$$f" "$$lc" 2>/dev/null; \
	done; true
	@echo "romfs/ prepared (WAD embedded)"
else
	@echo "romfs/ prepared (no WAD — place DOOM.WAD in sdmc:/switch/sillydoom/)"
endif

$(APP_NAME).nacp:
	$(DEVKITPRO)/tools/bin/nacptool --create \
		"$(APP_TITLE)" "$(APP_AUTHOR)" "$(APP_VERSION)" \
		$(APP_NAME).nacp

$(APP_NAME).nro: $(APP_NAME).nacp sillydoom_switch.elf romfs
	$(DEVKITPRO)/tools/bin/elf2nro \
		sillydoom_switch.elf \
		$(APP_NAME).nro \
		--nacp=$(APP_NAME).nacp \
		--icon=icon/icon.jpg \
		--romfsdir=romfs
	@echo "NRO: $(APP_NAME).nro"
	@ls -lh $(APP_NAME).nro

deploy: $(APP_NAME).nro
	$(DEVKITPRO)/tools/bin/nxlink -s $(APP_NAME).nro

clean:
	rm -rf build romfs *.nro *.nacp *.elf *.o *.a
