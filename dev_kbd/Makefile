obj-m += dev_kbd.o

all: dev_kbd.ko

dev_kbd.ko: dev_kbd.c
	rm -rf build
	mkdir build
	cp *.c Makefile build
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/build modules

clean:
	rm -rf build