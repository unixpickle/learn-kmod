#!/bin/bash

echo -n 'const char* virt_fs_data = "' >build/virt_fs_data.c
xxd -p example.tar | sed -E 's/(..)/\\x\1/g' | tr -d $'\n' >>build/virt_fs_data.c
echo '";' >>build/virt_fs_data.c
echo -n 'const int virt_fs_data_size = ' >>build/virt_fs_data.c
wc -c example.tar | cut -f 1 -d ' ' | tr -d $'\n' >>build/virt_fs_data.c
echo ';' >>build/virt_fs_data.c
