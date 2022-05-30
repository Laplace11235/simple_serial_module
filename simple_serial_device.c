#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <linux/circ_buf.h>

// TODO debug out
// TODO indets of code

#define SIMPLE_SERIAL_DEVICE_CREATE_FILE

#define DRIVER_NAME    "my_device_driver" 

static int buffer_size=256;
module_param(buffer_size, int, 0);

#ifdef SIMPLE_SERIAL_DEVICE_CREATE_FILE

#define CH_FILE_NAME   "simple_serial_device"
#define CLASS_DEVICE   "simple_serial_device_class"

#endif

static DEFINE_SPINLOCK(consumer_lock);
static DEFINE_SPINLOCK(producer_lock);

struct my_circ_buffer{
	char* 		buffer;
	unsigned long head;
	unsigned long tail;
	ssize_t 	  size;
};

#ifdef SIMPLE_SERIAL_DEVICE_CREATE_FILE
	static struct class *dev_class;
	static struct device *device;
#endif

static struct cdev my_cdev;
static struct my_circ_buffer  circ_buffer;
static unsigned int my_major                = 0;
static unsigned int num_of_dev              = 1;


static int __is_pow_2(int val)
{
	int count = 0;	
	while(val){
		count += (val & 1);
		val >>=1;
	}
	return count == 1;
}

static int my_open(struct inode *inode, struct file *file)
{
	file->private_data      = &circ_buffer;
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
			//new_offset = ((struct my_private_data*)file->private_data)->size + offset;
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
	struct my_circ_buffer* p_circ_buffer;
	ssize_t ret = 0;
	unsigned long head;
	unsigned long tail;
	ssize_t circ_space = 0;
	spin_lock(&producer_lock);

	p_circ_buffer = (struct my_circ_buffer*)file->private_data;
	head = p_circ_buffer->head;
	tail = READ_ONCE(p_circ_buffer->tail);

	circ_space = CIRC_SPACE(head, tail, p_circ_buffer->size);

	if (circ_space >= 1)
	{
			char* p_item  = &(p_circ_buffer->buffer[head]);

			if (get_user(*p_item, user_buffer))
			{
				ret = -EFAULT;
				goto out;
			}
			pr_alert("%s: %d circ_cpace  %ld, head %ld, tail %ld, buffer->size %ld size %ld *p_item %c \n", __func__, __LINE__, \
			circ_space, head, tail, \
			p_circ_buffer->size, size, *p_item);
			smp_store_release(&(p_circ_buffer->head),
							(head + 1) & (p_circ_buffer->size - 1));
			ret = 1;
	}
out:
	spin_unlock(&producer_lock);
	return ret;
}

static ssize_t my_read(struct file *file, char __user *user_buffer, size_t size, loff_t *offset)
{
	struct my_circ_buffer* p_circ_buffer;
	ssize_t ret = 0;
	unsigned long head;
	unsigned long tail;
	ssize_t circ_cnt = 0;

	spin_lock(&consumer_lock);
	p_circ_buffer = (struct my_circ_buffer*)file->private_data;
	head = smp_load_acquire(&(p_circ_buffer->head));
	tail = p_circ_buffer->tail;

	circ_cnt = CIRC_CNT(head, tail, p_circ_buffer->size); 
	pr_alert("%s: %d circ_cnt %ld, head %ld, tail %ld, size %ld\n", __func__, __LINE__, \
	circ_cnt, head, tail, p_circ_buffer->size);
	if (circ_cnt >= 1) 
	{
			char* item = &(p_circ_buffer->buffer[tail]);

			if(put_user(*item, user_buffer) == 0)
			{
				pr_alert("%s: %d copy_to_user failed\n", __func__, __LINE__);
				ret = -EFAULT;
				goto out;
			}

			smp_store_release(&(p_circ_buffer->tail),
							(tail + 1) & (p_circ_buffer->size - 1));

		ret =  1;
	}
	else
	{
		ret = 0;
	}

out:
	spin_unlock(&consumer_lock);

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
	int alloc_err      = -1;
	int add_err        = -1;
	int enomem	       = -1;

#ifdef SIMPLE_SERIAL_DEVICE_CREATE_FILE
	int class_cr_err   = -1;
	int dev_cr_err     = -1;
#endif

	if(!__is_pow_2(buffer_size) && buffer_size > 1)
	{
		pr_err("buffer_size=%d have to be pow2\n", buffer_size);
		goto error_out;
	}
	alloc_err= alloc_chrdev_region(&device_id, 0, num_of_dev, DRIVER_NAME);

	if(alloc_err != 0)
	{
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

#ifdef SIMPLE_SERIAL_DEVICE_CREATE_FILE
	dev_class = class_create(THIS_MODULE, CLASS_DEVICE);
	if(dev_class == NULL)
	{
		pr_err("Cannot create the struct class %s for device\n", CLASS_DEVICE);
		goto error_out;
	}
	class_cr_err = 0;  

	device = device_create(dev_class,NULL,device_id,NULL, CH_FILE_NAME);
	if(device == NULL)
	{
		pr_err("Cannot create the device file %s\n", CH_FILE_NAME);
		goto error_out;
	}
	dev_cr_err = 0;
#endif

	circ_buffer.buffer = kmalloc(buffer_size, GFP_KERNEL);

	if(circ_buffer.buffer == NULL)
	{
		goto error_out;
	}
	enomem = 0;
	circ_buffer.head = 0;
	circ_buffer.tail = 0;
	circ_buffer.size = buffer_size;

	pr_alert("%s driver(major: %d) installed. buffer_size: %d\n", DRIVER_NAME, my_major, buffer_size);

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
	
#ifdef SIMPLE_SERIAL_DEVICE_CREATE_FILE
	if(class_cr_err == 0)
	{
        class_destroy(dev_class);
	}
		   
	if(dev_cr_err == 0)
	{
        unregister_chrdev_region(device_id,1);
	}
#endif

	if(enomem == 0)
	{
		kfree(circ_buffer.buffer);
	}
	return -1;
}

static void my_exit(void)
{
	dev_t dev = MKDEV(my_major, 0);

	cdev_del(&my_cdev);

#ifdef SIMPLE_SERIAL_DEVICE_CREATE_FILE
	device_destroy(dev_class,dev);
	class_destroy(dev_class);
#endif

	unregister_chrdev_region(dev, num_of_dev);
	pr_alert("%s driver removed\n", DRIVER_NAME);
	kfree(circ_buffer.buffer);
}

module_init(my_init)
module_exit(my_exit)
MODULE_LICENSE("GPL");
