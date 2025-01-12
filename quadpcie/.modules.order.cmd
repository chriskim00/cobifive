cmd_/home/cobidev/vdriver/quadpcie/modules.order := {   echo /home/cobidev/vdriver/quadpcie/vpci_driver64.ko; :; } | awk '!x[$$0]++' - > /home/cobidev/vdriver/quadpcie/modules.order
