Xilinx tools version: 2022.2

Use code from repository
Vivado
Open project:
- launch Vivado
- in tcp console enter: 
	cd /path/to/repository/AxiQuadSpi/Vivado/ZCU104SPI
	source Zcu104DMA.tcl
	

Petalinux
- create a petalinux project: ```petalinux-create -t project -n Your_project's_name --template zynqMP```
- copy content of /path/to/repository/DMA/Petalinux/ZCU104-DMA into your project directory
- you can get some errors because libs or sstage pathes. To fix this, launch ```petalinux-config```, Yocto Settings -> Add pre-mirror url -> write a default value (http://petatalinux.xilinx.com/sswreleases/rel-v${PETALINUX_MAJOR_VER}/downloads) ; Yocto Settings -> Local sstate feeds and settings -> clear it (it must be empty by default)
- configurate it: ```petalinux-config --get-hw-description /your/project/directory/ZCU104-DMA.xsa```
- build it: ```petalinux-build```
- create bootloaders: ```petalinux-package --boot --format BIN --fsbl ./images/linux/zynqmp_fsbl.elf --fpga ./images/linux/system.bit --u-boot --force```
- copy bootloaders and filesystem to your sd:
	```cp /your/project/directory/images/linux/BOOT.BIN /your/project/directory/images/linux/boot.scr /your/project/directory/images/linux/image.ub /media/*username*/*boot partirion's name*/```
	```sudo tar -xf /your/project/directory/images/linux/rootfs.tar.gz -C /media/*username*/*rootfs partirion's name*/```
- insert sd into card, load system, check if device exists: ls -l /dev/ | grep axi-dma
- if device exists in devfs, launch test: sudo /usr/bin/app-dma


Setup from zero
Petalinux
- create a petalinux project: petalinux-create -t project -n Your_project's_name --template zynqMP
- configurate it: petalinux-config --get-hw-description /your/project/directory/ZCU104-DMA.xsa
- customize DMA node in device tree, to change driver (file Project'sDir/project-spec/meta-user/recipes-bsp/device-tree/files/system-user.dtsi):
```
/include/ "system-conf.dtsi"
/ {
	model = "ZynqMP ZCU104 RevC Dan's DMA";

	amba_pl@0 {
		dma@a0000000 {
			compatible = "dgd-axi-dma-1.00.a";
		};
	};
};
```
- add application for dma test: ```petalinux-create -t apps --name app-dma --template c --enable``` and copy files from ZCU104/DMA/Petalinux/ZCU104-DMA/project-spec/meta-user/recipes-apps/app-dma/ (directory with your app is placed Project'sDir/project-spec/meta-user/recipes-apps/app-dma/)
- add driver for dma device: ```petalinux-create -t modules --name dgd-axi-dma --template c --enable``` and copy files from ZCU104/DMA/Petalinux/ZCU104-DMA/project-spec/meta-user/recipes-modules/dgd-axi-dma/ (directory with your app is placed Project'sDir/project-spec/meta-user/recipes-modules/dgd-axi-dma/)

- build it: ```petalinux-build```
- create bootloaders: ```petalinux-package --boot --format BIN --fsbl ./images/linux/zynqmp_fsbl.elf --fpga ./images/linux/system.bit --u-boot --force```
- copy bootloaders and filesystem to your sd:
	```cp /your/project/directory/images/linux/BOOT.BIN /your/project/directory/images/linux/boot.scr /your/project/directory/images/linux/image.ub /media/*username*/*boot partirion's name*/```
	```sudo tar -xf /your/project/directory/images/linux/rootfs.tar.gz -C /media/*username*/*rootfs partirion's name*/```
- insert sd into card, load system, check if device exists: ls -l /dev/ | grep axi-dma
- if device exists in devfs, launch test: sudo /usr/bin/app-dma
