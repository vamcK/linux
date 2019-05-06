#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/hw_random.h>
#include <linux/scatterlist.h>
#include <linux/spinlock.h>
#include <linux/virtio.h>
#include <linux/virtio_nic.h>
#include <linux/module.h>
#include <linux/init.h>           
#include <linux/device.h>         
#include <linux/fs.h>             
#include <linux/uaccess.h>        
#define  DEVICE_NAME "nicdevice"
#define  CLASS_NAME  "nic"

struct tosend {
   int a;
   char str[10];
};

struct torecieve {
   int a;
   char str[10];
};

struct virtnic_info {
	char name[25];
	struct completion have_data;
	bool busy;
	struct virtqueue *vq;
	bool nic_register_done;
	bool nic_removed;
   struct tosend var;
   struct torecieve data;
};

struct virtnic_info *vn = NULL;
static int    majorNumber;                  ///< Stores the device number -- determined automatically
static char   message[256] = {0};           ///< Memory for the string that is passed from userspace
static short  size_of_message;              ///< Used to remember the size of the string stored
static int    numberOpens = 0;              ///< Counts the number of times the device is opened
static struct class*  charDeviceClass  = NULL; ///< The device-driver class struct pointer
static struct device* charDevice = NULL; ///< The device-driver device struct pointer

static int     dev_open(struct inode *, struct file *);
static int     dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations fops =
{
   .open = dev_open,
   .read = dev_read,
   .write = dev_write,
   .release = dev_release,
};
static int nicDevice_init(void){
   printk(KERN_INFO "NicDevice: Initializing the NicDevice LKM\n");
 
   // Try to dynamically allocate a major number for the device -- more difficult but worth it
   majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
   if (majorNumber<0){
      printk(KERN_ALERT "NicDevice failed to register a major number\n");
      return majorNumber;
   }
   printk(KERN_INFO "NicDevice: registered correctly with major number %d\n", majorNumber);
 
   // Register the device class
   charDeviceClass = class_create(THIS_MODULE, CLASS_NAME);
   if (IS_ERR(charDeviceClass)){                // Check for error and clean up if there is
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ALERT "NicDevice: Failed to register device class\n");
      return PTR_ERR(charDeviceClass);          // Correct way to return an error on a pointer
   }
   printk(KERN_INFO "NicDevice: device class registered correctly\n");
 
   // Register the device driver
   charDevice = device_create(charDeviceClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
   if (IS_ERR(charDevice)){               // Clean up if there is an error
      class_destroy(charDeviceClass);           // Repeated code but the alternative is goto statements
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ALERT "NicDevice: Failed to create the device\n");
      return PTR_ERR(charDevice);
   }
   printk(KERN_INFO "NicDevice: device class created correctly\n"); // Made it! device was initialized
   return 0;
}

static void nicDevice_exit(void){
   device_destroy(charDeviceClass, MKDEV(majorNumber, 0));     // remove the device
   class_unregister(charDeviceClass);                          // unregister the device class
   class_destroy(charDeviceClass);                             // remove the device class
   unregister_chrdev(majorNumber, DEVICE_NAME);             // unregister the major number
   printk(KERN_INFO "NicDevice: Goodbye from the LKM!\n");
}

static int dev_open(struct inode *inodep, struct file *filep){
   numberOpens++;
   printk(KERN_INFO "NicDevice: Device has been opened %d time(s)\n", numberOpens);
   return 0;
}
 
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
   int error_count = 0;
   // copy_to_user has the format ( * to, *from, size) and returns 0 on success
   error_count = copy_to_user(buffer, message, size_of_message);
 
   if (error_count==0){            // if true then have success
      printk(KERN_INFO "NicDevice: Sent %d characters to the user\n", size_of_message);
      return (size_of_message=0);  // clear the position to the start and return 0
   }
   else {
      printk(KERN_INFO "NicDevice: Failed to send %d characters to the user\n", error_count);
      return -EFAULT;              // Failed -- return a bad address message (i.e. -14)
   }
}


static void register_buffer(u8 *buf, size_t size)
{
	// struct scatterlist sg1, sg2, *sgs[2];
   
   // unsigned int num_out = 0, num_in = 0;

	// sg_init_one(&sg1, buf, size);
   // sgs[num_out++] = &sg1;

   // sg_init_one(&sg2, buf, size);
   // sgs[num_out + num_in++] = &sg2;
	/* There should always be room for one buffer. */
	//virtqueue_add_outbuf(vn->vq, &sg, 1, buf, GFP_KERNEL);
   // virtqueue_add_sgs(vn->vq, sgs, num_out, num_in, buf, GFP_KERNEL);
   
   int err;
   struct scatterlist sg1, sg2, *sgs[2];
   unsigned int num_out = 0, num_in = 0;
   // sg_init_one(&sg, buf, size);
   // char *buf1;
   // buf1 = sg_virt(&sg);
   // printk(KERN_INFO "nic %s----buf1---",buf1);
   // virtqueue_add_outbuf(vn->vq, &sg, 1, buf, GFP_KERNEL);
   // virtqueue_add_outbuf(vn->vq, &sg, 1, buf, GFP_ATOMIC);

   vn->var.a=100;
   strcpy(vn->var.str, (char *)buf);
   sg_init_one(&sg1, &vn->var, sizeof(vn->var));
   sg_init_one(&sg2, &vn->data, sizeof(vn->data));
   sgs[num_out++]=&sg1;
   sgs[num_out + num_in++]=&sg2;
   // err=virtqueue_add_outbuf(vn->vq, sgs, 1, vn, GFP_ATOMIC);
   err=virtqueue_add_sgs(vn->vq, sgs, num_out, num_in, vn, GFP_ATOMIC);
   printk(KERN_INFO "NicDevice: return value of add_sgs-%d", err);

	if(virtqueue_kick(vn->vq)){
		printk(KERN_INFO "NicDevice: kick passed");
	}
} 

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){
   sprintf(message, "%s(%zu letters)", buffer, len);   // appending received string with its length
   size_of_message = strlen(message);                 // store the length of the stored message
   printk(KERN_INFO "NicDevice: Received %zu characters from the user\n", len);
   register_buffer(buffer, len);
   return len;
}
 
static int dev_release(struct inode *inodep, struct file *filep){
   printk(KERN_INFO "NicDevice: Device successfully closed\n");
   return 0;
}

static void handle_input(struct virtqueue *vq){
	// TO DO
   unsigned int len;
   struct torecieve *rcv_buf=NULL;
   printk(KERN_INFO "NicDevice: inside handle_input");
   // if (var=virtqueue_get_buf(vq, &len)!=NULL){
   //    printk(KERN_INFO "NicDevice: no buf in get buf");
	// 	return;
   // }
   rcv_buf = virtqueue_get_buf(vq, &len);
   printk(KERN_INFO "NicDevice: a = %d", rcv_buf->a);
   printk(KERN_INFO "NicDevice: str = %s", rcv_buf->str);
   printk(KERN_INFO "NicDevice: buf is there in get buf");
}
static int virtnic_probe(struct virtio_device *vdev)
{
	printk(KERN_INFO "NicDevice: probing started");

	vn = kzalloc(sizeof(struct virtnic_info), GFP_KERNEL);
	if (!vn)
		return -ENOMEM;
	nicDevice_init();

	sprintf(vn->name, "virtio_nic");
	init_completion(&vn->have_data);
	vdev->priv = vn;
	vn->vq = virtio_find_single_vq(vdev, handle_input, "input");
	if (IS_ERR(vn->vq)) {
		// err = PTR_ERR(vi->vq);
		// goto err_find;
		printk(KERN_INFO "NicDevice: error in finding virtqueue");
	}
	// if(virtqueue_kick(vn->vq)){
	// 	printk(KERN_INFO "nic kick passed");
	// }
   virtio_device_ready(vdev);
	printk(KERN_INFO "NicDevice: probing ended");
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
	// TO DO
}


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
};

module_virtio_driver(virtio_nic_driver);
MODULE_DEVICE_TABLE(virtio, id_table);
MODULE_DESCRIPTION("Virtio Nic driver");
MODULE_LICENSE("GPL");
