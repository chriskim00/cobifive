obj-m += pci_driver64.o
#obj-m += vpci_driver64.o

# Compiler settings - use system gcc
ARCH := $(shell uname -m)
KERNEL := $(shell uname -r)
KERNEL_BUILD := /lib/modules/$(KERNEL)/build
CROSS_COMPILE :=
CC := x86_64-linux-gnu-gcc-12
EXTRA_CFLAGS :=  -std=gnu99 

# WSL2 Kernel directories
KVERSION := $(shell uname -r)

# Current directory
CURRENT_PATH := $(shell pwd)

# Logging
LOG_FILE := build.log

#Build directory
BUILD_DIR := build


all:
	$(MAKE) -C $(KERNEL_BUILD) M=$(CURRENT_PATH)  modules 2> $(LOG_FILE)

install:
	$(MAKE) -C $(KERNEL_BUILD) M=$(CURRENT_PATH)  modules_install
	depmod -a

clean:
	$(MAKE) -C $(KERNEL_BUILD) M=$(CURRENT_PATH)  clean

#OLD MAKEFILE VERSION FOR non-WSL 
# CC := x86_64-linux-gnu-gcc-12
# KDIR := /lib/modules/$(shell uname -r)/build
# CURRENT_PATH := $(shell pwd)
# EXTRA_CFLAGS := -std=gnu99
# LOG_FILE := build_errors.log

# all:
# 	make -C $(KDIR) M=$(CURRENT_PATH) modules 2> $(LOG_FILE)

# install:
# 	make -C $(KDIR) M=$(CURRENT_PATH) modules_install
# 	depmod -a

# clean:
# 	make -C $(KDIR) M=$(CURRENT_PATH) clean

