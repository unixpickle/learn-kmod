obj-m += virt_fs.o
virt_fs-objs := virt_fs_main.o virt_fs_data.o virt_fs_tar.o

all: virt_fs.ko

virt_fs.ko: virt_fs_main.c example.tar
	rm -rf build
	mkdir build
	./example_dump.sh
	cp *.c *.h Makefile build
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/build modules

virt_fs_data.c:

clean:
	rm -rf build
