obj-m += fake_webcam.o

all: build/fake_webcam.ko build/bouncy_ball

build/fake_webcam.ko: fake_webcam.c
	rm -rf build
	mkdir build
	cp *.c Makefile build
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/build modules

build/bouncy_ball: bouncy_ball.c
	mkdir -p build
	gcc bouncy_ball.c -o build/bouncy_ball

clean:
	rm -rf build