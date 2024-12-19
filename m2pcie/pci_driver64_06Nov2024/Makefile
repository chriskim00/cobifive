obj-m += pci_driver64.o

KDIR := /lib/modules/$(shell uname -r)/build
CURRENT_PATH := $(shell pwd)
EXTRA_CFLAGS := -std=gnu99

all:
	make -C $(KDIR) M=$(CURRENT_PATH) modules

install:
	make -C $(KDIR) M=$(CURRENT_PATH) modules_install
	depmod -a

clean:
	make -C $(KDIR) M=$(CURRENT_PATH) clean
