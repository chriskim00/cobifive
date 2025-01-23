cmd_/home/willammoy/cobifive/quadpcie/modules.order := {   echo /home/willammoy/cobifive/quadpcie/pci_driver64.ko; :; } | awk '!x[$$0]++' - > /home/willammoy/cobifive/quadpcie/modules.order
