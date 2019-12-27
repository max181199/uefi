#Path to OVMF.fd file
OVMF_PATH=/home/max/edk2/Build/OvmfIa32/DEBUG_GCC5/FV
#Path to Your JOS repo
OS_LAB_PATH=/home/max/Dropbox/m.fedyashkin-osprac
#Path to Loader.efi file builded from JOS package 
JOS_PKG=/home/max/edk2/Build/Jos/DEBUG_GCC5/IA32
#Path to JOS qemu installation directory
QEMU_INST=/usr

#Make uefi image and run simple target
all: image simple

complex:
	/home/chert/os/inst/bin/qemu-system-i386 -m 2048 -machine q35 -cpu Penryn -smp 4,cores=2 \
	-bios $(OVMF_PATH)/OVMF.fd \
	-usb -device usb-kbd -device usb-tablet -device e1000,netdev=usr0,id=vnet0 \
	-device ide-drive,bus=ide.0,drive=ESP -drive id=ESP,if=none,file=fat:rw:ESP,format=raw \
	-netdev user,id=usr0

#add-symbol-file Loader.debug 0x06AC1240 -s .data 0x6ACD780

#https://wiki.osdev.org/UEFI
simple:
	$(QEMU_INST)/bin/qemu-system-i386 -m 512 -s -bios $(OVMF_PATH)/OVMF.fd -drive file=uefi.img,if=ide -net none -serial mon:stdio -debugcon file:debug.log -global isa-debugcon.iobase=0x402

#Build JOS kernel, generate uefi image
#Current file size = 48mb, increase if JOS kernel overlaps
#File Kernel64.txt is used by Bootloader and must have UTF16 encoding with BOM
image:
	make -C $(OS_LAB_PATH)
	dd if=/dev/zero of=uefi.img bs=512 count=93750
	parted uefi.img -s -a minimal mklabel gpt
	parted uefi.img -s -a minimal mkpart EFI FAT32 2048s 93716s
	parted uefi.img -s -a minimal toggle 1 boot
	dd if=/dev/zero of=part.img bs=512 count=91669
	mformat -i part.img -h 32 -t 32 -n 64 -c 1
	mmd -i part.img /EFI
	mmd -i part.img /EFI/BOOT
	echo "\EFI\BOOT\kernel" > Kernel64.txt
	iconv -f US-ASCII -t UTF-16LE Kernel64.txt -o Kernel64.txt
	sed -i '1s/^/\xff\xfe/' Kernel64.txt
	mcopy -i part.img $(JOS_PKG)/Loader.efi ::/EFI/BOOT/BOOTIA32.EFI
	mcopy -i part.img $(JOS_PKG)/Loader.debug ::/EFI/BOOT/BOOTIA32.DEBUG
	mcopy -i part.img Kernel64.txt ::/EFI/BOOT
	mcopy -i part.img $(OS_LAB_PATH)/obj/kern/kernel ::/EFI/BOOT
	dd if=part.img of=uefi.img bs=512 count=91669 seek=2048 conv=notrunc
	rm part.img
	rm Kernel64.txt

