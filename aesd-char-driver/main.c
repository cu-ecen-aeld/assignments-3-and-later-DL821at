/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/mutex.h>
#include <linux/slab.h>    // kmalloc, kfree, krealloc
#include <linux/uaccess.h> // copy_to_user, copy_from_user
#include <linux/string.h>
#include "aesd-circular-buffer.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("DL821at"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

/* 
 *   struct mutex m;                     // for mutual exclusion
 *   struct aesd_circular_buffer buf;      // circular buffer state 
 *   struct aesd_buffer_entry entry;       // temporary incomplete write entry
 *
 * and that your header includes the prototypes for:
 *   aesd_circular_buffer_init(), 
 *   aesd_circular_buffer_add_entry(), 
 *   aesd_circular_buffer_find_entry_offset_for_fpos(), and 
 *   aesd_circular_buffer_deinit() (if you have one).
 */

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    filp->private_data = container_of(inode->i_cdev, struct aesd_dev, cdev);
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    size_t entry_offset = 0;
    struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry *entry = NULL;



    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */
    if (mutex_lock_interruptible(&dev->m))
        return -ERESTARTSYS;

    /* Use helper to find which circular buffer entry contains the requested f_pos */
    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->buf, *f_pos, &entry_offset);
    if (!entry) {
        /* No entry found: return 0 to indicate EOF */
        mutex_unlock(&dev->m);
        return 0;
    }

    /* Calculate the number of bytes available from this entry */
    if (entry->size - entry_offset > count)
        retval = count;
    else
        retval = entry->size - entry_offset;

    if (copy_to_user(buf, entry->buffptr + entry_offset, retval)) {
        retval = -EFAULT;
        goto out;
    }
    *f_pos += retval;
out:
    mutex_unlock(&dev->m);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    struct aesd_dev *dev = filp->private_data;
    char *tmp = NULL;
    int i = 0;

    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */
    if (mutex_lock_interruptible(&dev->m))
        return -ERESTARTSYS;

    /* 
    * Accumulate the new data into the temporary incomplete write entry.
    * If this is the first write, allocate a new buffer;
    * otherwise, reallocate to extend the existing buffer.
     */
    if (dev->entry.size == 0) {
        dev->entry.buffptr = kmalloc(count, GFP_KERNEL);
        if (!dev->entry.buffptr) {
           retval = -ENOMEM;
            goto out;
        }
    } else {
        tmp = krealloc(dev->entry.buffptr, dev->entry.size + count, GFP_KERNEL);
        if (!tmp) {
            retval = -ENOMEM;
            goto out;
        }
        dev->entry.buffptr = tmp;
    }
    /* Copy the data from user into the temporary buffer */
    if (copy_from_user(dev->entry.buffptr + dev->entry.size, buf, count)) {
      retval = -EFAULT;
        goto out;
    }
    dev->entry.size += count;
    retval = count;

    /* Process the temporary buffer for any complete write commands (ending with '\n') */
    for (i = 0; i < dev->entry.size; i++) {
        if (dev->entry.buffptr[i] == '\n') {
            /* A complete command is ready: add it to the circular buffer */
            aesd_circular_buffer_add_entry(&dev->buf, &dev->entry);
            /* Reset the temporary entry for future writes */
            dev->entry.buffptr = NULL;
            dev->entry.size = 0;
            break; /* Process one complete command at a time */
        }
    }
    // *f_pos is not used for the device in this implementation
out:
    mutex_unlock(&dev->m);
    return retval;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    /* Initialize the mutex, the circular buffer and the temporary entry */
    mutex_init(&aesd_device.m);
    aesd_circular_buffer_init(&aesd_device.buf);
    /* Initialize the temporary write entry to empty */
    aesd_device.entry.buffptr = NULL;
    aesd_device.entry.size = 0;

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
    /* Free any remaining memory in the temporary write entry */
    if (aesd_device.entry.buffptr) {
        kfree(aesd_device.entry.buffptr);
        aesd_device.entry.buffptr = NULL;
        aesd_device.entry.size = 0;
    }
    /* Cleanup the circular buffer; assume aesd_circular_buffer_deinit frees allocated entries */
    aesd_circular_buffer_deinit(&aesd_device.buf);



    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
