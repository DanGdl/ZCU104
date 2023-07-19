#ifndef AXI_DMA_WRAPPER_
#define AXI_DMA_WRAPPER_

#include <stdint.h>
#include "dgd-axi-dma.h"

#define TEMPLATE_PATH_DEVICE 		"/dev/"DRIVER_NAME"_%d0"
#define SIZE_TEMPLATE_PATH_DEVICE	(sizeof(TEMPLATE_PATH_DEVICE) / sizeof(TEMPLATE_PATH_DEVICE[0]) - 2)

typedef struct AxiDma AxiDma_t;

typedef struct DmaSettings {
	const unsigned long transfer_size;
	const unsigned long count_blocks;
	const unsigned long count_buffers;
} DmaSettings_t;

typedef enum DmaChannel {
	S2MM, MM2S
} DmaChannel_t;



AxiDma_t* dma_create(
	const char* const device_name,
	const DmaSettings_t* const settings
);

int dma_free_buffers(const AxiDma_t* const dma);

int dma_release(AxiDma_t** const dma);


int dma_start(const AxiDma_t* const dma);

int dma_stop(const AxiDma_t* const dma);

int dma_get_current_buffer_idx(const AxiDma_t* const dma, DmaChannel_t type, unsigned int* const ptr_idx);

int dma_dump_registers(const AxiDma_t* const dma);

int dma_get_status(const AxiDma_t* const dma, DmaChannel_t type, unsigned long* const status);

int dma_get_get_blocks_to_transfer(const AxiDma_t* const dma, DmaChannel_t type, unsigned long* const counter);


void* dma_mmap_buffer(
	const AxiDma_t* const dma, DmaChannel_t type, unsigned int idx_buffer, unsigned long size_memory
);

void dma_unmpap_buffer(const AxiDma_t* const dma, void** const memory, unsigned long memory_size);

#endif
