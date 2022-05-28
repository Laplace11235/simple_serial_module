#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <linux/circ_buf.h>

// TODO fix seek
// TODO buffer overloading checking
// TODO create special character device file from module code

#define DRIVER_NAME    "my_device_driver" 


#define __BUFFER_SIZE	16

static DEFINE_SPINLOCK(consumer_lock);
static DEFINE_SPINLOCK(producer_lock);



struct my_circ_buffer{
	char buffer[__BUFFER_SIZE];
	unsigned long head;
	unsigned long tail;
	ssize_t 	  size;
};


static struct cdev my_cdev;
static struct my_circ_buffer* _p_circ_buffer;
static unsigned int my_major                = 0;
static unsigned int num_of_dev              = 1;


static int my_open(struct inode *inode, struct file *file)
{

	pr_alert("open serial device, size private_data %ld\n", sizeof(struct my_circ_buffer));


	file->private_data      = _p_circ_buffer;
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
	//struct my_private_data* data = (struct my_private_data *) file->private_data;
	struct my_circ_buffer* p_circ_buffer;
	ssize_t ret = -1;
	unsigned long head;
	unsigned long tail;
	ssize_t circ_space = 0;
	spin_lock(&producer_lock);

	p_circ_buffer = (struct my_circ_buffer*)file->private_data;
	head = p_circ_buffer->head;
	tail = READ_ONCE(p_circ_buffer->tail);

	circ_space = CIRC_SPACE(head, tail, p_circ_buffer->size);
	pr_alert("%s: %d circ_cpace  %ld, head %ld, tail %ld, size %ld\n", __func__, __LINE__, \
	circ_space, head, tail, p_circ_buffer->size);
	if (circ_space > 0)
	{

			char* p_item  = &(p_circ_buffer->buffer[head]);

			if (copy_from_user(p_item, user_buffer, size))
			{
				ret = -EFAULT;
				goto out;
			}
			smp_store_release(&(p_circ_buffer->head),
							(head + 1) & (p_circ_buffer->size - 1));
			ret = 1;
	}
out:
	spin_unlock(&producer_lock);
	return ret;
}

static ssize_t my_read(struct file *file, char *user_buffer, size_t size, loff_t *offset)
{
	struct my_circ_buffer* p_circ_buffer;
	ssize_t ret = -1;
	unsigned long head;
	unsigned long tail;
	ssize_t circ_cnt = 0;

	spin_lock(&consumer_lock);
	p_circ_buffer = (struct my_circ_buffer*)file->private_data;
	head = smp_load_acquire(&(p_circ_buffer->head));
	tail = p_circ_buffer->tail;

	circ_cnt = CIRC_CNT(head, tail, p_circ_buffer->size) > 0; 
	pr_alert("%s: %d circ_cnt %ld, head %ld, tail %ld, size %ld\n", __func__, __LINE__, \
	circ_cnt, head, tail, p_circ_buffer->size);
	if (circ_cnt > 0) 
	{
			/* extract one item from the buffer */
			char* item = &(p_circ_buffer->buffer[tail]);

			//consume_item(item);
			if(copy_to_user(user_buffer, item, 1))
			{
				pr_alert("%s: %d copy_to_user failed\n", __func__, __LINE__);
				ret = -EFAULT;
				goto out;
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
	int alloc_err = -1;
	int add_err   = -1;
	int enomem	  = -1;

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


	_p_circ_buffer = kmalloc(sizeof(struct my_circ_buffer), GFP_KERNEL);

	if(_p_circ_buffer == NULL)
	{
		goto error_out;
	}
	enomem = 0;
	_p_circ_buffer->head = 0;
	_p_circ_buffer->tail = 0;
	_p_circ_buffer->size = __BUFFER_SIZE;

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
	
	if(enomem == 0)
	{
		kfree(_p_circ_buffer);
	}
	return -1;
}

static void my_exit(void)
{
	dev_t dev = MKDEV(my_major, 0);

	cdev_del(&my_cdev);
	unregister_chrdev_region(dev, num_of_dev);
	pr_alert("%s driver removed\n", DRIVER_NAME);
	kfree(_p_circ_buffer);
}

module_init(my_init)
module_exit(my_exit)
MODULE_LICENSE("GPL");
