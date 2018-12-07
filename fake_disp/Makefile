obj-m += fake_disp.o
fake_disp-objs := fake_disp_main.o fake_disp_drm.o fake_disp_mm.o

all: build/fake_disp.ko

build/fake_disp.ko: fake_disp_main.c fake_disp_drm.c fake_disp_mm.c
	rm -rf build
	mkdir build
	cp *.c *.h Makefile build
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/build modules

clean:
	rm -rf build