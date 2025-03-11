SUPPARCHS		:= x86_64 # i386 aarch64 riscv64 loongarch64 ia64 mips64el
ARCH			:= x86_64

CC				= clang


# Make sure the architecture is supported.
ifeq ($(filter $(ARCH),$(SUPPARCHS)),)
$(error '$(ARCH)' is not a supported architecture)
endif


SRC_DIR			= ./
BUILD_DIR		= ../../build
INCLUDE_DIR		= ../include/kernel

OPTIM			= -O0
CFLAGS			= -target $(ARCH) \
					-ffreestanding -fno-common \
					-mno-red-zone -masm=intel -nostdlib \
					-Wall -Wextra -Wmissing-prototypes -Wstrict-prototypes \
					-I$(INCLUDE_DIR) $(OPTIM)
LDFLAGS			= -target $(ARCH) \
					-nostdlib -ffreestanding -I$(INCLUDE_DIR) $(OPTIM) \
					-z max-page-size=0x1000 \
					-T ./kernel.ld \
					-Wl,--build-id=none

SRCS			= $(shell find $(SRC_DIR) -type f -name "*.c") $(shell find $(SRC_DIR) -type f -name "*.cpp")
OBJS			= $(patsubst %.c,%.o,$(SRCS))

TARGET			= $(BUILD_DIR)/CrOwS.ELF


.PHONY: default
.PHONY: clean
.PHONY: clean-objs
.PHONY: debug
.PHONY: all
.PHONY: no-clean

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
debug: CFLAGS += -DCROWS_DEBUG=1
debug: $(TARGET) clean-objs

all: $(BUILD_DIR) $(TARGET) clean-objs
no-clean: $(BUILD_DIR) $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET): $(BUILD_DIR) $(OBJS)
	$(CC) $(LDFLAGS) -o $(TARGET) $(OBJS)
