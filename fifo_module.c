#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/cdev.h>

MODULE_LICENSE("Dual BSD/GPL");

int fifo_buffer[16];
int pos = 0;

dev_t dev_id;
static struct class *dev_class;
static struct device *fifo_device;
static struct cdev *fifo_cdev;

DECLARE_WAITE_QUEUE_HEAD(readQueue);
DECLARE_WAITE_QUEUE_HEAD(writeQueue);

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

int file_open(struct inode *pinode, struct file *pfile){
		printk(KERN_INFO "Succesfully opened file\n");
		return 0;
}

int file_close(struct inode *pinode, struct file *pfile){
		printk(KERN_INFO "Succesfully closed file\n");
		return 0;
}

ssize_t fifo_read(struct file *pfile, char __user *buffer, size_t length, loff_t *offset){
  char output[80];
  long int len = 0;
  int i;

  if (wait_event_interruptible(readQueue,(pos>0)))     // Postavi u queue ako nema sta procitati
    return -ERESTARTSYS;

  if(pos == 0){
    printk(KERN_WARNING "Fifo empty!\n");
  }else{
    pos--;
    len = scnprintf(output, strlen(output), "%#04x", fifo_buffer[0]);  // Uvek cita prvi elemenat kao najstariji
    if (copy_to_user(buffer, input, len))
      return -EFAULT;
    for(i=0;i<15;i++){                                // Pomeramo niz ulevo u stilu fifo buffera, na desne pozicije simbolicno pisemo -1
      fifo_buffer[i]=fifo_buffer[i+1];
    }
    fifo_buffer[15] = -1;
    printk(KERN_INFO "Read from fifo!\n");
  }

  wake_up_interruptible(&writeQueue); // Budimo eventualne procese za upis, mesto oslobodjeno
	return 0;
}

ssize_t fifo_write(struct file *pfile, const char __user *buffer, size_t length, loff_t *offset){
  char input[80]; // max 16 vrednosti u fifo-u, dakle 80 karaktera u formatu: 0x01;0x02;0x03 ...
  char current[4];
  int toBuffer;
  int finished = 0;
  if (copy_from_user(input,buffer,length)){
    return -EFAULT;
  }

  input[length-1] = '\0'; // Terminirati da bi funkcije ispod uspesno radile

  if ((input[4] != "\0") && (input[4] != ";"))              // U slucaju da je posle hex broja dodat nedozvoljen karakter
    printk(KERN_ERR "Expected format: 0x??;0x??;0x??..(16) of max value 0xFF");
  while(1){
    strncpy(current, input, 4);                    //Uzimamo 4 karaktera (0x??)
    if (kstrtoint(current, 0, toBuffer) != 0){
      printk(KERN_ERR "Expected format: 0x??;0x??;0x??..(16) of max value 0xFF");   //U slucaju da ta cetiri karaktera nisu hex broj
    }

    if (wait_event_interruptible(writeQueue,(pos<16)))    // Postavi u queue ako je fifo pun
      return -ERESTARTSYS;

    if(pos > 15){
      printk(KERN_WARNING "Fifo full\n");
    }else{
      fifo_buffer[pos] = toBuffer;
      pos++;                                                 //pos zapravo oznacava broj elemenata i poziciju poslednjeg
      printk(KERN_INFO "Wrote %d into fifo", toBuffer);
    }

    if ((finished = 1) || (input[4] == '\0')) break;     // Zaustavi parsing u zavisnosti da li je bilo vise vrednosti
    if (strsep(&input, ";") == NULL) finished = 1;       //Delimo string delimiterom ;, ako ga nema u sledecem ciklusu se zaustavi(zbog poslednjeg broja)
  }

  wake_up_interruptible(&readQueue); // Budimo eventualne procese za citanje, fifo vise nije prazan
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
