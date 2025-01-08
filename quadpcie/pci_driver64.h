#ifndef PCI_DRIVER64_H
#define PCI_DRIVER64_H

#include <linux/pci.h>
#include <linux/cdev.h>

//sturcutre built to hold the pci device data and more custom data. 
struct pci_mmap_dev {
    struct pci_dev *pdev;
    void __iomem *hw_addr;
    struct cdev cdev;
    dev_t devno;  // Device number
    size_t offset;
    struct list_head list;  // Linked list node
    struct mutex dev_lock;  // Mutex for protecting device-specific operations
    bool busy; // True when device has been opened by another process
};

typedef struct {
    off_t offset;
    uint64_t value;  // Changed to uint64_t for 64-bit data
} write_data_t;

#endif // PCI_DRIVER64_H