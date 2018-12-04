obj-m += fake_webcam.o

all: fake_webcam.ko

fake_webcam.ko: fake_webcam.c
	rm -rf build
	mkdir build
	cp *.c Makefile build
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/build modules

clean:
	rm -rf build