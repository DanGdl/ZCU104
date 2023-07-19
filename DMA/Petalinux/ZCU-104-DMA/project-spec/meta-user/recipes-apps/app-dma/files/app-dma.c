#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include "axi_dma.h"

#define COUNT_BUFFERS	1



void await_termination(const AxiDma_t* const dma) {
	uint8_t is_done = 0;
	do {
		dma_dump_registers(dma);
		
		unsigned long blocks_to_transfer = 0;
		if (dma_get_get_blocks_to_transfer(dma, S2MM, &blocks_to_transfer)) {
			continue;
		}
		is_done = blocks_to_transfer == 0;
		if (!is_done) {
			continue;
		}
		blocks_to_transfer = 0;
		if (dma_get_get_blocks_to_transfer(dma, MM2S, &blocks_to_transfer)) {
			continue;
		}
		is_done = blocks_to_transfer == 0;
	} while(!is_done);
}

int main(void) {
	int32_t* buffers_s2mm[COUNT_BUFFERS] = { MAP_FAILED };
	int32_t* buffers_mm2s[COUNT_BUFFERS] = { MAP_FAILED };

	const int page_size = getpagesize();
	printf("Memory page size %d\n", page_size);
	
	const DmaSettings_t settings = {
		.transfer_size = page_size, // can't be smaller then memory page
		.count_blocks = COUNT_BUFFERS,
		.count_buffers = COUNT_BUFFERS,
	};
	// map from bytes to ints
	const size_t size_buffer = settings.transfer_size / sizeof(*(buffers_s2mm[0]));

	char name[56] = { 0 };
	AxiDma_t* dma = NULL;
	do {
		const int rc = sprintf(name, TEMPLATE_PATH_DEVICE, 0);
		if (rc != SIZE_TEMPLATE_PATH_DEVICE) {
			printf("Failed to setup path to device. Expected %d, got %d\n", SIZE_TEMPLATE_PATH_DEVICE, rc);
			break;
		}
		dma = dma_create(name, &settings);
		if (dma == NULL) {
			break;
		}

		uint8_t mmap_ok = 1;
		for (int i = 0 ; i < settings.count_buffers; i++) {
			buffers_s2mm[i] = (int32_t*) dma_mmap_buffer(dma, S2MM, i, settings.transfer_size);
			if (buffers_s2mm[i] == MAP_FAILED) {
				mmap_ok = 0;
				printf("Failed to map S2MM buffer %d\n", i);
				i = settings.count_buffers;
			}
			buffers_mm2s[i] = (int32_t*) dma_mmap_buffer(dma, MM2S, i, settings.transfer_size);
			if (buffers_mm2s[i] == MAP_FAILED) {
				mmap_ok = 0;
				printf("Failed to map MM2S buffer %d\n", i);
				i = settings.count_buffers;
			}
		}
		if (!mmap_ok) {
			break;
		}

		int32_t counter = 200;
		for (int i = 0 ; i < settings.count_buffers; i++) {
			int32_t* buffer = buffers_mm2s[i];
			for (int j = 0; j < size_buffer; j++) {
				buffer[j] = counter;
				counter++;
			}
		}

		printf("Registers before start\n");
		dma_dump_registers(dma);
		
		if (dma_start(dma)) {
			break;
		}

		await_termination(dma);
		
		printf("Registers after termination\n");
		dma_dump_registers(dma);
		
		uint8_t is_ok = 1;
		counter = 200;
		for (int i = 0 ; i < settings.count_buffers; i++) {
			int32_t* buffer = buffers_s2mm[i];
			for (int j = 0; j < size_buffer; j++) {
				if (is_ok && buffer[j] != counter) {
					is_ok = 0;
					printf("Diff found at buffer %d, idx %d: expected %d, got %d\n", i, j, counter, buffer[j]);
				}
				counter++;
			}
		}
		if (is_ok) {
			printf("S2MM buffers are same as MM2S\n");
		}
		else {
			printf("S2MM buffers are different from MM2S\n");
		}
	} while(0);


	if (dma != NULL) {
		for (int i = 0 ; i < settings.count_buffers; i++) {
			if (buffers_s2mm[i] != MAP_FAILED) {
				dma_unmpap_buffer(dma, (void**) &buffers_s2mm[i], settings.transfer_size);
			}
			if (buffers_mm2s[i] != MAP_FAILED) {
				dma_unmpap_buffer(dma, (void**) &buffers_mm2s[i], settings.transfer_size);
			}
		}
		while (dma_release(&dma)) {
		}
	}
    return 0;
}
