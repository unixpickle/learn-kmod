obj-m += fake_disp.o

all: fake_disp.ko

fake_disp.ko: fake_disp.c
	rm -rf build
	mkdir build
	cp *.c Makefile build
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/build modules

clean:
	rm -rf build