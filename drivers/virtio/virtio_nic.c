/*
 * Randomness driver for virtio
 *  Copyright (C) 2007, 2008 Rusty Russell IBM Corporation
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/hw_random.h>
#include <linux/scatterlist.h>
#include <linux/spinlock.h>
#include <linux/virtio.h>
#include <linux/virtio_nic.h>
#include <linux/module.h>
#include <linux/init.h>           // Macros used to mark up functions e.g. __init __exit
#include <linux/device.h>         // Header to support the kernel Driver Model
#include <linux/fs.h>             // Header for the Linux file system support
#include <linux/uaccess.h>          // Required for the copy to user function
#define  DEVICE_NAME "nicdevice"
#define  CLASS_NAME  "nic"

struct virtnic_info {
	char name[25];
	struct completion have_data;
	bool busy;
	struct virtqueue *vq;
	bool nic_register_done;
	bool nic_removed;
};
struct virtnic_info *vn = NULL;
static int    majorNumber;                  ///< Stores the device number -- determined automatically
static char   message[256] = {0};           ///< Memory for the string that is passed from userspace
static short  size_of_message;              ///< Used to remember the size of the string stored
static int    numberOpens = 0;              ///< Counts the number of times the device is opened
static struct class*  ebbcharClass  = NULL; ///< The device-driver class struct pointer
static struct device* ebbcharDevice = NULL; ///< The device-driver device struct pointer
 
// The prototype functions for the character driver -- must come before the struct definition
static int     dev_open(struct inode *, struct file *);
static int     dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);
 
/** @brief Devices are represented as file structure in the kernel. The file_operations structure from
 *  /linux/fs.h lists the callback functions that you wish to associated with your file operations
 *  using a C99 syntax structure. char devices usually implement open, read, write and release calls
 */
static struct file_operations fops =
{
   .open = dev_open,
   .read = dev_read,
   .write = dev_write,
   .release = dev_release,
};
static int ebbchar_init(void){
   printk(KERN_INFO "EBBChar: Initializing the EBBChar LKM\n");
 
   // Try to dynamically allocate a major number for the device -- more difficult but worth it
   majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
   if (majorNumber<0){
      printk(KERN_ALERT "EBBChar failed to register a major number\n");
      return majorNumber;
   }
   printk(KERN_INFO "EBBChar: registered correctly with major number %d\n", majorNumber);
 
   // Register the device class
   ebbcharClass = class_create(THIS_MODULE, CLASS_NAME);
   if (IS_ERR(ebbcharClass)){                // Check for error and clean up if there is
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ALERT "Failed to register device class\n");
      return PTR_ERR(ebbcharClass);          // Correct way to return an error on a pointer
   }
   printk(KERN_INFO "EBBChar: device class registered correctly\n");
 
   // Register the device driver
   ebbcharDevice = device_create(ebbcharClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
   if (IS_ERR(ebbcharDevice)){               // Clean up if there is an error
      class_destroy(ebbcharClass);           // Repeated code but the alternative is goto statements
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ALERT "Failed to create the device\n");
      return PTR_ERR(ebbcharDevice);
   }
   printk(KERN_INFO "EBBChar: device class created correctly\n"); // Made it! device was initialized
   return 0;
}
 
/** @brief The LKM cleanup function
 *  Similar to the initialization function, it is static. The __exit macro notifies that if this
 *  code is used for a built-in driver (not a LKM) that this function is not required.
 */
static void ebbchar_exit(void){
   device_destroy(ebbcharClass, MKDEV(majorNumber, 0));     // remove the device
   class_unregister(ebbcharClass);                          // unregister the device class
   class_destroy(ebbcharClass);                             // remove the device class
   unregister_chrdev(majorNumber, DEVICE_NAME);             // unregister the major number
   printk(KERN_INFO "EBBChar: Goodbye from the LKM!\n");
}
 
/** @brief The device open function that is called each time the device is opened
 *  This will only increment the numberOpens counter in this case.
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_open(struct inode *inodep, struct file *filep){
   numberOpens++;
   printk(KERN_INFO "EBBChar: Device has been opened %d time(s)\n", numberOpens);
   return 0;
}
 
/** @brief This function is called whenever device is being read from user space i.e. data is
 *  being sent from the device to the user. In this case is uses the copy_to_user() function to
 *  send the buffer string to the user and captures any errors.
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 *  @param buffer The pointer to the buffer to which this function writes the data
 *  @param len The length of the b
 *  @param offset The offset if required
 */
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
   int error_count = 0;
   // copy_to_user has the format ( * to, *from, size) and returns 0 on success
   error_count = copy_to_user(buffer, message, size_of_message);
 
   if (error_count==0){            // if true then have success
      printk(KERN_INFO "EBBChar: Sent %d characters to the user\n", size_of_message);
      return (size_of_message=0);  // clear the position to the start and return 0
   }
   else {
      printk(KERN_INFO "EBBChar: Failed to send %d characters to the user\n", error_count);
      return -EFAULT;              // Failed -- return a bad address message (i.e. -14)
   }
}
static void register_buffer(u8 *buf, size_t size)
{
	struct scatterlist sg;

	sg_init_one(&sg, buf, size);

	/* There should always be room for one buffer. */
	virtqueue_add_outbuf(vn->vq, &sg, 1, buf, GFP_KERNEL);

	if(virtqueue_kick(vn->vq)){
		printk(KERN_INFO "nic kick passed");
	}
} 
/** @brief This function is called whenever the device is being written to from user space i.e.
 *  data is sent to the device from the user. The data is copied to the message[] array in this
 *  LKM using the sprintf() function along with the length of the string.
 *  @param filep A pointer to a file object
 *  @param buffer The buffer to that contains the string to write to the device
 *  @param len The length of the array of data that is being passed in the const char buffer
 *  @param offset The offset if required
 */
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){
   sprintf(message, "%s(%zu letters)", buffer, len);   // appending received string with its length
   size_of_message = strlen(message);                 // store the length of the stored message
   printk(KERN_INFO "EBBChar: Received %zu characters from the user\n", len);
   register_buffer(buffer, len);
   return len;
}
 
/** @brief The device release function that is called whenever the device is closed/released by
 *  the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_release(struct inode *inodep, struct file *filep){
   printk(KERN_INFO "EBBChar: Device successfully closed\n");
   return 0;
}

// static void random_recv_done(struct virtqueue *vq)
// {
// 	struct virtrng_info *vi = vq->vdev->priv;

// 	/* We can get spurious callbacks, e.g. shared IRQs + virtio_pci. */
// 	if (!virtqueue_get_buf(vi->vq, &vi->data_avail))
// 		return;

// 	complete(&vi->have_data);
// }

/* The host will fill any buffer we give it with sweet, sweet randomness. */
// static void register_buffer(struct virtrng_info *vi, u8 *buf, size_t size)
// {
// 	struct scatterlist sg;

// 	sg_init_one(&sg, buf, size);

// 	/* There should always be room for one buffer. */
// 	virtqueue_add_inbuf(vi->vq, &sg, 1, buf, GFP_KERNEL);

// 	virtqueue_kick(vi->vq);
// }

// static int virtio_read(struct hwrng *rng, void *buf, size_t size, bool wait)
// {
// 	int ret;
// 	struct virtrng_info *vi = (struct virtrng_info *)rng->priv;

// 	if (vi->hwrng_removed)
// 		return -ENODEV;

// 	if (!vi->busy) {
// 		vi->busy = true;
// 		init_completion(&vi->have_data);
// 		register_buffer(vi, buf, size);
// 	}

// 	if (!wait)
// 		return 0;

// 	ret = wait_for_completion_killable(&vi->have_data);
// 	if (ret < 0)
// 		return ret;

// 	vi->busy = false;

// 	return vi->data_avail;
// }

// static void virtio_cleanup(struct hwrng *rng)
// {
// 	struct virtrng_info *vi = (struct virtrng_info *)rng->priv;

// 	if (vi->busy)
// 		wait_for_completion(&vi->have_data);
// }

static void handle_input(struct virtqueue *vq){
	// TO DO
}
static int virtnic_probe(struct virtio_device *vdev)
{
	printk(KERN_INFO "nic probing started!");
	

	vn = kzalloc(sizeof(struct virtnic_info), GFP_KERNEL);
	if (!vn)
		return -ENOMEM;
	ebbchar_init();

	sprintf(vn->name, "virtio_nic");
	init_completion(&vn->have_data);
	vdev->priv = vn;
	vn->vq = virtio_find_single_vq(vdev, handle_input, "input");
	if (IS_ERR(vn->vq)) {
		// err = PTR_ERR(vi->vq);
		// goto err_find;
		printk(KERN_INFO "nic.c: error in finding virtqueue\n");
	}
	if(virtqueue_kick(vn->vq)){
		printk(KERN_INFO "nic kick passed");
	}
	printk(KERN_INFO "nic probing ended!");
	return 0;
}

static void virtnic_remove(struct virtio_device *vdev)
{
	struct virtnic_info *vn = vdev->priv; // private pointer of driver
	complete(&vn->have_data);
	vdev->config->reset(vdev);
	vn->busy = false;
	// vdev->config->del_vqs(vdev); delete the defined virtqueues
	kfree(vn);
}

static void virtnic_scan(struct virtio_device *vdev)
{
	// struct virtnic_info *vn = vdev->priv;
	// int err;
}


/*
Code related to device going to sleep, we will deal with this later, once
basic setup is done
*/

// #ifdef CONFIG_PM_SLEEP
// static int virtrng_freeze(struct virtio_device *vdev)
// {
// 	remove_common(vdev);
// 	return 0;
// }

// static int virtrng_restore(struct virtio_device *vdev)
// {
// 	int err;

// 	err = probe_common(vdev);
// 	if (!err) {
// 		struct virtrng_info *vi = vdev->priv;

// 		/*
// 		 * Set hwrng_removed to ensure that virtio_read()
// 		 * does not block waiting for data before the
// 		 * registration is complete.
// 		 */
// 		vi->hwrng_removed = true;
// 		err = hwrng_register(&vi->hwrng);
// 		if (!err) {
// 			vi->hwrng_register_done = true;
// 			vi->hwrng_removed = false;
// 		}
// 	}

// 	return err;
// }
// #endif

static struct virtio_device_id id_table[] = {
	{ VIRTIO_ID_NIC, VIRTIO_DEV_ANY_ID },
	{ 0 },
};

static struct virtio_driver virtio_nic_driver = {
	.driver.name =	KBUILD_MODNAME,
	.driver.owner =	THIS_MODULE,
	.id_table =	id_table,
	.probe =	virtnic_probe,
	.remove =	virtnic_remove,
	.scan =		virtnic_scan,
// #ifdef CONFIG_PM_SLEEP
// 	.freeze =	virtrng_freeze,
// 	.restore =	virtrng_restore,
// #endif
};

module_virtio_driver(virtio_nic_driver);
MODULE_DEVICE_TABLE(virtio, id_table);
MODULE_DESCRIPTION("Virtio nic driver");
MODULE_LICENSE("GPL");
  

