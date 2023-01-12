/*
 *  finalprojet.c: Creates a read and write char device that says how many times
 *  you've read from the dev file, how many time you write and you open the file .Those statistic are stored in /procfs file
 */


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/jiffies.h>

#define DEV_NAME "chardev"
#define CLASS_NAME "my_class"
#define PROC_DIR "dossier_proc"
#define PROC_STATS "procfs"
#define KEY 0x41
static char *buffer;
static size_t buffer_size = 1024;
static int open_count;
static int write_count;
static int read_count;
static long open_time;
static long total_time;
static int major;
static struct class *my_class;
static struct device *chardev;


static struct proc_dir_entry *proc_dir, *proc_stats;

static ssize_t chrdev_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{   
	int i;
	char *tmp_buf;

	if (*off >= buffer_size)
		return 0;

	if (len > buffer_size - *off)
		len = buffer_size - *off;

	tmp_buf = kmalloc(len, GFP_KERNEL);
	if (!tmp_buf)
		return -ENOMEM;

// copy the data from the buffer to a temporary buffer
        memcpy(tmp_buf, buffer + *off, len);
     // decrypt the data in the temporary buffer
        for (i = 0; i < len; i++)
        	tmp_buf[i] ^= KEY;
	

	if (copy_to_user(buf, tmp_buf, len)) {
		kfree(tmp_buf);
		return -EFAULT;
	}

	kfree(tmp_buf);
	*off += len;
	read_count++;

	return len;
}


static ssize_t chrdev_write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
	int i;
	char *tmp_buf;

	if (*off >= buffer_size)
		return -ENOSPC;

	if (len > buffer_size - *off)
		len = buffer_size - *off;

	tmp_buf = kmalloc(len, GFP_KERNEL);
	if (!tmp_buf)
		return -ENOMEM;

	if (copy_from_user(tmp_buf, buf, len)) {
		kfree(tmp_buf);
		return -EFAULT;
	}
        for (i = 0; i < len; i++)
        	tmp_buf[i] ^= KEY;
	
        memcpy(buffer + *off, tmp_buf, len);
	kfree(tmp_buf);
	*off += len;
	write_count++;

	return len;
}

static int chrdev_open(struct inode *inode, struct file *filp)
{   
	open_count++;
	open_time = jiffies;

	return 0;
}


static int chrdev_release(struct inode *inode, struct file *filp)
{   
	open_time = jiffies - open_time;
	total_time = total_time + open_time;

	return 0;
}


static const struct file_operations chrdev_fops = {
	.owner = THIS_MODULE,
	.open = chrdev_open,
	.release = chrdev_release,
	.read = chrdev_read,
	.write = chrdev_write,
};


static int chrdev_stats_show(struct seq_file *m, void *v)
{
	seq_printf(m, "Open count: %d\n", open_count);
	seq_printf(m, "Write count: %d\n", write_count);
	seq_printf(m, "Read count: %d\n", read_count);
	seq_printf(m, "Open time: %ld ms\n", jiffies_to_msecs(total_time));

	return 0;
}


static int chrdev_stats_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, chrdev_stats_show, NULL);
}


static const struct proc_ops chrdev_stats_fops = {
	.proc_open = chrdev_stats_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int __init chrdev_init(void)
{
    printk(KERN_INFO "chardev: file initialised\n");

	major = register_chrdev(0, DEV_NAME, &chrdev_fops);
	if (major < 0) {
        printk(KERN_ALERT "chardev: failed to register a major number\n");
		kfree(buffer);
		return major;
	}
    my_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(my_class)) {
		unregister_chrdev(major, DEV_NAME);
		printk(KERN_ALERT "chardev: failed to register device class\n");
		return PTR_ERR(my_class);
	}
    chardev = device_create(my_class, NULL, MKDEV(major, 0), NULL, DEV_NAME);
	if (IS_ERR(chardev)) {
		class_destroy(my_class);
		unregister_chrdev(major, DEV_NAME);
		printk(KERN_ALERT "chardev: failed to create the device\n");
		return PTR_ERR(chardev);
	}

    buffer = kmalloc(buffer_size, GFP_KERNEL);
	if (!buffer) {
		device_destroy(my_class, MKDEV(major, 0));
		class_destroy(my_class);
		unregister_chrdev(major, DEV_NAME);
		printk(KERN_ALERT "chardev: failed to allocate memory\n");
		return -ENOMEM;
	}

	proc_dir = proc_mkdir(PROC_DIR, NULL);
	if (!proc_dir) {
		unregister_chrdev(major, DEV_NAME);
		kfree(buffer);
		return -ENOMEM;
	}

	proc_stats = proc_create(PROC_STATS, 0, proc_dir, &chrdev_stats_fops);
	if (!proc_stats) {
		remove_proc_entry(PROC_DIR, NULL);
		unregister_chrdev(major, DEV_NAME);
		kfree(buffer);
        printk(KERN_INFO "my_proc: fail\n");
		return -ENOMEM;
	}
    printk(KERN_INFO "procfs: stat confirmed\n");

	return 0;
}



static void __exit chrdev_exit(void)
{
	remove_proc_entry(PROC_STATS, proc_dir);
	remove_proc_entry(PROC_DIR, NULL);
    device_destroy(my_class, MKDEV(major, 0));
    class_destroy(my_class);
	unregister_chrdev(0, DEV_NAME);
	kfree(buffer);
    printk(KERN_INFO "my_dev: exit\n");
}



module_init(chrdev_init);
module_exit(chrdev_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Virtual File Driver");