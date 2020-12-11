#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/cdev.h>

MODULE_LICENSE("Dual BSD/GPL");

dev_t dev_id;
static struct class *dev_class;
static struct device *fifo_device;
static struct cdev *fifo_cdev;

int file_open(struct inode *pinode, struct file *pfile);
int file_close(struct inode *pinode, struct file *pfile);
ssize_t fifo_read(struct file *pfile, char __user *buffer, size_t length, loff_t *offset);
ssize_t fifo_write(struct file *pfile, const char __user *buffer, size_t length, loff_t *offset);

struct file_operations fifo_ops = {
  .owner = THIS_MODULE,
  .open = file_open,
  .read = fifo_read,
  .write = fifo_write,
  .release = file_close,
};

static int __init fifo_init(void){
  if (alloc_chrdev_region(&dev_id, 0, 1, "fifo")){
    printk(KERN_ERR "Can not register device!\n");
    return -1;
  }
  printk(KERN_INFO "Character device number registered!\n");

  dev_class = class_create(THIS_MODULE, "fifo_class");
  if(dev_class == NULL){
    printk(KERN_ERR "Cannot create class!\n");
    goto fail_0;
  }
  printk(KERN_INFO "Device class created!\n");

  fifo_device = device_create(dev_class, NULL, dev_id, NULL, "fifo");
  if(fifo_device == NULL){
    printk(KERN_ERR "Cannot create device!\n");
    goto fail_1;
  }
  printk(KERN_INFO "Device created!\n");

  fifo_cdev = cdev_alloc();
  fifo_cdev->ops = &fifo_ops;
  fifo_cdev->owner = THIS_MODULE;
  if(cdev_add(fifo_cdev, dev_id, 1)){
    printk(KERN_ERR "Cannot add cdev!\n");
    goto fail_2;
  }
  printk(KERN_INFO "Character device fifo added!\n");

  printk(KERN_INFO "Fifo loaded!\n");

  return 0;

  fail_2:
    device_destroy(dev_class, dev_id);
  fail_1:
    class_destroy(dev_class);
  fail_0:
    unregister_chrdev_region(dev_id, 1);
  return -1;

}

int file_open(struct inode *pinode, struct file *pfile)
{
		printk(KERN_INFO "Succesfully opened file\n");
		return 0;
}

int file_close(struct inode *pinode, struct file *pfile)
{
		printk(KERN_INFO "Succesfully closed file\n");
		return 0;
}

ssize_t fifo_read(struct file *pfile, char __user *buffer, size_t length, loff_t *offset)
{
		printk(KERN_INFO "Read from fifo\n");
		return 0;
}

ssize_t fifo_write(struct file *pfile, const char __user *buffer, size_t length, loff_t *offset)
{
		printk(KERN_INFO "Written into fifo\n");
		return length;
}

static void __exit fifo_exit(void){
  cdev_del(fifo_cdev);
  device_destroy(dev_class, dev_id);
  class_destroy(dev_class);
  unregister_chrdev_region(dev_id, 1);
  printk(KERN_INFO "Fifo unloaded!\n");
}

module_init(fifo_init);
module_exit(fifo_exit);
