#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>

// TODO create special character device file from module code
// TODO allocate memory for buffer

#define DRIVER_NAME    "my_device_driver" 


struct my_deivce_data {
	struct cdev cdev;
	char* buffer;
	size_t size;
};

static unsigned int my_major   = 0;
static unsigned int num_of_dev = 1;
static struct my_deivce_data device_data;



#define __BUFFER_SIZE	1024

static char __buffer[__BUFFER_SIZE];

static int my_open(struct inode *inode, struct file *file)
{
	struct my_deivce_data* my_data;
	pr_info("open serial device\n");
	my_data = container_of(inode->i_cdev, struct my_deivce_data, cdev);
	my_data->buffer    = __buffer;
	my_data->size      = __BUFFER_SIZE;
	file->private_data = my_data;
	return 0;
}

static ssize_t my_write(struct file *file, const char __user *user_buffer,
					size_t size, loff_t * offset)
{
	return size;
	// struct my_deivce_data *my_data = (struct my_deivce_data *) file->private_data;
	// ssize_t len = min(my_data->size - (size_t)*offset, size);

	//if (len <= 0)
	//	return 0;

	//if (copy_from_user(my_data->buffer + *offset, user_buffer, len))
	//	return -EFAULT;

	//*offset += len;
	//return len;
}

static ssize_t my_read(struct file *file, char __user *user_buffer, size_t size, loff_t *offset)
{
	//my_data = (struct my_deivce_data *) file->private_data;
	return size;
}

static int my_release(struct inode* __inode, struct file* __file){
	pr_info("release serial device\n");
	return 0;
}

const struct file_operations my_fops = {
	.owner   = THIS_MODULE,
	.open    = my_open,
	.read    = my_read,
	.write   = my_write,
	.release = my_release,
};

static int my_init(void)
{
	dev_t device_id;
	int alloc_err = -1;
	int add_err   = -1;

	alloc_err= alloc_chrdev_region(&device_id, 0, num_of_dev, DRIVER_NAME);

	if(alloc_err != 0){
		pr_err("register device error %d\n", alloc_err);
		goto error_out;
	}

	my_major = MAJOR(device_id);

	cdev_init(&device_data.cdev, &my_fops);

	add_err = cdev_add(&device_data.cdev, device_id, num_of_dev);

	if(add_err != 0)
	{
		pr_err("add device error %d\n", add_err);
		goto error_out;
	}

	pr_alert("%s driver(major: %d) installed.\n", DRIVER_NAME, my_major);

	return 0;

error_out:
	if(add_err == 0)
	{
		cdev_del(&device_data.cdev);
	}

	if(alloc_err == 0)
	{
		unregister_chrdev_region(device_id, num_of_dev);
	}
	return -1;
}

static void my_exit(void)
{
	dev_t dev = MKDEV(my_major, 0);

	cdev_del(&device_data.cdev);
	unregister_chrdev_region(dev, num_of_dev);
	pr_alert("%s driver removed\n", DRIVER_NAME);
}

module_init(my_init)
module_exit(my_exit)
MODULE_LICENSE("GPL");
