# This version of rules.mk expects the following to be defined before
# inclusion..
### REQUIRED ###
# OPENCM3_DIR - duh
# PROJECT - will be the basename of the output elf, eg usb-gadget0-stm32f4disco
# CFILES - basenames only, eg main.c blah.c
# CXXFILES - same for C++ files. Must have cxx suffix!
# DEVICE - the full device name, eg stm32f405ret6
#  _or_
# LDSCRIPT - full path, eg ../../examples/stm32/f4/stm32f4-discovery/stm32f4-discovery.ld
# OPENCM3_LIB - the basename, eg: opencm3_stm32f4
# OPENCM3_DEFS - the target define eg: -DSTM32F4
# ARCH_FLAGS - eg, -mthumb -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16
#    (ie, the full set of cpu arch flags, _none_ are defined in this file)
#
### OPTIONAL ###
# INCLUDES - fully formed -I paths, if you want extra, eg -I../shared
# BUILD_DIR - defaults to bin, should set this if you are building multiarch
# OPT - full -O flag, defaults to -Os
# CSTD - defaults -std=c99
# CXXSTD - no default.
# OOCD_INTERFACE - eg stlink-v2
# OOCD_TARGET - eg stm32f4x
#    both only used if you use the "make flash" target.
# OOCD_FILE - eg my.openocd.cfg
#    This overrides interface/target above, and is used as just -f FILE
### TODO/FIXME/notes ###
# No support for stylecheck.
# No support for BMP/texane/random flash methods, no plans either
# C++ hasn't been actually tested with this..... sorry bout that. ;)
# Second expansion/secondary not set, add this if you need them.

# Directory of this file (repo root), even when included from a subproject.
RULES_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

BUILD_DIR ?= bin
OPT ?= -Os
CSTD ?= -std=c99

# Be silent per default, but 'make V=1' will show all compiler calls.
# If you're insane, V=99 will print out all sorts of things.
V?=0
ifeq ($(V),0)
Q	:= @
NULL	:= 2>/dev/null
endif

# genlink-config.mk only sets LIBNAME if the archive already exists.
# On a first build, derive it from the device family and compile libopencm3.
ifeq ($(LIBNAME),)
ifneq ($(genlink_family),)
LIBNAME = opencm3_$(genlink_family)
LDLIBS := $(filter-out -l,$(LDLIBS))
LDLIBS += -l$(LIBNAME)
LIBDEPS := $(filter-out $(OPENCM3_DIR)/lib/lib.a,$(LIBDEPS))
LIBDEPS += $(OPENCM3_DIR)/lib/lib$(LIBNAME).a
endif
endif

# Tool paths.
PREFIX	?= arm-none-eabi-
CC	= $(PREFIX)gcc
CXX	= $(PREFIX)g++
LD	= $(PREFIX)gcc
OBJCOPY	= $(PREFIX)objcopy
OBJDUMP	= $(PREFIX)objdump
SIZE	= $(PREFIX)size
PYTHON	?= python3
OOCD	?= openocd
UF2_BASE ?= 0x08000000
BIN2UF2  = $(RULES_DIR)bin2uf2.py
ifeq ($(UF2_FAMILY_ID),)
UF2_FAMILY_FLAG = --device "$(DEVICE)"
else
UF2_FAMILY_FLAG = --family $(UF2_FAMILY_ID)
endif

OPENCM3_INC = $(OPENCM3_DIR)/include

# Inclusion of library header files
INCLUDES += $(patsubst %,-I%, . $(OPENCM3_INC) )

OBJS = $(CFILES:%.c=$(BUILD_DIR)/%.o)
OBJS += $(CXXFILES:%.cxx=$(BUILD_DIR)/%.o)
OBJS += $(AFILES:%.S=$(BUILD_DIR)/%.o)
GENERATED_BINS = $(PROJECT).elf $(PROJECT).bin $(PROJECT).hex $(PROJECT).uf2 \
		$(PROJECT).map $(PROJECT).list $(PROJECT).lss

TGT_CPPFLAGS += -MD
TGT_CPPFLAGS += -Wall -Wundef $(INCLUDES)
TGT_CPPFLAGS += $(INCLUDES) $(OPENCM3_DEFS)

TGT_CFLAGS += $(OPT) $(CSTD) -ggdb3
TGT_CFLAGS += $(ARCH_FLAGS)
TGT_CFLAGS += -fno-common
TGT_CFLAGS += -ffunction-sections -fdata-sections
TGT_CFLAGS += -Wextra -Wshadow -Wno-unused-variable -Wimplicit-function-declaration
TGT_CFLAGS += -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes

TGT_CXXFLAGS += $(OPT) $(CXXSTD) -ggdb3
TGT_CXXFLAGS += $(ARCH_FLAGS)
TGT_CXXFLAGS += -fno-common
TGT_CXXFLAGS += -ffunction-sections -fdata-sections
TGT_CXXFLAGS += -Wextra -Wshadow -Wredundant-decls  -Weffc++

TGT_ASFLAGS += $(OPT) $(ARCH_FLAGS) -ggdb3

TGT_LDFLAGS += -T$(LDSCRIPT) -L$(OPENCM3_DIR)/lib -nostartfiles
TGT_LDFLAGS += $(ARCH_FLAGS)
# ARM GNU toolchain / Ubuntu ship newlib-nano (nano.specs).
# Debian/Kali currently ship picolibc instead.
ifeq ($(shell $(CC) -print-file-name=nano.specs),nano.specs)
TGT_LDFLAGS += -specs=picolibc.specs
else
TGT_LDFLAGS += -specs=nano.specs
endif
TGT_LDFLAGS += -Wl,--gc-sections
# OPTIONAL
#TGT_LDFLAGS += -Wl,-Map=$(PROJECT).map
ifeq ($(V),99)
TGT_LDFLAGS += -Wl,--print-gc-sections
endif

# Linker script generator fills this in for us.
ifeq (,$(DEVICE))
LDLIBS += -l$(OPENCM3_LIB)
endif
# nosys is only in newer gcc-arm-embedded...
#LDLIBS += -specs=nosys.specs
LDLIBS += -Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group

# Burn in legacy hell fortran modula pascal yacc idontevenwat
.SUFFIXES:
.SUFFIXES: .c .S .h .o .cxx .elf .bin .hex .uf2 .list .lss

# Bad make, never *ever* try to get a file out of source control by yourself.
%: %,v
%: RCS/%,v
%: RCS/%
%: s.%
%: SCCS/s.%

all: $(PROJECT).elf $(PROJECT).bin $(PROJECT).hex $(PROJECT).uf2 size
flash: $(PROJECT).flash
size: $(PROJECT).elf
	@printf "  SIZE\t$<\n"
	$(Q)$(SIZE) $<
	$(Q)$(SIZE) $< | awk -v ldscript="$(LDSCRIPT)" '\
		function bytes(v) { \
			gsub(/,/, "", v); \
			if (v ~ /[Kk]$$/) return substr(v,1,length(v)-1)*1024; \
			if (v ~ /[Mm]$$/) return substr(v,1,length(v)-1)*1024*1024; \
			return v+0; \
		} \
		BEGIN { \
			while ((getline line < ldscript) > 0) { \
				n = split(line, f); \
				for (i = 1; i <= n; i++) { \
					if (f[i] != "LENGTH") continue; \
					len = bytes(f[i+2]); \
					if (line ~ /^[ \t]*rom /) rom_max = len; \
					if (line ~ /^[ \t]*ram /) ram_max = len; \
				} \
			} \
			close(ldscript); \
		} \
		NR == 2 { \
			rom = $$1 + $$2; \
			ram = $$2 + $$3; \
			printf "  ROM\t%d / %d bytes (%.2f%%)\n", rom, rom_max, (rom_max ? 100*rom/rom_max : 0); \
			printf "  RAM\t%d / %d bytes (%.2f%%)  [static: data+bss]\n", ram, ram_max, (ram_max ? 100*ram/ram_max : 0); \
		}'

# error if not using linker script generator
ifeq (,$(DEVICE))
$(LDSCRIPT):
ifeq (,$(wildcard $(LDSCRIPT)))
    $(error Unable to find specified linker script: $(LDSCRIPT))
endif
else
# if linker script generator was used, make sure it's cleaned.
GENERATED_BINS += $(LDSCRIPT)
endif

# Need a special rule to have a bin dir
# First-time (or after a library clean): stm32f4 -> TARGETS=stm32/f4
$(OPENCM3_DIR)/lib/libopencm3_%.a:
	@printf "  BUILD\tlibopencm3 ($*)\n"
	$(Q)$(MAKE) -C $(OPENCM3_DIR) PREFIX="$(PREFIX)" TARGETS="$(patsubst stm32%,stm32/%,$*)"

$(BUILD_DIR)/%.o: %.c
	@printf "  CC\t$<\n"
	@mkdir -p $(dir $@)
	$(Q)$(CC) $(TGT_CFLAGS) $(CFLAGS) $(TGT_CPPFLAGS) $(CPPFLAGS) -o $@ -c $<

$(BUILD_DIR)/%.o: %.cxx
	@printf "  CXX\t$<\n"
	@mkdir -p $(dir $@)
	$(Q)$(CXX) $(TGT_CXXFLAGS) $(CXXFLAGS) $(TGT_CPPFLAGS) $(CPPFLAGS) -o $@ -c $<

$(BUILD_DIR)/%.o: %.S
	@printf "  AS\t$<\n"
	@mkdir -p $(dir $@)
	$(Q)$(CC) $(TGT_ASFLAGS) $(ASFLAGS) $(TGT_CPPFLAGS) $(CPPFLAGS) -o $@ -c $<

$(PROJECT).elf: $(OBJS) $(LDSCRIPT) $(LIBDEPS)
	@printf "  LD\t$@\n"
	$(Q)$(LD) $(TGT_LDFLAGS) $(LDFLAGS) $(OBJS) $(LDLIBS) -o $@

%.bin: %.elf
	@printf "  OBJCOPY\t$@\n"
	$(Q)$(OBJCOPY) -O binary  $< $@

%.hex: %.elf
	@printf "  OBJCOPY\t$@\n"
	$(Q)$(OBJCOPY) -O ihex $< $@

%.uf2: %.bin
	@printf "  UF2\t$@\n"
	$(Q)$(PYTHON) $(BIN2UF2) --base $(UF2_BASE) $(UF2_FAMILY_FLAG) -o $@ $<

%.lss: %.elf
	$(OBJDUMP) -h -S $< > $@

%.list: %.elf
	$(OBJDUMP) -S $< > $@

%.flash: %.elf
	@printf "  FLASH\t$<\n"
ifeq (,$(OOCD_FILE))
	$(Q)(echo "halt; program $(realpath $(*).elf) verify reset" | nc -4 localhost 4444 2>/dev/null) || \
		$(OOCD) -f interface/$(OOCD_INTERFACE).cfg \
		-f target/$(OOCD_TARGET).cfg \
		-c "program $(realpath $(*).elf) verify reset exit" \
		$(NULL)
else
	$(Q)(echo "halt; program $(realpath $(*).elf) verify reset" | nc -4 localhost 4444 2>/dev/null) || \
		$(OOCD) -f $(OOCD_FILE) \
		-c "program $(realpath $(*).elf) verify reset exit" \
		$(NULL)
endif

clean:
	rm -rf $(BUILD_DIR) $(GENERATED_BINS)

.PHONY: all clean flash size
-include $(OBJS:.o=.d)

