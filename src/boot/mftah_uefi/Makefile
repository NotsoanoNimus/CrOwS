SUPPARCHS		:= x86_64 # i386 aarch64 riscv64 loongarch64 ia64 mips64el
ARCH			:= x86_64

CXX				= clang


# Make sure the architecture is supported.
ifeq ($(filter $(ARCH),$(SUPPARCHS)),)
$(error '$(ARCH)' is not a supported architecture)
endif


SRC_DIR			= ./
BUILD_DIR		= ../../../build
INCLUDE_DIR		= ../../../include/boot/mftah_uefi

GNUEFI_SUBMOD	= $(INCLUDE_DIR)/gnu-efi

EFIINC			= $(GNUEFI_SUBMOD)/inc
EFIINCS			= -I$(EFIINC) -I$(EFIINC)/$(ARCH) -I$(EFIINC)/protocol

LIB				= $(GNUEFI_SUBMOD)/$(ARCH)
EFILIBS			= -L$(LIB)/lib -L$(LIB)/gnuefi

LIBGNUEFI		= $(LIB)/gnuefi/libgnuefi.a
LIBEFI			= $(LIB)/lib/libefi.a

LIBMFTAH		= $(BUILD_DIR)/libmftah.a

OPTIM			= -O3
CFLAGS			= -target $(ARCH)-unknown-windows -ffreestanding -fshort-wchar \
					-mno-red-zone -Wall -I$(INCLUDE_DIR) $(EFIINCS) $(OPTIM) \
					-DMFTAH_RELEASE_DATE=$(shell printf "0x`date +%Y``date +%m`%d" `date +%02d`) \
					-DMFTAH_ARCH=$(ARCH)
LDFLAGS			= -target $(ARCH)-unknown-windows -nostdlib -Wl,-entry:efi_main \
					-Wl,-subsystem:efi_application -fuse-ld=lld-link $(EFILIBS)

SRCS_GNU_EFI	= $(shell find $(GNUEFI_SUBMOD)/lib -maxdepth 1 -type f -name "*.c" | grep -Pvi '(entry|lock)\.')
SRCS_ARCH		= $(shell find $(GNUEFI_SUBMOD)/lib/$(ARCH) -maxdepth 1 -type f -name "*.c")
SRCS_RT			= $(shell find $(GNUEFI_SUBMOD)/lib/runtime -maxdepth 1 -type f -name "*.c")
SRCS			= $(shell find $(SRC_DIR) -maxdepth 1 -type f -name "*.c")

OBJS_GNUEFI		= $(patsubst %.c,%.o,$(SRCS_GNU_EFI) $(SRCS_ARCH) $(SRCS_RT))
OBJS			= $(patsubst %.c,%.o,$(SRCS))

OBJS			= $(OBJS_GNUEFI) $(OBJS)

TARGET			= $(BUILD_DIR)/MFTAH.EFI


.PHONY: default
.PHONY: clean
.PHONY: clean-objs
.PHONY: debug
.PHONY: all

default: all

clean:
	-rm $(TARGET)* &>/dev/null
	-rm $(OBJS) &>/dev/null

clean-objs:
	-rm $(OBJS) &>/dev/null

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR) &>/dev/null

# We can assume a 'clean' should be run on all .o files
#   after the build completes. This is because compilation
#   of the EFI file is rather expedient anyway, and it
#   helps not to mix up release and debug build artifacts.
debug: CFLAGS += -DEFI_DEBUG=1
debug: $(TARGET) clean-objs

all: $(BUILD_DIR) $(TARGET) clean-objs

%.o: %.c
	$(CXX) $(CFLAGS) -c -o $@ $<

$(TARGET): $(LIBGNUEFI) $(LIBEFI) $(LIBMFTAH) $(OBJS)
	$(CXX) $(LDFLAGS) -o $(TARGET) $(OBJS) $(LIBMFTAH)

$(LIBMFTAH):
	$(MAKE) -C $(MFTAH_DIR) static
	@cp $(MFTAH_DIR)/build $(LIBMFTAH)

$(LIBEFI): $(LIBGNUEFI)
$(LIBGNUEFI): $(GNUEFI_SUBMOD) $(GNUEFI_SUBMOD)/README.gnuefi
	$(MAKE) -C $(GNUEFI_SUBMOD)
	$(MAKE) $(TARGET)
