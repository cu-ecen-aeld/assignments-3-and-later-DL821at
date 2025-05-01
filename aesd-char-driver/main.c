/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * Author: Dan Walkes
 * Date: 2019-10-22
 * Copyright (c) 2019
 *
 */

 #include <linux/module.h>
 #include <linux/init.h>
 #include <linux/printk.h>
 #include <linux/types.h>
 #include <linux/cdev.h>
 #include <linux/fs.h>          // file_operations, llseek, unlocked_ioctl
 #include <linux/mutex.h>
 #include <linux/slab.h>        // kmalloc, kfree, krealloc
 #include <linux/uaccess.h>     // copy_to_user, copy_from_user
 #include <linux/string.h>
 #include "aesd-circular-buffer.h"
 #include "aesdchar.h"          // struct aesd_dev
 #include "aesd_ioctl.h"        // <<< added for IOCTL
 
 // Add a simple debug macro to prevent PDEBUG implicit declaration error
 #ifndef PDEBUG
 #define PDEBUG(fmt, ...) printk(KERN_DEBUG fmt, ##__VA_ARGS__)
 #endif
 
 int aesd_major = 0; // use dynamic major
 int aesd_minor = 0;
 
 MODULE_AUTHOR("DL821at");
 MODULE_LICENSE("Dual BSD/GPL");
 
 /* 
  * struct aesd_dev {
  *     struct cdev cdev;
  *     struct mutex m;
  *     struct aesd_circular_buffer buf;
  *     struct aesd_buffer_entry entry;
  * };
  */
 static struct aesd_dev aesd_device;
 
 /**
  * Custom llseek implementation.
  */
 static loff_t aesd_llseek(struct file *filp, loff_t offset, int whence)
 {
     struct aesd_dev *dev = filp->private_data;
     loff_t newpos;
     size_t total_size = 0;
     size_t entry_count, i;
     uint8_t index;
 
     if (mutex_lock_interruptible(&dev->m))
         return -ERESTARTSYS;
 
     if (dev->buf.full) {
         entry_count = AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
     } else {
         entry_count = (dev->buf.in_offs + AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED
                        - dev->buf.out_offs) %
                       AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
     }
 
     index = dev->buf.out_offs;
     for (i = 0; i < entry_count; i++) {
         total_size += dev->buf.entry[index].size;
         index = (index + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
     }
 
     switch (whence) {
     case SEEK_SET:
         newpos = offset;
         break;
     case SEEK_CUR:
         newpos = filp->f_pos + offset;
         break;
     case SEEK_END:
         newpos = total_size + offset;
         break;
     default:
         mutex_unlock(&dev->m);
         return -EINVAL;
     }
 
     if (newpos < 0 || newpos > total_size) {
         mutex_unlock(&dev->m);
         return -EINVAL;
     }
 
     filp->f_pos = newpos;
     mutex_unlock(&dev->m);
     return newpos;
 }
 
 int aesd_open(struct inode *inode, struct file *filp)
 {
     PDEBUG("open");
     filp->private_data = container_of(inode->i_cdev, struct aesd_dev, cdev);
     return 0;
 }
 
 int aesd_release(struct inode *inode, struct file *filp)
 {
     PDEBUG("release");
     return 0;
 }
 
 ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
 {
     ssize_t retval = 0;
     size_t entry_offset = 0;
     struct aesd_dev *dev = filp->private_data;
     struct aesd_buffer_entry *entry = NULL;
 
     PDEBUG("read %zu bytes with offset %lld", count, *f_pos);
     if (mutex_lock_interruptible(&dev->m))
         return -ERESTARTSYS;
 
     entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->buf, *f_pos, &entry_offset);
     if (!entry) {
         mutex_unlock(&dev->m);
         return 0;
     }
 
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
 
 ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
 {
     ssize_t retval = -ENOMEM;
     struct aesd_dev *dev = filp->private_data;
     char *tmp = NULL;
     int i;
 
     PDEBUG("write %zu bytes with offset %lld", count, *f_pos);
     if (mutex_lock_interruptible(&dev->m))
         return -ERESTARTSYS;
 
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
 
     if (copy_from_user(dev->entry.buffptr + dev->entry.size, buf, count)) {
         retval = -EFAULT;
         goto out;
     }
     dev->entry.size += count;
     retval = count;
 
     for (i = 0; i < dev->entry.size; i++) {
         if (dev->entry.buffptr[i] == '\n') {
             aesd_circular_buffer_add_entry(&dev->buf, &dev->entry);
             dev->entry.buffptr = NULL;
             dev->entry.size = 0;
             break;
         }
     }
 out:
     mutex_unlock(&dev->m);
     return retval;
 }
 
 /**
  * IOCTL handler for AESDCHAR_IOCSEEKTO
  */
 static long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
 {
     struct aesd_dev *dev = filp->private_data;
     struct aesd_seekto seekto;
     size_t entry_count, i;
     uint8_t index;
     struct aesd_buffer_entry *entry;
     loff_t newpos = 0;
     size_t offset = 0;
     int ret = 0;
 
     if (cmd != AESDCHAR_IOCSEEKTO)
         return -ENOTTY;
 
     if (copy_from_user(&seekto, (const void __user *)arg, sizeof(seekto)))
         return -EFAULT;
 
     if (mutex_lock_interruptible(&dev->m))
         return -ERESTARTSYS;
 
     if (dev->buf.full) {
         entry_count = AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
     } else {
         entry_count = (dev->buf.in_offs + AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED
                        - dev->buf.out_offs) %
                       AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
     }
 
     if (seekto.write_cmd >= entry_count) {
         ret = -EINVAL;
         goto ioctl_out;
     }
 
     index = dev->buf.out_offs;
     for (i = 0; i < seekto.write_cmd; i++) {
         entry = &dev->buf.entry[index];
         offset += entry->size;
         index = (index + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
     }
     entry = &dev->buf.entry[index];
 
     if (seekto.write_cmd_offset >= entry->size) {
         ret = -EINVAL;
         goto ioctl_out;
     }
     offset += seekto.write_cmd_offset;
     newpos = offset;
     filp->f_pos = newpos;
 
 ioctl_out:
     mutex_unlock(&dev->m);
     return ret;
 }
 
 struct file_operations aesd_fops = {
     .owner          = THIS_MODULE,
     .llseek         = aesd_llseek,
     .read           = aesd_read,
     .write          = aesd_write,
     .unlocked_ioctl = aesd_ioctl,   // <<< added
     .open           = aesd_open,
     .release        = aesd_release,
 };
 
 static int aesd_setup_cdev(struct aesd_dev *dev)
 {
     int err, devno = MKDEV(aesd_major, aesd_minor);
 
     cdev_init(&dev->cdev, &aesd_fops);
     dev->cdev.owner = THIS_MODULE;
     dev->cdev.ops   = &aesd_fops;
     err = cdev_add(&dev->cdev, devno, 1);
     if (err) {
         printk(KERN_ERR "Error %d adding aesd cdev", err);
     }
     return err;
 }
 
 int aesd_init_module(void)
 {
     dev_t dev = 0;
     int result;
 
     result = alloc_chrdev_region(&dev, aesd_minor, 1, "aesdchar");
     aesd_major = MAJOR(dev);
     if (result < 0) {
         printk(KERN_WARNING "Can't get major %d\n", aesd_major);
         return result;
     }
     memset(&aesd_device, 0, sizeof(struct aesd_dev));
 
     mutex_init(&aesd_device.m);
     aesd_circular_buffer_init(&aesd_device.buf);
     aesd_device.entry.buffptr = NULL;
     aesd_device.entry.size    = 0;
 
     result = aesd_setup_cdev(&aesd_device);
     if (result) {
         unregister_chrdev_region(dev, 1);
     }
     return result;
 }
 
 void aesd_cleanup_module(void)
 {
     dev_t devno = MKDEV(aesd_major, aesd_minor);
 
     cdev_del(&aesd_device.cdev);
 
     if (aesd_device.entry.buffptr) {
         kfree(aesd_device.entry.buffptr);
     }
     aesd_circular_buffer_deinit(&aesd_device.buf);
     unregister_chrdev_region(devno, 1);
 }
 
 module_init(aesd_init_module);
 module_exit(aesd_cleanup_module);
 