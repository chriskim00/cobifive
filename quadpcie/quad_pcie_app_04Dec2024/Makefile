obj-m += pci_driver64.o
CC := x86_64-linux-gnu-gcc-12
KDIR := /lib/modules/$(shell uname -r)/build
CURRENT_PATH := $(shell pwd)
EXTRA_CFLAGS := -std=gnu99 -O3 -funroll-loops

all:
	make -C $(KDIR) M=$(CURRENT_PATH) modules

install:
	make -C $(KDIR) M=$(CURRENT_PATH) modules_install
	depmod -a

clean:
	make -C $(KDIR) M=$(CURRENT_PATH) clean

