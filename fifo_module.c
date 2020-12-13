#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/wait.h>

MODULE_LICENSE("Dual BSD/GPL");

struct semaphore sem;

int batch = 1;                                                                                              // batch nema potrebe stititi semaforom jer je jedini deo na koji on utice (read()) vec zasticen ?

int fifo_buffer[16];
int pos = 0;
int secondPass = 1;

dev_t dev_id;
static struct class *dev_class;
static struct device *fifo_device;
static struct cdev *fifo_cdev;

DECLARE_WAIT_QUEUE_HEAD(readQueue);
DECLARE_WAIT_QUEUE_HEAD(writeQueue);

int file_open(struct inode *pinode, struct file *pfile);
int file_close(struct inode *pinode, struct file *pfile);
ssize_t fifo_read(struct file *pfile, char __user *buffer, size_t length, loff_t *offset);
ssize_t fifo_write(struct file *pfile, const char __user *buffer, size_t length, loff_t *offset);

struct file_operations fifo_ops = {                                                                           // povezujemo file ops strukturu za char device sa funkcijama read/write/open/release
  .owner = THIS_MODULE,
  .open = file_open,
  .read = fifo_read,
  .write = fifo_write,
  .release = file_close,
};

static int __init fifo_init(void){
  sema_init(&sem,1);

  if (alloc_chrdev_region(&dev_id, 0, 1, "fifo")){                                                            // Dinamicka alokacija major i minor broja
    printk(KERN_ERR "Can not register device!\n");
    return -1;
  }
  printk(KERN_INFO "Character device number registered!\n");

  dev_class = class_create(THIS_MODULE, "fifo_class");                                                        // device klasa sa vlasnikom i imenom
  if(dev_class == NULL){
    printk(KERN_ERR "Cannot create class!\n");
    goto fail_0;
  }
  printk(KERN_INFO "Device class created!\n");

  fifo_device = device_create(dev_class, NULL, dev_id, NULL, "fifo");                                         // Stvaranje device fajla u fajl sistemu
  if(fifo_device == NULL){
    printk(KERN_ERR "Cannot create device!\n");
    goto fail_1;
  }
  printk(KERN_INFO "Device created!\n");

  fifo_cdev = cdev_alloc();                                                                                   // Registrovanje char device-a u kernelu i povezivanje fops pokazivaca
  fifo_cdev->ops = &fifo_ops;
  fifo_cdev->owner = THIS_MODULE;
  if(cdev_add(fifo_cdev, dev_id, 1)){
    printk(KERN_ERR "Cannot add cdev!\n");
    goto fail_2;
  }
  printk(KERN_INFO "Character device fifo added!\n");

  printk(KERN_INFO "Fifo loaded!\n");

  return 0;

  fail_2:                                                                                                     // fail labele za skokove i brisanje u slucaju greske
    device_destroy(dev_class, dev_id);
  fail_1:
    class_destroy(dev_class);
  fail_0:
    unregister_chrdev_region(dev_id, 1);
  return -1;

}

int file_open(struct inode *pinode, struct file *pfile){                                                      // Open i close funkcije sluze samo za KERN INFO signalizaciju u ovom slucaju
		printk(KERN_INFO "Succesfully opened file\n");
		return 0;
}

int file_close(struct inode *pinode, struct file *pfile){
		printk(KERN_INFO "Succesfully closed file\n");
		return 0;
}

ssize_t fifo_read(struct file *pfile, char __user *buffer, size_t length, loff_t *offset){
  long int len = 0;
  char output[80];
  int ret;
  int i;
  int j;
  output[0] = '\0';
  //secondPass = !secondPass;                                                                           // Kako  sistem ocekuje povratnu vrednost u vidu broja bajtova za ispisati
                                                                                                        // Ispis ne radi za return 0, tako da ovime dozvoljavamo prvi ispis, ali na drugi
                                                                                                        // zahtev od cat vracamo 0
                                                                                                        // Ovo ima smisla staviti unutar kriticne sekcije, u slucaju vise simultanih cat-ova, pa je to ispod i uradjeno
  //if (secondPass == 1) return 0;

  if(down_interruptible(&sem))                                                                          // spusatmo semafor zbog provere pos
    return -ERESTARTSYS;
  while(pos < batch){
    up(&sem);                                                                                           // dizemo semafor da bi drugi proces mogao probuditi trenutni iz readQueue
    printk(KERN_WARNING "Fifo empty or not enough elements for a full batch! On hold...\n");
    if(wait_event_interruptible(readQueue,(pos>(batch-1))))                                             // Postavi u queue ako nema sta procitati
      return -ERESTARTSYS;
    if(down_interruptible(&sem))                                                                        // kada se dobije novi elemenat, spustamo semafor i nastavljamo dalje, ako ne,
      return -ERESTARTSYS;                                                                              // oznaci kao task_interruptible i cekaj a) svoj red na semafor, b) ispunjen uslov pos>=batch
  }
  secondPass = !secondPass;                                                                             // Objasnjeno iznad
  if (secondPass == 1) return 0;

  if(pos > (batch-1)){

    pos=pos-batch;
    for(j=0;j<batch;j++){
      len = snprintf(output+strlen(output), 80, "%#04x ", fifo_buffer[0]);                              // Uvek cita prvi elemenat kao najstariji

      for(i=0;i<15;i++){                                                                                // Pomeramo niz ulevo u stilu fifo buffera, na desne pozicije simbolicno pisemo -1
        fifo_buffer[i]=fifo_buffer[i+1];
      }
      fifo_buffer[15] = -1;
    }

    ret = copy_to_user(buffer, output, len*batch);
    if (ret)
      return -EFAULT;
    printk(KERN_INFO "Read from fifo!\n");
  }
  up(&sem);                                                                                              // Kritican deo zavrsen, dizemo semafor
  wake_up_interruptible(&writeQueue);                                                                    // Budimo eventualne procese za upis, mesto oslobodjeno
	return len*batch;
}

ssize_t fifo_write(struct file *pfile, const char __user *buffer, size_t length, loff_t *offset){
  char input[80];                                                                                        // max 16 vrednosti u fifo-u, dakle 80 karaktera u formatu: 0x01;0x02;0x03 ...
  char *inputCopy;
  char trimmed[5];
  int toBuffer;

  if (copy_from_user(input,buffer,length)){
    return -EFAULT;
  }
  input[length-1] = '\0';                                                                                // Terminirati da bi funkcije ispod uspesno radile
  inputCopy = input;                                                                                     // strsep ocekuje char**


  strncpy(trimmed, inputCopy, 4);                                                                        // Uzimamo 4 karaktera (potencijalno num=)
  trimmed[4] = '\0';
  if ((inputCopy[4] != '\0') && (inputCopy[4] != ';') && (strncmp(trimmed, "num=", 4)!=0)){              // U slucaju da je posle hex broja dodat nedozvoljen karakter
    printk(KERN_ERR "Expected format: 0x??;0x??;0x??..(16) of max value 0xFF (1)");
    return length;

  }else if(strncmp(trimmed, "num=", 4)==0){                                                              // U slucaju num=komande
    strsep(&inputCopy, "=");                                                                             // odbaciti num=
    kstrtoint(inputCopy, 10, &batch);                                                                    // broj iza = proglasiti za batch size
    printk(KERN_INFO "Batch size set to %d", batch);
    return length;
  }
                                                                                                        /*
                                                                                                        * Deo ispod upisuje redom brojeve dobijene u formatu 0x??;0x??;0x??;... ali ide u writeQueue tek nailaskom na pun fifo pri upisu niza,
                                                                                                        * odnosno ne predvidja mesta pre upisa i ne ceka za upis celog niza, nego upisuje sta moze. Mislio sam da ovo ima vise smisla. Ako gresim,
                                                                                                        * potrebno je prebrojati broj ";" delimitera pa je predvidjeni broj za upis jednak broj delimitera+1, sto bi se trebalo proveriti pre while-a
                                                                                                        * i na tom mestu odraditi proveru za waitQueue. Takodje semafor spustati/podizati van while-a a ne unutar u tom slucaju
                                                                                                        */
  while(1){
    strncpy(trimmed, inputCopy, 4);                                                                       // Uzimamo 4 karaktera (0x??)
    trimmed[4] = '\0';
    if (kstrtoint(trimmed, 0, &toBuffer) != 0){
      printk(KERN_ERR "%s received. Expected format: 0x??;0x??;0x??..(16) of max value 0xFF (2)", trimmed); // U slucaju da ta cetiri karaktera nisu hex broj
      return length;
    } else if (toBuffer>255 || toBuffer<0){
      printk(KERN_ERR "Input exceeds max value of 0xFF!");
      return length;
    }

    if(down_interruptible(&sem))                                                                         // Sputstamo i dizemo semafor kao i u read(), ali je odluceno da upis bude element-wise
      return -ERESTARTSYS;                                                                               // a ne za citav batch, dakle nece cekati na oslobadjanje svih mesta, nego upisuje 1 po 1 , a ceka kada se napuni
    while(pos > 15){
      printk(KERN_WARNING "Fifo full! On hold...\n");
      up(&sem);
      if (wait_event_interruptible(writeQueue,(pos<16)))                                                 // Postavi u queue ako je fifo pun
        return -ERESTARTSYS;
      if(down_interruptible(&sem))
        return -ERESTARTSYS;
    }

    if(pos < 16){
      fifo_buffer[pos] = toBuffer;
      pos++;                                                                                              // pos zapravo oznacava broj elemenata i poziciju poslednjeg
      printk(KERN_INFO "Wrote %d into fifo", toBuffer);
    }
    up(&sem);                                                                                             // Dizemo semafor pre eventualnog break-a
    if (inputCopy[4] == '\0') break;                                                                      // Zaustavi parsing u zavisnosti da li je bilo vise vrednosti
    if (strsep(&inputCopy, ";") == NULL) break;                                                           // Delimo string delimiterom ;, ako ga nema u sledecem ciklusu se zaustavi(zbog poslednjeg broja)

  }


  wake_up_interruptible(&readQueue);                                                                      // Budimo eventualne procese za citanje, fifo vise nije prazan
	return length;
}

static void __exit fifo_exit(void){                                                                       // U izlaznoj funkciji brisemo sve char uredjaje i oslobadjamo alocirane brojeve
  cdev_del(fifo_cdev);
  device_destroy(dev_class, dev_id);
  class_destroy(dev_class);
  unregister_chrdev_region(dev_id, 1);
  printk(KERN_INFO "Fifo unloaded!\n");
}

module_init(fifo_init);
module_exit(fifo_exit);
