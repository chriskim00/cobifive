obj-m += vpci_driver64.o
CC := x86_64-linux-gnu-gcc-12
KDIR := /lib/modules/$(shell uname -r)/build
CURRENT_PATH := $(shell pwd)
EXTRA_CFLAGS := -std=gnu99
LOG_FILE := build_errors.log

all:
	make -C $(KDIR) M=$(CURRENT_PATH) modules 2> $(LOG_FILE)

install:
	make -C $(KDIR) M=$(CURRENT_PATH) modules_install
	depmod -a

clean:
	make -C $(KDIR) M=$(CURRENT_PATH) clean

