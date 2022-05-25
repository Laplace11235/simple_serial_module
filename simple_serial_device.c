#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <linux/circ_buf.h>

// TODO circular buffer (without seek from userspace)
// TODO FREE MEMEORY !!!!!
// TODO allocate memory for buffer separatedly
// TODO buffer overloading checking
// TODO create special character device file from module code

#define DRIVER_NAME    "my_device_driver" 


#define __BUFFER_SIZE	16


struct my_private_data{
	char buffer[__BUFFER_SIZE];
	size_t count;
	size_t size;
};

struct my_circ_buffer_data{
	char buffer[__BUFFER_SIZE];
	unsigned long head;
	unsigned long tail;
	ssize_t 	  size;
};

static struct my_circ_buffer_data circ_buffer;
static struct my_circ_buffer_data* p_circ_buffer = &circ_buffer;

static struct cdev my_cdev;
static unsigned int my_major   = 0;
static unsigned int num_of_dev = 1;


static int my_open(struct inode *inode, struct file *file)
{
	struct my_private_data* private_data;

	pr_alert("open serial device, size private_data %ld\n", sizeof(struct my_private_data));

	private_data = kmalloc(sizeof(struct my_private_data), GFP_KERNEL);

	p_circ_buffer->size = __BUFFER_SIZE;

	if(private_data == NULL){
		return -ENOMEM;
	}
	//rwlock_init(&ioctl_data->lock);
	private_data->size      = __BUFFER_SIZE;
	private_data->count     = 0;
	file->private_data      = private_data;
	return 0;
}
static loff_t my_seek(struct file *file, loff_t offset, int whence)
{
	loff_t new_offset = 0;
	pr_alert("%s offset %lld, whence %d (0 abs, 1/2 relative cur/end)", __func__, offset, whence);

	switch(whence)
	{
		case 0:
			new_offset = offset;
			break;
		case 1:
			new_offset = file->f_pos + offset;
			break;
		case 2:
			new_offset = ((struct my_private_data*)file->private_data)->size + offset;
			break;
		default:
			return -EINVAL;
	}
	if(new_offset < 0)
	{
		return -EINVAL;
	}

	file->f_pos = new_offset;

	return new_offset;
}

static ssize_t my_write(struct file *file, const char __user *user_buffer,
					size_t size, loff_t * offset)
{
	//struct my_private_data* data = (struct my_private_data *) file->private_data;
	unsigned long head = p_circ_buffer->head;

	/* The spin_unlock() and next spin_lock() provide needed ordering. */
	unsigned long tail = READ_ONCE(p_circ_buffer->tail);

	if (CIRC_SPACE(head, tail, p_circ_buffer->size) >= 1){
			/* insert one item into the buffer */
			char* p_item  = &(p_circ_buffer->buffer[head]);

			if (copy_from_user(p_item, user_buffer, 1))
			{
				return -EFAULT;
			}
			smp_store_release(&(p_circ_buffer->head),
							(head + 1) & (p_circ_buffer->size - 1));
	}
	//spin_unlock(&producer_lock);
	return 1;
}

static ssize_t my_read(struct file *file, char *user_buffer, size_t size, loff_t *offset)
{
	ssize_t ret = -1;
	//spin_lock(&consumer_lock);

	/* Read index before reading contents at that index. */
	unsigned long head = smp_load_acquire(&(p_circ_buffer->head));
	unsigned long tail = p_circ_buffer->tail;

	if (CIRC_CNT(head, tail, p_circ_buffer->size) >= 1) {

			/* extract one item from the buffer */
			char* item = &(p_circ_buffer->buffer[tail]);

			//consume_item(item);
			if(copy_to_user(user_buffer, item, 1))
			{
				return -EFAULT;
			}
			/* Finish reading descriptor before incrementing tail. */
			smp_store_release(&(p_circ_buffer->tail),
							(tail + 1) & (p_circ_buffer->size - 1));
		ret =  1;
	}
	else
	{
		ret = 0;
	}

	//spin_unlock(&consumer_lock);
	return ret;
}

static int my_release(struct inode* __inode, struct file* __file){
	pr_info("release serial device\n");
	return 0;
}

const struct file_operations my_fops = {
	.owner   = THIS_MODULE,
	.open    = my_open,
	.llseek  = my_seek,
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

	cdev_init(&my_cdev, &my_fops);

	add_err = cdev_add(&my_cdev, device_id, num_of_dev);

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
		cdev_del(&my_cdev);
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

	cdev_del(&my_cdev);
	unregister_chrdev_region(dev, num_of_dev);
	pr_alert("%s driver removed\n", DRIVER_NAME);
}

module_init(my_init)
module_exit(my_exit)
MODULE_LICENSE("GPL");
