obj-m += hello_world.o

all: hello_world.ko

hello_world.ko: hello_world.c
	rm -rf build
	mkdir build
	cp *.c Makefile build
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/build modules

clean:
	rm -rf build