#ifndef MOCK_DMA

#include <stdio.h>		// print log
#include <stdlib.h>		// memory allocation
#include <string.h>		// error code to string
#include <fcntl.h>		// open/close permissions
#include <unistd.h>		// open/close
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>

#include "axi_dma.h"


typedef struct AxiDma {
	int axi_dma;
} AxiDma_t;


int dma_setup(
	const AxiDma_t* const dma, const DmaSettings_t* const settings
) {
	if (dma == NULL || settings == NULL) {
		return -1;
	}

	if (ioctl(dma->axi_dma, IOCTL_DGD_AXI_DMA_LOG, 1)) {
		printf("Failed to setup logging\n");
		return -1;
	}
	if (ioctl(dma->axi_dma, IOCTL_DGD_AXI_DMA_RESET, 1)) {
		printf("Failed to reset DMA\n");
		return -1;
	}
	if (ioctl(dma->axi_dma, IOCTL_DGD_AXI_DMA_TRANSFER_BLOCK_SIZE, settings->transfer_size)) {
		printf("Failed to set transfer size\n");
		return -1;
	}
	if (ioctl(dma->axi_dma, IOCTL_DGD_AXI_DMA_SET_BLOCKS_COUNT_MM2S, settings->count_blocks)) {
		printf("Failed to set amount of blocks for MM2S\n");
		return -1;
	}
	if (ioctl(dma->axi_dma, IOCTL_DGD_AXI_DMA_SET_BLOCKS_COUNT_S2MM, settings->count_blocks)) {
		printf("Failed to set amount of blocks for S2MM\n");
		return -1;
	}
	if (ioctl(dma->axi_dma, IOCTL_DGD_AXI_BUFFERS_COUNT_MM2S, settings->count_buffers)) {
		printf("Failed to set amount of buffers MM2S\n");
		return -1;
	}
	if (ioctl(dma->axi_dma, IOCTL_DGD_AXI_BUFFERS_COUNT_S2MM, settings->count_buffers)) {
		printf("Failed to set amount of buffers S2MM\n");
		return -1;
	}
	if (ioctl(dma->axi_dma, IOCTL_DGD_AXI_DMA_ALLOCATE_BUFFERS, 1)) {
		printf("Failed to allocate DMA buffers\n");
		return -1;
	}
	return 0;
}

AxiDma_t* dma_create(
	const char* const device_name,
	const DmaSettings_t* const settings
) {
	AxiDma_t* dma = NULL;
	if (device_name == NULL || settings == NULL) {
		return NULL;
	}
	do {
		dma = calloc(sizeof(*dma), 1);
		if (dma == NULL) {
			printf("Failed to allocate memory for DMA: %s\n", strerror(errno));
			break;
		}
		dma->axi_dma = -1;
		dma->axi_dma = open(device_name, O_RDWR);
		if (dma->axi_dma < 0) {
			printf("Can't open file %s: %s\n", device_name, strerror(errno));
			break;
		}

		if (dma_setup(dma, settings)) {
			printf("Failed to setup dma\n");
			break;
		}
		return dma;
	} while(0);
	dma_release(&dma);
	return NULL;
}



int dma_release(AxiDma_t** const dma_p) {
	if (dma_p == NULL || *dma_p == NULL) {
		return 0;
	}
	AxiDma_t* dma = *dma_p;
	if (dma != NULL && dma->axi_dma > 0) {
		if (ioctl(dma->axi_dma, IOCTL_DGD_AXI_DMA_FREE_BUFFERS, 1)) {
			printf("Failed to free buffers\n");
			return -1;
		}

		close(dma->axi_dma);
		dma->axi_dma = -1;
	}
	if (dma != NULL) {
		free(dma);
	}
	*dma_p = NULL;
	return 0;
}

int dma_free_buffers(const AxiDma_t* const dma) {
	if (dma == NULL || dma->axi_dma < 0) {
		return -1;
	}
	return ioctl(dma->axi_dma, IOCTL_DGD_AXI_DMA_FREE_BUFFERS , 1);
}

int dma_start(const AxiDma_t* const dma) {
	if (dma == NULL || dma->axi_dma < 0) {
		return -1;
	}
	if (ioctl(dma->axi_dma, IOCTL_DGD_AXI_DMA_START_S2MM, 1)) {
		printf("Failed to start S2MM transfer\n");
		return -1;
	}
	if (ioctl(dma->axi_dma, IOCTL_DGD_AXI_DMA_START_MM2S, 1)) {
		printf("Failed to start MM2S transfer\n");
		return -1;
	}
	return 0;
}

int dma_get_current_buffer_idx(const AxiDma_t* const dma, DmaChannel_t type, unsigned int* const ptr_idx) {
	if (dma == NULL || dma->axi_dma < 0 || ptr_idx == NULL) {
		return -1;
	}
	if (type == S2MM) {
		if (ioctl(dma->axi_dma, IOCTL_DGD_AXI_DMA_IDX_S2MM, ptr_idx)) {
			printf("Failed to get index of current buffer S2MM\n");
			return -1;
		}
	}
	else if (type == MM2S) {
		if (ioctl(dma->axi_dma, IOCTL_DGD_AXI_DMA_IDX_MM2S, ptr_idx)) {
			printf("Failed to get index of current buffer MM2S\n");
			return -1;
		}
	} else {
		printf("Unsupported DMA channel type\n");
		return -1;
	}
	return 0;
}

void* dma_mmap_buffer(const AxiDma_t* const dma, DmaChannel_t type, unsigned int idx_buffer, unsigned long size_memory) {
	if (dma == NULL || dma->axi_dma < 0) {
		return MAP_FAILED;
	}

	if (type == S2MM) {
		if (ioctl(dma->axi_dma, IOCTL_DGD_AXI_DMA_MMAP_S2MM, 1)) {
			printf("Failed to select S2MM stream for memory mapping\n");
			return MAP_FAILED;
		}
	}
	else if (type == MM2S) {
		if (ioctl(dma->axi_dma, IOCTL_DGD_AXI_DMA_MMAP_MM2S, 1)) {
			printf("Failed to select MM2S stream for memory mapping\n");
			return MAP_FAILED;
		}
	}
	else {
		printf("Unsupported DMA channel type\n");
		return MAP_FAILED;
	}

	// set the buffer that you want to map into virtual address space of user
	if (ioctl(dma->axi_dma, IOCTL_DGD_AXI_DMA_MMAP_BUFFER_IDX, idx_buffer)) {
		printf("Failed to select buffer for memory mapping\n");
		return MAP_FAILED;
	}
	return mmap(NULL, size_memory, PROT_READ | PROT_WRITE, MAP_SHARED, dma->axi_dma, 0);
}


void dma_unmpap_buffer(const AxiDma_t* const dma, void** const memory, unsigned long memory_size) {
	if (dma == NULL || dma->axi_dma < 0 || memory == NULL || *memory == NULL) {
		return;
	}
	munmap(*memory, memory_size);
	*memory = NULL;
}

int dma_stop(const AxiDma_t* const dma) {
	if (dma == NULL || dma->axi_dma < 0) {
		return -1;
	}
	// not supported
	return -1; // ioctl(dma->axi_dma, IOCTL_RTG_MCDMA_STOP_S2MM, 1);
}

int dma_dump_registers(const AxiDma_t* const dma) {
	if (dma == NULL || dma->axi_dma < 0) {
		return -1;
	}
	if (ioctl(dma->axi_dma, IOCTL_DGD_AXI_DMA_DUMP_REGS, 1)) {
		printf("Failed to request dump\n");
		return -1;
	}
	return 0;
}

int dma_get_status(const AxiDma_t* const dma, DmaChannel_t type, unsigned long* const status) {
	if (dma == NULL || dma->axi_dma < 0) {
		return -1;
	}
	if (type == S2MM) {
		if (ioctl(dma->axi_dma, IOCTL_DGD_AXI_DMA_STATUS_S2MM, status)) {
			printf("Failed to request S2MM status\n");
			return -1;
		}
	}
	else if (type == MM2S) {
		if (ioctl(dma->axi_dma, IOCTL_DGD_AXI_DMA_STATUS_MM2S, status)) {
			printf("Failed to request MM2S status\n");
			return -1;
		}
	} else {
		printf("Unsupported DMA channel type\n");
		return -1;
	}
	return 0;
}

int dma_get_get_blocks_to_transfer(const AxiDma_t* const dma, DmaChannel_t type, unsigned long* const counter) {
	if (dma == NULL || dma->axi_dma < 0) {
		return -1;
	}
	if (type == S2MM) {
		if (ioctl(dma->axi_dma, IOCTL_DGD_AXI_DMA_GET_BLOCKS_COUNT_S2MM, counter)) {
			printf("Failed to request S2MM transfer blocks count\n");
			return -1;
		}
	}
	else if (type == MM2S) {
		if (ioctl(dma->axi_dma, IOCTL_DGD_AXI_DMA_GET_BLOCKS_COUNT_MM2S, counter)) {
			printf("Failed to request MM2S transfer blocks count\n");
			return -1;
		}
	} else {
		printf("Unsupported DMA channel type\n");
		return -1;
	}
	return 0;
}

#endif
