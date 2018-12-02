obj-m += ram_disk.o

all: ram_disk.ko

ram_disk.ko: ram_disk.c
	rm -rf build
	mkdir build
	cp *.c Makefile build
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/build modules

clean:
	rm -rf build