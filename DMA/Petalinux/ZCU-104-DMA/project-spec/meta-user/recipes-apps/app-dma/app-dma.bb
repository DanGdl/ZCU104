#
# This file is the app-dma recipe.
#

SUMMARY = "Simple app-dma application"
SECTION = "PETALINUX/apps"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://dgd-axi-dma.h \
		file://axi_dma.h \
		file://axi_dma.c \
		file://app-dma.c \
	    file://Makefile \
		  "

S = "${WORKDIR}"

do_compile() {
	     oe_runmake
}

do_install() {
	     install -d ${D}${bindir}
	     install -m 0755 app-dma ${D}${bindir}
}
