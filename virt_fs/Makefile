obj-m += virt_fs.o

all: virt_fs.ko

virt_fs.ko: virt_fs.c
	rm -rf build
	mkdir build
	cp *.c Makefile build
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/build modules

clean:
	rm -rf build