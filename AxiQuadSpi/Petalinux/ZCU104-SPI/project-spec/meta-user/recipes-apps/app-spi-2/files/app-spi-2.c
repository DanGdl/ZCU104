#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>


// To read data from an SPI (Serial Peripheral Interface) device in Linux, you can use the spidev driver. Here are the basic steps:
//
// Open the spidev device file using the open system call. The device file is usually located at /dev/spidevX.Y, where X is the SPI bus number and Y is the chip select (slave) number.
//
// Set the SPI mode, which determines the clock polarity and phase, using the ioctl system call. The mode is specified as a bit field with the following options:
//
// SPI_CPOL: Clock polarity (0 or 1)
// SPI_CPHA: Clock phase (0 or 1)
// SPI_MODE_0: CPOL=0, CPHA=0
// SPI_MODE_1: CPOL=0, CPHA=1
// SPI_MODE_2: CPOL=1, CPHA=0
// SPI_MODE_3: CPOL=1, CPHA=1
//
// Set the maximum transfer speed using the SPI_IOC_WR_MAX_SPEED_HZ ioctl. This determines the maximum clock frequency that the SPI bus will use.
//
// Use the read system call to read data from the device. The read call takes a buffer and a length as arguments and returns the number of bytes read.
//
// Here is some sample code to demonstrate the basic process:

void main(void) {
	uint8_t tx[] = "HELLO DUDES!!"; // { 0x01, 0x02, 0x03, 0x04 };
	uint8_t rx[sizeof(tx)/sizeof(tx[0])] = { 0 };
	uint32_t speed = 1000000; // 1 MHz (value, that we set in output clock for SPI in vivado)
	uint32_t bits = 8;
	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long) tx, // mosi
		.rx_buf = (unsigned long) rx, // miso
		.len = sizeof(tx),
		.delay_usecs = 0,
		.speed_hz = speed,
		.bits_per_word = bits,
	};

	int fd = -1;
	do {
		fd = open("/dev/spidev1.0", O_RDWR);
		if (fd < 0) {
			printf("Failed to open device: %s\n", strerror(errno));
			break;
		}

		// Set SPI mode to mode 0
		uint8_t mode = SPI_MODE_0;
		if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0) {
			printf("Failed to set SPI mode 0: %s\n", strerror(errno));
			break;
		}

		// Set maximum transfer speed
		if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
			printf("Failed to set SPI speed: %s\n", strerror(errno));
			break;
		}

		if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
			printf("Failed to set SPI bits_per_word: %s\n", strerror(errno));
			break;
		}

		// Read data from SPI device
		if (ioctl(fd, SPI_IOC_MESSAGE(1), &tr) < 0) {
			printf("Failed to write message to SPI: %s\n", strerror(errno));
			break;
		}

		// Print received data
		printf("Received: ");
		for (int i = 0; i < sizeof(rx); i++) {
			printf(" %02X", rx[i]);
		}
		printf("\n");
	} while(0);

	if (fd > 0) {
		close(fd);
	}
}
