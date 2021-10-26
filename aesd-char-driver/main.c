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
#include <linux/slab.h>
#include <asm/uaccess.h>
#include "aesd-circular-buffer.h"
#include "aesdchar.h"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("SundarKrishnakumar"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
	PDEBUG("open");
	/**
	 * TODO: handle open
	 */
	struct aesd_dev *device = NULL;

	device = container_of(inode->i_cdev, struct aesd_dev, cdev);
	filp->private_data = device;

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
	ssize_t read_len = 0;
	size_t byte_offset = 0;
	int err;


	PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
	/**
	 * TODO: handle read
	 */

	struct aesd_dev *device = filp->private_data;

	// acquire the lock
	if ((err = mutex_lock_interruptible(&device->mutex)) != 0)
	{
		return err;
	}	

	struct aesd_buffer_entry *lc_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&device->c_buff, 
											*f_pos, &byte_offset);

	if (lc_entry != NULL)
	{
		if((count + byte_offset) <= lc_entry->size)
		{

			read_len = count;

		}
		else
		{
			read_len = lc_entry->size - byte_offset;
		}
	}
	else
	{

		// release the lock
		mutex_unlock(&device->mutex);		
		return 0;
	}

	if (copy_to_user(buf, (lc_entry->buffptr + byte_offset), read_len) != 0)
	{
		return -EFAULT;
	}

	*f_pos += read_len;

	// release lock
	mutex_unlock(&device->mutex);	

	return read_len;


}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{

	PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
	/**
	 * TODO: handle write 
	 */
	struct aesd_dev *device = NULL;
	int err;
	
	device = filp->private_data;

	// acquire the lock
	if ((err = mutex_lock_interruptible(&device->mutex)) != 0)
	{
		return err;
	}	

	if (!(device->tmp = kzalloc(sizeof(char) * count, GFP_KERNEL)))
	{
		return -ENOMEM;
	}

	if (copy_from_user(device->tmp, buf, count) != 0)
	{
		kfree(device->tmp);
		return -EFAULT;
	}

	// Note: In init module the aesd_device is init to zero
	// During the first entry of this method, device->entry.size = 0
	
	char *swap = device->entry.buffptr;
	if (!(device->entry.buffptr = kzalloc((sizeof(char) * (device->entry.size + count)), GFP_KERNEL)))
	{
		// if kzalloc fails reinstate the previous ptr value from swap
		device->entry.buffptr = swap;
		kfree(device->tmp);
		return -ENOMEM;
	}

	if (swap != NULL)
	{
		memcpy(device->entry.buffptr, swap, device->entry.size);
		kfree(swap);
	}	


	memcpy(device->entry.buffptr + device->entry.size, device->tmp, count);

	device->entry.size += count;

	if(strchr(device->tmp , '\n') != 0)
	{
		
		char *buff_to_release = aesd_circular_buffer_add_entry(&device->c_buff, &device->entry);

		// TODO: return value not null, then do free - overwrite condition
		if (buff_to_release != NULL)
		{
			kfree(buff_to_release);
		}
		// After receiving a complete packet, device->entry.size = 0
		device->entry.size = 0;
		device->entry.buffptr = NULL;


	}

	kfree(device->tmp);

	// release the lock
	mutex_unlock(&device->mutex);


	return count;
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
	PDEBUG("aesd_init_module()\n");

	dev_t dev = 0;
	int result;
	result = alloc_chrdev_region(&dev, aesd_minor, 1,
			"aesdchar");

	aesd_major = MAJOR(dev);

	if (result < 0) {
		printk(KERN_WARNING "Can't get major %d\n", aesd_major);
		return result;
	}

	// This inits c_buff to zero. Need not do aesd_circular_buffer_init()
	memset(&aesd_device,0,sizeof(struct aesd_dev));

	/**
	 * TODO: initialize the AESD specific portion of the device
	 */

	// initialize the mutex
	mutex_init(&aesd_device.mutex);	

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
	aesd_circular_buffer_cleanup(&aesd_device.c_buff);

	unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
