ifneq ($(KERNELRELEASE),)
	obj-m := fifo_module.o
else
all:
	$(MAKE) -C /lib/modules/`uname -r`/build M=`pwd` modules
clean:
	$(MAKE) -C /lib/modules/`uname -r`/build M=`pwd` clean
	rm -f *~
endif
