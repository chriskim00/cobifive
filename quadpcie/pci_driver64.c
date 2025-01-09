#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/slab.h>  // For kmalloc and kfree

#include <linux/fs.h>
#include <linux/file.h>
#include <linux/err.h>
#include <linux/uaccess.h>
#include <linux/timekeeping.h>

#define DRIVER_NAME "cobi_chip_driver64"
#define DEVICE_NAME "cobi_pcie_card"
#define BAR0 0

//timing flags that can be set to 1 to enable time logging for the specific operation
#define timing_flags 0b0001 //bits 3 - 0 data offset timing, mem read timing, pcie write timing, total timing

static int major;
static struct class *pci_mmap_class;

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

static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);
static DEFINE_MUTEX(open_lock);

// Function to log time to a file that is stored in /var/log/pci_driver.log
static void log_time(const char *message, const s64 *time_elapsed) {
    struct file *file;
    loff_t pos = 0;
    char *buf;
    size_t len;
    const char *filename;

    buf = kmalloc(256, GFP_KERNEL);
    if (!buf)
        return;

    snprintf(buf, 256, "%s: %llu\n", message, *time_elapsed);
    len = strlen(buf);

    filename = "/tmp/pci_driver.log";
    file = filp_open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (IS_ERR(file)) {
        kfree(buf);
        printk(KERN_ERR "Failed to open log file2: %ld\n", PTR_ERR(file));

        return;
    }

    kernel_write(file, buf, len, &pos);

    filp_close(file, NULL);
    kfree(buf);
}

static int pci_mmap_open(struct inode *inode, struct file *file)
{
    int ret = 0;
    struct pci_mmap_dev *dev = container_of(inode->i_cdev, struct pci_mmap_dev, cdev);
    file->private_data = dev;

    mutex_lock(&open_lock);
    if (dev->busy) {
        ret = -EBUSY;
    } else {
        // claim device
        dev->busy = true;
    }
    mutex_unlock(&open_lock);

    return ret;
}

static int pci_mmap_release(struct inode *inode, struct file *file)
{
    struct pci_mmap_dev *dev = (struct pci_mmap_dev *) file->private_data;
    dev->busy = false;
    return 0;
}

static ssize_t pci_mmap_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    struct pci_mmap_dev *dev = file->private_data;
    u32 data;
    struct timespec64 start, end;


    mutex_lock(&dev->dev_lock);  // Lock the device for this operation

    if (dev->offset >= pci_resource_len(dev->pdev, BAR0)) {
        mutex_unlock(&dev->dev_lock);  // Unlock if there's an error
        return 0;
    }

    //start trackign the time of an individual read operation
    ktime_get_real_ts64(&start);

    data = ioread32(dev->hw_addr + dev->offset);
    
    ktime_get_real_ts64(&end);//end recording read time

    data = ((data & 0xFF000000) >> 24) |
           ((data & 0x00FF0000) >> 8) |
           ((data & 0x0000FF00) << 8) |
           ((data & 0x000000FF) << 24);

    if (copy_to_user(buf, &data, sizeof(data))) {
        mutex_unlock(&dev->dev_lock);  // Unlock if there's an error
        return -EFAULT;
    }

    //end tracking the time of an individual read operation and log the read time
    s64 time_elapsed = timespec64_to_ns(&end) - timespec64_to_ns(&start);
    log_time("Read operation completed", &time_elapsed);

    mutex_unlock(&dev->dev_lock);  // Unlock after the operation
    return sizeof(data);
}

static ssize_t pci_mmap_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    //time stamps for the write time
    struct timespec64 start, end;
    struct timespec64 total_start, total_end;
    s64 pcie_write_average = 0;
    s64 mem_read_average = 0;
    s64 data_offset_average = 0;
    s64 time_elapsed;

    struct pci_mmap_dev *dev = file->private_data;
    write_data_t write_data;
    off_t read_offset;

    // Check if the input count matches the size of an off_t, indicating it's likely a read offset update
    if (count == sizeof(off_t)) {
        if (copy_from_user(&read_offset, buf, sizeof(off_t))) {
            return -EFAULT;
        }

        // Validate the read offset against the MMIO region's length
        if (read_offset >= pci_resource_len(dev->pdev, BAR0)) {
            return -EINVAL;  // Return error if the offset is out of range
        }

        // Update the device's offset for the next read
        dev->offset = read_offset;
        //    printk(KERN_INFO "Read offset set to %lld\n", (long long)read_offset);
        return sizeof(off_t);
    }



    // Otherwise, handle as a write_data_t structure for data writes
    if (count == sizeof(write_data_t) || count % sizeof(write_data_t) == 0) {
        mutex_lock(&dev->dev_lock);  // Lock only once per bulk write operation
        
        //start tracking the total time for the write operation
        if (timing_flags & 0b0001) {
            ktime_get_real_ts64(&total_start);
        }

        size_t data_count = count / sizeof(write_data_t);  // Determine number of write_data_t structures
        for (size_t i = 0; i < data_count; i++) {
            // Copy each chunk of data

            //start tracking the time of a single memory transfer
            if (timing_flags & 0b0100) {
                ktime_get_real_ts64(&start);//start time stamp for write
            }   

            if (copy_from_user(&write_data, buf + (i * sizeof(write_data_t)), sizeof(write_data_t))) {
                mutex_unlock(&dev->dev_lock);  // Unlock on error
                return -EFAULT;
            }
            
            //end tracking the time of a single memory transfer and log the data
            if (timing_flags & 0b0100) {
                ktime_get_real_ts64(&end);        //end time stamp for write
                time_elapsed = timespec64_to_ns(&end) - timespec64_to_ns(&start);
                mem_read_average += time_elapsed;
                log_time("Copy data from user complete", &time_elapsed);
            }

            //start tracking the time of a single offset write
            if (timing_flags & 0b1000) {
                ktime_get_real_ts64(&start);//start time stamp for write
            }

            // Validate the offset to prevent access outside the MMIO region
            if (write_data.offset >= pci_resource_len(dev->pdev, BAR0)) {
                mutex_unlock(&dev->dev_lock);  // Unlock on error
                return -EINVAL;
            }

            //end tracking the time of a single offset write and log the data
            if (timing_flags & 0b1000) {
                ktime_get_real_ts64(&end);        //end time stamp for write
                time_elapsed = timespec64_to_ns(&end) - timespec64_to_ns(&start);
                data_offset_average += time_elapsed;
                log_time( "Offset writing completed", &time_elapsed);
            }

            //start tracking the time of a single pcie write
            if (timing_flags & 0b0010) {
                ktime_get_real_ts64(&start);//start time stamp for write
            }

            // Perform a single MMIO write
            writeq(write_data.value, dev->hw_addr + write_data.offset);

            //end tracking the time of a single pcie write and log the data
            if (timing_flags & 0b0010) {
                ktime_get_real_ts64(&end);        //end time stamp for write
                time_elapsed = timespec64_to_ns(&end) - timespec64_to_ns(&start);
                pcie_write_average += time_elapsed;
                log_time("PCI Write completed", &time_elapsed);
            }


        }

        //end tracking the total time of the write operation
        if (timing_flags & 0b0001) {
            ktime_get_real_ts64(&total_end);
            time_elapsed = timespec64_to_ns(&total_end) - timespec64_to_ns(&total_start);
            log_time("Total Write completed", &time_elapsed);
        }
        mutex_unlock(&dev->dev_lock);  // Release lock after bulk write operation

        //log the average time of each memory transfer if the flags are set
        if (timing_flags & 0b0100) {
            time_elapsed = mem_read_average / data_count;
            log_time("Copy data from user average completed", &time_elapsed);
        }

        //log the average time of each offset write if the flags are set
        if (timing_flags & 0b1000) {
            time_elapsed = data_offset_average / data_count;
            log_time("Offset writing average completed", &time_elapsed);
        }

        //log the average time of each pci write if the flags are set
        if (timing_flags & 0b0010) {
            time_elapsed = pcie_write_average / data_count;
            log_time("PCI Write average completed", &time_elapsed);
        }


        return count;  // Return the full count to indicate successful batch write
    }

    return -EINVAL;  // If not a valid batch size, return error
}

static struct file_operations pci_mmap_fops = {
    .owner = THIS_MODULE,
    .open = pci_mmap_open,
    .release = pci_mmap_release,
    .read = pci_mmap_read,
    .write = pci_mmap_write,
};

static struct pci_device_id pci_mmap_id_table[] = {
    { PCI_DEVICE(0x800E, 0x7011) },
    { 0, }
};
MODULE_DEVICE_TABLE(pci, pci_mmap_id_table);

static int pci_mmap_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    int err;
    resource_size_t mmio_start, mmio_len;
    struct pci_mmap_dev *dev;
    static int device_count = 0;

    dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;

    err = pci_enable_device(pdev);
    if (err) {
        dev_err(&pdev->dev, "Failed to enable PCI device\n");
        return err;
    }

    err = pci_request_regions(pdev, DRIVER_NAME);
    if (err) {
        dev_err(&pdev->dev, "Failed to request PCI regions\n");
        goto disable_device;
    }

    mmio_start = pci_resource_start(pdev, BAR0);
    mmio_len = pci_resource_len(pdev, BAR0);
    dev->hw_addr = ioremap(mmio_start, mmio_len);
    if (!dev->hw_addr) {
        dev_err(&pdev->dev, "Failed to map MMIO region\n");
        err = -ENOMEM;
        goto release_regions;
    }

    err = alloc_chrdev_region(&dev->devno, device_count, 1, DEVICE_NAME);
    if (err) {
        dev_err(&pdev->dev, "Failed to allocate char device region\n");
        goto unmap_region;
    }
    major = MAJOR(dev->devno);

    if (!pci_mmap_class) {
        pci_mmap_class = class_create(DEVICE_NAME);
        if (IS_ERR(pci_mmap_class)) {
            err = PTR_ERR(pci_mmap_class);
            goto unregister_chrdev;
        }
    }

    cdev_init(&dev->cdev, &pci_mmap_fops);
    dev->cdev.owner = THIS_MODULE;
    err = cdev_add(&dev->cdev, dev->devno, 1);
    if (err) {
        dev_err(&pdev->dev, "Failed to add char device\n");
        goto destroy_class;
    }

    //    printk(KERN_INFO "Creating device file /dev/%s_%x_%x\n", DEVICE_NAME, pdev->bus->number, PCI_SLOT(pdev->devfn));
    device_create(pci_mmap_class, NULL, dev->devno, NULL, DEVICE_NAME "%d", device_count);
    dev_info(&pdev->dev, "Created device file: /dev/%s%d\n", DEVICE_NAME, device_count);  // Print device file name

    dev->pdev = pdev;
    dev->offset = 0; // Initialize the offset
    mutex_init(&dev->dev_lock);  // Initialize the mutex lock for this device
    pci_set_drvdata(pdev, dev);

    mutex_lock(&device_list_lock);
    list_add_tail(&dev->list, &device_list);
    mutex_unlock(&device_list_lock);

    dev_info(&pdev->dev, "PCI device %s probed\n", DRIVER_NAME);

    device_count++;
    return 0;

destroy_class:
    if (device_count == 0) {
        class_destroy(pci_mmap_class);
    }
unregister_chrdev:
    unregister_chrdev_region(dev->devno, 1);
unmap_region:
    iounmap(dev->hw_addr);
release_regions:
    pci_release_regions(pdev);
disable_device:
    pci_disable_device(pdev);
    return err;
}

static void pci_mmap_remove(struct pci_dev *pdev)
{
    struct pci_mmap_dev *dev = pci_get_drvdata(pdev);

    if (dev) {  // Check if dev is not NULL
        device_destroy(pci_mmap_class, dev->devno);
        cdev_del(&dev->cdev);
        unregister_chrdev_region(dev->devno, 1);

        if (dev->hw_addr) {
            iounmap(dev->hw_addr);
        }

        pci_release_regions(pdev);
        pci_disable_device(pdev);

        mutex_lock(&device_list_lock);
        list_del(&dev->list);
        mutex_unlock(&device_list_lock);

        kfree(dev);

        if (list_empty(&device_list) && pci_mmap_class) {
            class_destroy(pci_mmap_class);
            pci_mmap_class = NULL;
        }
    }

    dev_info(&pdev->dev, "PCI device %s removed\n", DRIVER_NAME);
}

static struct pci_driver pci_mmap_driver = {
    .name = DRIVER_NAME,
    .id_table = pci_mmap_id_table,
    .probe = pci_mmap_probe,
    .remove = pci_mmap_remove,
};

static int __init pci_mmap_init(void)
{
    int err;

    err = pci_register_driver(&pci_mmap_driver);
    if (err)
        return err;

    //    printk(KERN_INFO "%s driver registered\n", DRIVER_NAME);
    return 0;
}

static void __exit pci_mmap_exit(void)
{
    pci_unregister_driver(&pci_mmap_driver);
    //    printk(KERN_INFO "%s driver unregistered\n", DRIVER_NAME);
}

module_init(pci_mmap_init);
module_exit(pci_mmap_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("PCI driver for 64-bit data handling");
