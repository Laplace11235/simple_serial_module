KERNELDIR := /lib/modules/`uname -r`/build

obj-m += simple_serial_device.o

all:
	make -C $(KERNELDIR) M=`pwd`

clean:
	make -C $(KERNELDIR) M=`pwd` clean


