Xilinx tools version: 2022.2

Use code from repository
Vivado
Open project:
- launch Vivado
- in tcp console enter: 
	cd /path/to/repository/AxiQuadSpi/Vivado/ZCU104SPI
	source Zcu104SPI.tcl


Petalinux
- create a petalinux project: petalinux-create -t project -n Your_project's_name --template zynqMP
- copy content of /path/to/repository/AxiQuadSpi/Petalinux/ZCU104-SPI into your project directory
- configurate it: petalinux-config --get-hw-description /your/project/directory/ZCU104SPI.xsa
- build it: petalinux-build
- create bootloaders: petalinux-package --boot --format BIN --fsbl ./images/linux/zynqmp_fsbl.elf --fpga ./images/linux/system.bit --u-boot --force
- copy bootloaders and filesystem to your sd:
	cp /your/project/directory/images/linux/BOOT.BIN /your/project/directory/images/linux/boot.scr /your/project/directory/images/linux/image.ub /media/*username*/*boot partirion's name*/
	sudo tar -xf ./images/linux/rootfs.tar.gz -C /media/*username*/*rootfs partirion's name*/
- insert sd into card, load system, check if device exists: ls -l /dev/ | grep spi
- if device exists in devfs, launch test: sudo /usr/bin/app-spi -D /dev/spidevX.Y -v (see X and Y in output of ls -l /dev/ | grep spi)


Setup from zero
Vivado

Petalinux
