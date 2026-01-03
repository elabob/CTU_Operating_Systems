#!/bin/bash

#zistim kolko jadier mozem pouzit
CORES=$(nproc)

echo "Zacinam"

#vytvorim hlavny adresar
mkdir -p busybox_system
cd busybox_system

#Busybox
echo "Stahujem zdrojaky pre busybox"

# Stahujem busybox
if [ ! -d "busybox" ]; then
  git clone --depth 1 https://git.busybox.net/busybox
fi

cd busybox

#ak existuje tak nestiahnem znovu
if [ -f .config ]; then
  make clean
fi

echo "Robim defconfig pre busybox"
make defconfig

echo "Kompilujem busybox"
make -j$CORES
#nainstalujem to do _install
make install

#vratim sa o uroven vyssie
cd ..

#linux kernel
echo "Stahujem Linux kernel"
if [ ! -d "linux" ]; then
  git clone --depth 1 --branch v6.6 https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git
fi

cd linux

echo "Konfigurujem kernel"
if [ ! -f .config ]; then
  make defconfig
fi

echo "Kompilujem jadro"
make -j$CORES bzImage

#potrebujem nastroj get_init na vytvorenie ramdisku
make -j$CORES dir-pkg

cd ..

#Filesystem
echo "Pripravujem veci pre ramdisk"
cd busybox

# Ziskam bezne kniznice (tie co maju sipku =>)
LIBS=$(ldd _install/bin/busybox | grep "=>" | awk '{print $3}')

# Ziskam aj loader (ld-linux...), ktory nema sipku
LOADER=$(ldd _install/bin/busybox | grep "/" | grep -v "=>" | awk '{print $1}')

# Spojim to dokopy
ALL_LIBS="$LIBS $LOADER"

for lib_file in $ALL_LIBS; do
    if [ -f "$lib_file" ]; then
        echo "Kopirujem: $lib_file"
        cp --parents "$lib_file" _install/
    fi
done

#format zo zadania
echo "Generujem filelist..."
cat <<EOF > filelist
dir /dev 755 0 0
nod /dev/tty0 644 0 0 c 4 0
nod /dev/tty1 644 0 0 c 4 1
nod /dev/tty2 644 0 0 c 4 2
nod /dev/tty3 644 0 0 c 4 3
nod /dev/tty4 644 0 0 c 4 4
nod /dev/console 644 0 0 c 5 1
slink /init bin/busybox 700 0 0
dir /proc 755 0 0
dir /sys 755 0 0
EOF

# cesta k nastroju co som skompilovala s kernelom
CPIO_TOOL="../linux/usr/gen_init_cpio"

# pridam tam vsetky subory co vznikli po make install
find _install -mindepth 1 -type d -printf "dir /%P %m 0 0\n" >> filelist
find _install -type f -printf "file /%P %p %m 0 0\n" >> filelist
find _install -type l -printf "slink /%P %l %m 0 0\n" >> filelist

# vytvorim vysledny img
echo "Balim to do ramdisk.img..."
$CPIO_TOOL filelist | gzip > ramdisk.img

cd ..

#Spustenie
echo "Spustam QEMU emulaciu"
qemu-system-x86_64 \
    -kernel linux/arch/x86/boot/bzImage \
    -initrd busybox/ramdisk.img \
    -nographic \
    -append "console=ttyS0"

echo "Hotovo."




