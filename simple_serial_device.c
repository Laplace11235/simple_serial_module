#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>

// TODO dynamic  major numbers
// TODO pair major minor in struct
// TODO allocate memory for buffer
// TODO create special character device file from module code

#define MY_MAJOR       142
#define MY_MAX_MINORS  1

static dev_t my_device;

struct my_device_data {
	struct cdev cdev;
	char* buffer;
	size_t size;
};

static int my_init(void)
{
	pr_info("Init serial device\n");
	my_device = MKDEV(MY_MAJOR, 0);;
	return  register_chrdev_region(my_device, MY_MAX_MINORS, "my_device_driver");
}

static void my_exit(void)
{
	pr_info("exit serial device\n");
	unregister_chrdev_region(my_device, MY_MAX_MINORS);
}

static int my_open(struct inode *inode, struct file *file)
{
	struct my_device_data *my_data;
	pr_info("open serial device\n");
	my_data = container_of(inode->i_cdev, struct my_device_data, cdev);
	file->private_data = my_data;
	return 0;
}

static ssize_t my_write(struct file *file, const char __user *user_buffer,
					size_t size, loff_t * offset)
{
	struct my_device_data *my_data = (struct my_device_data *) file->private_data;
	ssize_t len = min(my_data->size - (size_t)*offset, size);

	if (len <= 0)
		return 0;

	if (copy_from_user(my_data->buffer + *offset, user_buffer, len))
		return -EFAULT;

	*offset += len;
	return len;
}

static ssize_t my_read(struct file *file, char __user *user_buffer, size_t size, loff_t *offset)
{
	struct my_device_data *my_data;
	my_data = (struct my_device_data *) file->private_data;
	return 0;
}

static int my_release(struct inode* __inode, struct file* __file){
	pr_info("release serial device\n");
	return 0;
}

const struct file_operations my_fops = {
	.owner = THIS_MODULE,
	.open = my_open,
	.read = my_read,
	.write = my_write,
	.release = my_release,
};

module_init(my_init)
module_exit(my_exit)
MODULE_LICENSE("GPL");
