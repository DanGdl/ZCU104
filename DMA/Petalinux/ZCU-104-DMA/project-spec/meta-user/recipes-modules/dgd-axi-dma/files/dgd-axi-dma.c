
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/fcntl.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/mutex.h>
#include <linux/sysctl.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>

#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>

#include "dgd-axi-dma.h"

#define TAG_LOG 		"DGD_DMA"
#define TAG_AXI_DMA_DTS	"dgd-axi-dma-1.00.a"

#define TEMPLATE_NAME_DRIVER			"%s_%d"
#define TEMPLATE_NAME_DRIVER_REPLACED	4
#define TEMPLATE_NAME_IRQ				"%s_%d_%s"
#define TEMPLATE_NAME_IRQ_REPLACED		6


#define len_arr(arr) (sizeof(arr)/sizeof(arr[0]))

// DMA register offset values (DMA in register mode.)
#define AXI_DMA_CR_MM2S 		0x0
#define AXI_DMA_SR_MM2S 		0x4

#define AXI_DMA_CR_S2MM			0x30
#define AXI_DMA_SR_S2MM			0x34

#define AXI_DMA_SRC_MM2S_LSB	0x18 // for physical address of source buffer
#define AXI_DMA_SRC_MM2S_MSB	0x1C

#define AXI_DMA_DEST_S2MM_LSB	0x48 // for physical address of destination buffer
#define AXI_DMA_DEST_S2MM_MSB	0x4C

#define AXI_DMA_LEN_MM2S		0x28 // for MM2S buffer's length
#define AXI_DMA_LEN_S2MM		0x58 // for S2MM buffer's length


typedef struct DmaBuffer {
	unsigned int idx_current;		// buffer pointer for s2mm transfers
	size_t size;
	dma_addr_t* phy;
	void** virt;
} DmaBuffer_t;

typedef struct DeviceInfo {
	unsigned long mem_start;
	unsigned long mem_end;
	void __iomem* base_addr;
	int irq_mm2s;
	int irq_s2mm;

	unsigned int dma_device_number;				// device number for dma.
	unsigned int print_log;
	struct class* driver_class;
	struct device* pl_dev;
	struct device* dev;
	struct cdev cdev;
	dev_t devt;
	
	unsigned int mmap_s2mm; 					// when 1, do mmap for s2mm buffers
	unsigned int mmap_mm2s;						// when 1, do mmap for mm2s buffers
	unsigned int mmap_idx; 						// which buffer number mmap should use for mapping?

	unsigned int count_transfer_blocks_s2mm;	// total number of blocks to transfer in each s2mm transfer task
	unsigned int count_transfer_blocks_mm2s; 	// total number of blocks to transfer in each mm2s transfer task
	
	unsigned int size_buffer; 					// size of one transfer buffer. max value is 4 MB
	DmaBuffer_t buffers_mm2s;
	DmaBuffer_t buffers_s2mm;
} DeviceInfo_t;


static int probe_counter = 0; 

static ssize_t dgd_axi_dma_write(struct file* file, const char __user* buf, size_t count, loff_t* ppos)  {
	return count;
}

static ssize_t dgd_axi_dma_read(struct file* file, char __user* buf, size_t count, loff_t* ppos)  {
	return count;
}

static int dgd_axi_dma_open(struct inode* inode, struct file* file)  {
	DeviceInfo_t* dvc = container_of(inode->i_cdev, DeviceInfo_t, cdev);
	file->private_data = dvc;
	return 0; 
}

static int dgd_axi_dma_release(struct inode* inode, struct file* file)  {
	return 0;
}



static void free_buffers(DeviceInfo_t* const dvc) {
	int i = 0;
	if (dvc->buffers_mm2s.virt && dvc->buffers_mm2s.phy) {
		for (i = 0; i < dvc->buffers_mm2s.size; i++ )  {
			 dma_free_coherent(dvc->pl_dev, dvc->size_buffer, dvc->buffers_mm2s.virt[i], dvc->buffers_mm2s.phy[i]);
		}
	}
	if (dvc->buffers_mm2s.virt) {
		kfree(dvc->buffers_mm2s.virt);
		dvc->buffers_mm2s.virt = NULL;
	}
	if (dvc->buffers_mm2s.phy) {
		kfree(dvc->buffers_mm2s.phy);
		dvc->buffers_mm2s.phy = NULL;
	}

	if (dvc->buffers_s2mm.virt && dvc->buffers_s2mm.phy) {
		for (i = 0; i < dvc->buffers_mm2s.size; i++ )  {
			 dma_free_coherent(dvc->pl_dev, dvc->size_buffer, dvc->buffers_s2mm.virt[i], dvc->buffers_s2mm.phy[i]);
		}
	}
	if (dvc->buffers_s2mm.virt) {
		kfree(dvc->buffers_s2mm.virt);
		dvc->buffers_s2mm.virt = NULL;
	}
	if (dvc->buffers_s2mm.phy) {
		kfree(dvc->buffers_s2mm.phy);
		dvc->buffers_s2mm.phy = NULL;
	}
	printk(TAG_LOG": free buffers done\n");
}

static int has_buffers(const DeviceInfo_t* const dvc) {
	return dvc->buffers_mm2s.virt != NULL || dvc->buffers_mm2s.phy != NULL
			|| dvc->buffers_s2mm.virt != NULL || dvc->buffers_s2mm.phy != NULL;
}

static int dma_running(const DeviceInfo_t* const dvc) {
	const int status_mm2s = ioread32(dvc->base_addr + AXI_DMA_SR_MM2S);
	const int status_s2mm = ioread32(dvc->base_addr + AXI_DMA_SR_S2MM);
	return ((status_mm2s & DGD_AXI_DMA_STATUS_HALTED) || (status_mm2s & DGD_AXI_DMA_STATUS_IDLE))
			&& ((status_s2mm & DGD_AXI_DMA_STATUS_HALTED) || (status_s2mm & DGD_AXI_DMA_STATUS_IDLE));
}

static int dma_idle(const DeviceInfo_t* const dvc) {
	const int status_mm2s = ioread32(dvc->base_addr + AXI_DMA_SR_MM2S);
	const int status_s2mm = ioread32(dvc->base_addr + AXI_DMA_SR_S2MM);
	return (status_mm2s & DGD_AXI_DMA_STATUS_IDLE) && (status_s2mm & DGD_AXI_DMA_STATUS_IDLE);
}

static long dgd_axi_dma_ioctl(struct file* filep, unsigned int cmd, unsigned long arg)  {
	DeviceInfo_t* const dvc = filep->private_data;
 	unsigned int tmpVal; 
	int i = 0;

	switch (cmd) {
		case IOCTL_DGD_AXI_DMA_LOG:
			dvc->print_log = arg;
			printk(TAG_LOG": logging is %s\n", arg ? "ON" : "OFF");
			break; 
		
		case IOCTL_DGD_AXI_DMA_TRANSFER_BLOCK_SIZE:
			if (has_buffers(dvc)) {
				printk(TAG_LOG": buffers already allocated. Free them before change size\n");
				return -1;
			}
			else if (dma_running(dvc)) {
				printk(TAG_LOG": DMA running. Stop it before change size\n");
				return -1;
			}
			dvc->size_buffer = arg;
			if (dvc->print_log) {
				printk(TAG_LOG": buffer size %u\n", dvc->size_buffer);
			}
			break;

		case IOCTL_DGD_AXI_DMA_GET_BLOCKS_COUNT_MM2S:
			tmpVal = dvc->count_transfer_blocks_mm2s;
			__put_user(tmpVal, (unsigned int __user*) arg);
			break;

		case IOCTL_DGD_AXI_DMA_GET_BLOCKS_COUNT_S2MM:
			tmpVal = dvc->count_transfer_blocks_s2mm;
			__put_user(tmpVal, (unsigned int __user*) arg);
			break;
			
		case IOCTL_DGD_AXI_DMA_SET_BLOCKS_COUNT_MM2S:
			if (has_buffers(dvc)) {
				printk(TAG_LOG": buffers already allocated. Free them before change size\n");
				return -1;
			}
			else if (dma_running(dvc)) {
				printk(TAG_LOG": DMA running. Stop it before change size\n");
				return -1;
			}
			dvc->count_transfer_blocks_mm2s = arg;
			if (dvc->print_log) {
				printk(TAG_LOG": mm2s blocks to transfer %d\n", dvc->count_transfer_blocks_mm2s);
			}
			break;

		case IOCTL_DGD_AXI_DMA_SET_BLOCKS_COUNT_S2MM:
			if (has_buffers(dvc)) {
				printk(TAG_LOG": buffers already allocated. Free them before change size\n");
				return -1;
			}
			else if (dma_running(dvc)) {
				printk(TAG_LOG": DMA running. Stop it before change size\n");
				return -1;
			}
			dvc->count_transfer_blocks_s2mm = arg;
			if (dvc->print_log) {
				printk(TAG_LOG": s2mm blocks to transfer %d\n", dvc->count_transfer_blocks_s2mm);
			}
			break;

		case IOCTL_DGD_AXI_DMA_STATUS_S2MM:
			tmpVal = ioread32(dvc->base_addr + AXI_DMA_SR_S2MM);
			__put_user(tmpVal, (unsigned int __user*) arg);
			break;

		case IOCTL_DGD_AXI_DMA_STATUS_MM2S:
			tmpVal = ioread32(dvc->base_addr + AXI_DMA_SR_MM2S);
			__put_user(tmpVal, (unsigned int __user*) arg);
			break;

		case IOCTL_DGD_AXI_DMA_IDX_S2MM:
			tmpVal = dvc->buffers_s2mm.idx_current;
			__put_user(tmpVal, (unsigned int __user*) arg);
			if (dvc->print_log) {
				printk(TAG_LOG": s2mm_pointer is pointing to buffer %d\n", dvc->buffers_s2mm.idx_current);
			}
			break;

		case IOCTL_DGD_AXI_DMA_IDX_MM2S:
			tmpVal = dvc->buffers_mm2s.idx_current;
			__put_user(tmpVal, (unsigned int __user*) arg);
			if (dvc->print_log) {
				printk(TAG_LOG": mm2s_pointer is pointing to buffer %d\n", dvc->buffers_mm2s.idx_current);
			}
			break;

		case IOCTL_DGD_AXI_DMA_MMAP_S2MM:
			dvc->mmap_s2mm = arg;
			dvc->mmap_mm2s = !arg;
			if (dvc->print_log) {
				printk(TAG_LOG": s2mm buffers selected for mapping\n");
			}
			break;

		case IOCTL_DGD_AXI_DMA_MMAP_MM2S:
			dvc->mmap_mm2s = arg;
			dvc->mmap_s2mm = !arg;
			if (dvc->print_log) {
				printk(TAG_LOG": mm2s buffers selected for mapping\n");
			}
			break;

		case IOCTL_DGD_AXI_DMA_MMAP_BUFFER_IDX:
			dvc->mmap_idx = arg;
			if (dvc->print_log) {
				printk(TAG_LOG": buffer %d selected for mapping\n", dvc->mmap_idx);
			}
			break;

		case IOCTL_DGD_AXI_DMA_DUMP_REGS:
			printk(TAG_LOG": mm2s CR: 0x%08X, SR 0x%08X, len 0x%08X, addr lsb 0x%08X, msb 0x%08X\n",
				   ioread32(dvc->base_addr + AXI_DMA_CR_MM2S),
				   ioread32(dvc->base_addr + AXI_DMA_SR_MM2S),
				   ioread32(dvc->base_addr + AXI_DMA_LEN_MM2S),
				   ioread32(dvc->base_addr + AXI_DMA_SRC_MM2S_LSB),
				   ioread32(dvc->base_addr + AXI_DMA_SRC_MM2S_MSB)
			);
			
			printk(TAG_LOG": s2mm CR: 0x%08X, SR 0x%08X, len 0x%08X, addr lsb 0x%08X, msb 0x%08X\n",
				   ioread32(dvc->base_addr + AXI_DMA_CR_S2MM),
				   ioread32(dvc->base_addr + AXI_DMA_SR_S2MM),
				   ioread32(dvc->base_addr + AXI_DMA_LEN_S2MM),
				   ioread32(dvc->base_addr + AXI_DMA_DEST_S2MM_MSB),
				   ioread32(dvc->base_addr + AXI_DMA_SRC_MM2S_MSB)
			);
			break;

		case IOCTL_DGD_AXI_DMA_RESET:
			// put both of the mm2s and s2mm channels in reset
			iowrite32(0x4, dvc->base_addr + AXI_DMA_CR_S2MM);
			iowrite32(0x4, dvc->base_addr + AXI_DMA_CR_MM2S);
			if (dvc->print_log) {
				printk(TAG_LOG": DMA reset is done! CR mm2s 0x%08X, s2mm 0x%08X\n", ioread32(dvc->base_addr + AXI_DMA_CR_MM2S), ioread32(dvc->base_addr + AXI_DMA_CR_S2MM));
			}
			// setup s2mm channel
			tmpVal = ioread32(dvc->base_addr + AXI_DMA_CR_S2MM);
			tmpVal = tmpVal | 0x1001;
			iowrite32(tmpVal, dvc->base_addr  + AXI_DMA_CR_S2MM);

			// double check if the value is set
			tmpVal = ioread32(dvc->base_addr  + AXI_DMA_CR_S2MM);
			if (dvc->print_log) {
				printk(TAG_LOG": value for DMA CR for s2mm is 0x%08X, SR 0x%08X\n", tmpVal, ioread32(dvc->base_addr + AXI_DMA_SR_S2MM));
			}
			// setup mm2s channel
			tmpVal = ioread32(dvc->base_addr + AXI_DMA_CR_MM2S);
			tmpVal = tmpVal | 0x1001;
			iowrite32(tmpVal, dvc->base_addr + AXI_DMA_CR_MM2S);

			// double check if the value is set
			tmpVal = ioread32(dvc->base_addr + AXI_DMA_CR_MM2S);
			if (dvc->print_log) {
				printk(TAG_LOG": value for DMA CR for mm2s is 0x%08X, SR 0x%08X\n", tmpVal, ioread32(dvc->base_addr + AXI_DMA_SR_MM2S));
			}
			break;

		case IOCTL_DGD_AXI_BUFFERS_COUNT_MM2S:
			if (has_buffers(dvc)) {
				printk(TAG_LOG": buffers already allocated. Free them before change size\n");
				return -1;
			}
			else if (dma_running(dvc)) {
				printk(TAG_LOG": DMA running. Stop it before change size\n");
				return -1;
			}
			dvc->buffers_mm2s.size = arg;
			if (dvc->print_log) {
				printk(TAG_LOG": number of mm2s buffers %lu\n", dvc->buffers_mm2s.size);
			}
			break; 
		
		case IOCTL_DGD_AXI_BUFFERS_COUNT_S2MM:
			if (has_buffers(dvc)) {
				printk(TAG_LOG": buffers already allocated. Free them before change size\n");
				return -1;
			}
			else if (dma_running(dvc)) {
				printk(TAG_LOG": DMA running. Stop it before change size\n");
				return -1;
			}
			dvc->buffers_s2mm.size = arg;
			if (dvc->print_log) {
				printk(TAG_LOG": number of s2mm buffers %lu\n", dvc->buffers_s2mm.size);
			}
			break; 
		
		case IOCTL_DGD_AXI_DMA_START_MM2S:
			if (!has_buffers(dvc)) {
				printk(TAG_LOG": buffers are not allocated\n");
				return -1;
			}
			else if (dma_running(dvc)) {
				printk(TAG_LOG": DMA already running\n");
				return -1;
			}
			dvc->buffers_mm2s.idx_current = 0;
			if (dvc->print_log) {
				printk(TAG_LOG": starting mm2s transfer length: %u, source address: 0x%llx\n", dvc->size_buffer, dvc->buffers_mm2s.phy[dvc->buffers_mm2s.idx_current]);
			}
			iowrite32(0, dvc->base_addr + AXI_DMA_SRC_MM2S_MSB);
			iowrite32(dvc->buffers_mm2s.phy[dvc->buffers_mm2s.idx_current], dvc->base_addr + AXI_DMA_SRC_MM2S_LSB);
			iowrite32(dvc->size_buffer, dvc->base_addr + AXI_DMA_LEN_MM2S);
			
			if (dvc->print_log) {
				printk(TAG_LOG": start mm2s CR: 0x%08X, SR 0x%08X, len 0x%08X, addr lsb 0x%08X, msb 0x%08X\n",
					ioread32(dvc->base_addr + AXI_DMA_CR_MM2S),
					ioread32(dvc->base_addr + AXI_DMA_SR_MM2S),
					ioread32(dvc->base_addr + AXI_DMA_LEN_MM2S),
					ioread32(dvc->base_addr + AXI_DMA_SRC_MM2S_LSB),
					ioread32(dvc->base_addr + AXI_DMA_SRC_MM2S_MSB)
				);
			}
			break;

		case IOCTL_DGD_AXI_DMA_START_S2MM:
			if (!has_buffers(dvc)) {
				printk(TAG_LOG": buffers are not allocated\n");
				return -1;
			}
			else if (dma_running(dvc)) {
				printk(TAG_LOG": DMA already running\n");
				return -1;
			}
			dvc->buffers_s2mm.idx_current = 0;
			if (dvc->print_log) {
				printk(TAG_LOG": starting s2mm transfer length: %u, source address: 0x%llx\n", dvc->size_buffer, dvc->buffers_s2mm.phy[dvc->buffers_s2mm.idx_current]);
			}
			iowrite32(0, dvc->base_addr + AXI_DMA_DEST_S2MM_MSB);
			iowrite32(dvc->buffers_s2mm.phy[dvc->buffers_s2mm.idx_current], dvc->base_addr + AXI_DMA_DEST_S2MM_LSB);
			iowrite32(dvc->size_buffer, dvc->base_addr + AXI_DMA_LEN_S2MM);
			
			if (dvc->print_log) {
				printk(TAG_LOG": start s2mm CR: 0x%08X, SR 0x%08X, len 0x%08X, addr lsb 0x%08X, msb 0x%08X\n",
					ioread32(dvc->base_addr + AXI_DMA_CR_S2MM),
					ioread32(dvc->base_addr + AXI_DMA_SR_S2MM),
					ioread32(dvc->base_addr + AXI_DMA_LEN_S2MM),
					ioread32(dvc->base_addr + AXI_DMA_DEST_S2MM_MSB),
					ioread32(dvc->base_addr + AXI_DMA_SRC_MM2S_MSB)
				);
			}
			break;

		case IOCTL_DGD_AXI_DMA_ALLOCATE_BUFFERS:
			if (has_buffers(dvc)) {
				printk(TAG_LOG": buffers already allocated. Free them before change allocate new buffers\n");
				return -1;
			}
			else if (dma_running(dvc)) {
				printk(TAG_LOG": DMA already running\n");
				return -1;
			}
			dvc->buffers_mm2s.virt = kzalloc(sizeof(*dvc->buffers_mm2s.virt) * dvc->buffers_mm2s.size, GFP_KERNEL);
			if (dvc->buffers_mm2s.virt == NULL) {
				printk(TAG_LOG": failed to allocate memory for virtual addresses mm2s\n");
				free_buffers(dvc);
				return -1;
			}
			
			dvc->buffers_mm2s.phy = (dma_addr_t*) kzalloc(sizeof(*dvc->buffers_mm2s.phy) * dvc->buffers_mm2s.size, GFP_KERNEL);
			if (dvc->buffers_mm2s.phy == NULL) {
				printk(TAG_LOG": failed to allocate memory for physical addresses mm2s\n");
				free_buffers(dvc);
				return -1;
			}
			
			for (i = 0; i < dvc->buffers_mm2s.size; i++)  {
				dvc->buffers_mm2s.virt[i] = dma_alloc_coherent(dvc->pl_dev, dvc->size_buffer, dvc->buffers_mm2s.phy + i, GFP_KERNEL);

				if (dvc->buffers_mm2s.virt[i] == NULL) {
					printk(TAG_LOG": failed to allocate coherent mm2s buffer %d\n", i);
					free_buffers(dvc);
					return -1;
				}
				else if (dvc->print_log) {
					printk(TAG_LOG": mm2s buffer %03d physical address: 0x%016llx, virtual address: 0x%016lx\n", i, dvc->buffers_mm2s.phy[i], dvc->buffers_mm2s.virt[i]);
				}
			}
			
			dvc->buffers_s2mm.virt = kzalloc(sizeof(*dvc->buffers_s2mm.virt) * dvc->buffers_s2mm.size, GFP_KERNEL);
			if (dvc->buffers_s2mm.virt == NULL) {
				printk(TAG_LOG": failed to allocate memory for virtual addresses s2mm\n");
				free_buffers(dvc);
				return -1;
			}
			
			dvc->buffers_s2mm.phy = (dma_addr_t*) kzalloc(sizeof(*dvc->buffers_s2mm.phy) * dvc->buffers_s2mm.size, GFP_KERNEL);
			if (dvc->buffers_s2mm.phy == NULL) {
				printk(TAG_LOG": failed to allocate memory for physical addresses s2mm\n");
				free_buffers(dvc);
				return -1;
			}
			
			for (i = 0; i < dvc->buffers_s2mm.size; i++)  {
				dvc->buffers_s2mm.virt[i] = dma_alloc_coherent(dvc->pl_dev, dvc->size_buffer, dvc->buffers_s2mm.phy + i, GFP_KERNEL);

				if (dvc->buffers_s2mm.virt[i] == NULL) {
					printk(TAG_LOG": failed to allocate coherent s2mm buffer %d\n", i);
					free_buffers(dvc);
					return -1;
				}
				else if (dvc->print_log) {
					printk(TAG_LOG": s2mm buffer %03d physical address: 0x%016llx, virtual address: 0x%016lx\n", i, dvc->buffers_s2mm.phy[i], dvc->buffers_s2mm.virt[i]);
				}
			}
			break; 
		
		case IOCTL_DGD_AXI_DMA_FREE_BUFFERS:
			if (!dma_idle(dvc)) {
				printk(TAG_LOG": Can't free buffers! DMA running!! \n");
				return -1;
			}
			free_buffers(dvc);
			break; 
		
		default:
			printk(TAG_LOG": unsupported command %d\n", cmd);
	}
	return 0;
}


static int dgd_axi_dma_mmap(struct file* filep, struct vm_area_struct* vma) {
	DeviceInfo_t* const dvc = filep->private_data;
	const unsigned int size = vma->vm_end - vma->vm_start;
	unsigned int addr = 0;
	if (size > dvc->size_buffer)  {
		printk(KERN_INFO TAG_LOG": Requested mmap region size is bigger than DMA buffer. Requested = %u, buffer = %u\n",
				size, dvc->size_buffer
		);
		return -1;
	}
	
	if (!has_buffers(dvc)) {
		printk(TAG_LOG": buffers are not allocated\n");
		return -1;
	}

	if (dvc->mmap_s2mm) {
		if (dvc->mmap_idx >= dvc->buffers_s2mm.size) {
			printk(TAG_LOG": index %u is out of bound. Size is %lu\n", dvc->mmap_idx, dvc->buffers_s2mm.size);
			return -1;
		}
		addr = dvc->buffers_s2mm.phy[dvc->mmap_idx];
		printk(TAG_LOG": map S2MM buffer %d. Address 0x%016x\n", dvc->mmap_idx, addr);
	}
	else if (dvc->mmap_mm2s) {
		if (dvc->mmap_idx >= dvc->buffers_mm2s.size) {
			printk(TAG_LOG": index %u is out of bound. Size is %lu\n", dvc->mmap_idx, dvc->buffers_mm2s.size);
			return -1;
		}
		addr = dvc->buffers_mm2s.phy[dvc->mmap_idx];
		printk(TAG_LOG": map MM2S buffer %d. Address 0x%016x\n", dvc->mmap_idx, addr);
	}
	else {
		printk(TAG_LOG": buffer for memory mapping is not selected\n");
		return -1;
	}
	addr = vma->vm_pgoff + (addr >> PAGE_SHIFT);
	vma->vm_flags |= (VM_DONTEXPAND | VM_DONTDUMP);
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	if (io_remap_pfn_range(vma, vma->vm_start, addr, size, vma->vm_page_prot)) {
		printk(KERN_INFO TAG_LOG": memory mapping failed\n");
		return -1;
	}
	if (dvc->print_log) {
		printk(KERN_INFO TAG_LOG": memory mapping OK. Virtual 0x%016lx, physical 0x%016x\n", vma->vm_start, addr << PAGE_SHIFT);
	}
	return 0;
}


static const struct file_operations driver_fops = {
	.owner 			= THIS_MODULE,
	.write 			= dgd_axi_dma_write,
	.read 			= dgd_axi_dma_read,
	.open 			= dgd_axi_dma_open,
	.release 		= dgd_axi_dma_release,
	.unlocked_ioctl = dgd_axi_dma_ioctl,
	.mmap 			= dgd_axi_dma_mmap,
};


static irqreturn_t dgd_axi_dma_irq_mm2s(int irq, void* lp) {
	DeviceInfo_t* const dvc = lp;
	
	iowrite32(0x1000, dvc->base_addr + AXI_DMA_SR_MM2S);
	
	if (dvc->print_log) {
		printk(KERN_DEFAULT TAG_LOG": DMA %d received mm2s interrupt. Current mm2s pointer: %d. Blocks to transfer %d\n",
			   dvc->dma_device_number, dvc->buffers_mm2s.idx_current, dvc->count_transfer_blocks_mm2s
		);
	}
	// mm2s is read from memory. update the read pointer
	dvc->buffers_mm2s.idx_current = (dvc->buffers_mm2s.idx_current + 1) % (dvc->buffers_mm2s.size);

	// determine if you should start a new mm2s operation.
	dvc->count_transfer_blocks_mm2s = dvc->count_transfer_blocks_mm2s - 1;
	if (dvc->count_transfer_blocks_mm2s > 0) {
		// start the next DMA block transfer
		iowrite32(dvc->buffers_mm2s.phy[dvc->buffers_mm2s.idx_current], dvc->base_addr + AXI_DMA_SRC_MM2S_LSB);
		iowrite32(dvc->size_buffer, dvc->base_addr + AXI_DMA_LEN_MM2S);
	}
	return IRQ_HANDLED;
}

static irqreturn_t dgd_axi_dma_irq_s2mm(int irq, void* lp) {
	DeviceInfo_t* const dvc = lp;
	
	// clear interrupt
	iowrite32(0x1000, dvc->base_addr + AXI_DMA_SR_S2MM);
	
	if (dvc->print_log) {
		printk(KERN_DEFAULT TAG_LOG": DMA %d received s2mm interrupt. Current s2mm pointer: %d. Blocks to transfer %d\n",
			   dvc->dma_device_number, dvc->buffers_s2mm.idx_current, dvc->count_transfer_blocks_s2mm
		);
	}
	// update the write pointer
	dvc->buffers_s2mm.idx_current = (dvc->buffers_s2mm.idx_current + 1) % (dvc->buffers_s2mm.size);

	// check if we need to transfer more blocks, start a new transfer if yes.
	dvc->count_transfer_blocks_s2mm = dvc->count_transfer_blocks_s2mm - 1;
	if (dvc->count_transfer_blocks_s2mm > 0)  {
		iowrite32(dvc->buffers_s2mm.phy[dvc->buffers_s2mm.idx_current], dvc->base_addr + AXI_DMA_DEST_S2MM_LSB);
		iowrite32(dvc->size_buffer, dvc->base_addr + AXI_DMA_LEN_S2MM);
	}
	return IRQ_HANDLED;
}


static void free_device_info(DeviceInfo_t** const ptr_dvc, struct device* const dev, int mem_locked) {
	if (ptr_dvc == NULL || *ptr_dvc == NULL) {
		return;
	}
	DeviceInfo_t* dvc;
	dvc = *ptr_dvc;
	if (dvc->dev != NULL) {
		device_destroy(dvc->driver_class, dvc->devt);
		dvc->dev = NULL;
	}
	if (dvc->driver_class != NULL) {
		class_destroy(dvc->driver_class);
		dvc->driver_class = NULL;
	}
	if (dvc->irq_s2mm != 0) {
		free_irq(dvc->irq_s2mm, dvc);
	}
	if (dvc->irq_mm2s != 0) {
		free_irq(dvc->irq_mm2s, dvc);
	}
	if (dvc->base_addr != NULL) {
		iounmap(dvc->base_addr);
		dvc->base_addr = NULL;
	}
	if (mem_locked) {
		release_mem_region(dvc->mem_start, dvc->mem_end - dvc->mem_start + 1);
	}

	kfree(dvc);
	dev_set_drvdata(dev, NULL);
	*ptr_dvc = NULL;
}

static int dgd_axi_dma_probe(struct platform_device* pdev) {
	struct resource* r_irq_0 = 0;
	struct resource* r_irq_1 = 0;
	struct resource* r_mem = 0;
	struct device* dev = &pdev->dev;
	DeviceInfo_t* dvc = NULL;
	
	int rc = 0;
	dev_t devt;
	int retval = 0;
	int mem_locked = 0;
	char* name_device = NULL;
	char* name_irq_s2mm = NULL;
	char* name_irq_mm2s = NULL;

	do {
		int str_size = len_arr(DRIVER_NAME) + len_arr(TEMPLATE_NAME_DRIVER) - TEMPLATE_NAME_DRIVER_REPLACED + (1 + probe_counter / 10);
		name_device = kzalloc(str_size, GFP_KERNEL);
		if (name_device == NULL) {
			printk(TAG_LOG": failed to allocate memory for device name\n");
			rc = -ENOMEM;
			break;
		}

		str_size = len_arr(DRIVER_NAME) + len_arr(TEMPLATE_NAME_IRQ) - TEMPLATE_NAME_IRQ_REPLACED + (1 + probe_counter / 10) + 4/* for s2mm/mm2s*/;
		name_irq_s2mm = kzalloc(str_size, GFP_KERNEL);
		if (name_irq_s2mm == NULL) {
			printk(TAG_LOG": failed to allocate memory for s2mm interrupt\n");
			rc = -ENOMEM;
			break;
		}
		name_irq_mm2s = kzalloc(str_size , GFP_KERNEL);
		if (name_irq_mm2s == NULL) {
			printk(TAG_LOG": failed to allocate memory for mm2s interrupt\n");
			rc = -ENOMEM;
			break;
		}

		str_size = len_arr(DRIVER_NAME) + (1 + probe_counter / 10);
		rc = sprintf(name_device, TEMPLATE_NAME_DRIVER, DRIVER_NAME, probe_counter);
		if (rc != str_size) {
			printk(TAG_LOG": failed to setup device name: expected %d, written %d\n", str_size, rc);
			rc = -ENOMEM;
			break;
		}
		str_size = len_arr(DRIVER_NAME) + (1 + probe_counter / 10) + len_arr("s2mm");
		rc = sprintf(name_irq_s2mm, TEMPLATE_NAME_IRQ, DRIVER_NAME, probe_counter, "s2mm");
		if (rc != str_size) {
			printk(TAG_LOG": failed to setup device s2mm interrupt name: expected %d, written %d\n", str_size, rc);
			rc = -ENOMEM;
			break;
		}
		str_size = len_arr(DRIVER_NAME) + (1 + probe_counter / 10) + len_arr("mm2s");
		rc = sprintf(name_irq_mm2s, TEMPLATE_NAME_IRQ, DRIVER_NAME, probe_counter, "mm2s");
		if (rc != str_size) {
			printk(TAG_LOG": failed to setup device mm2s interrupt name: expected %d, written %d\n", str_size, rc);
			rc = -ENOMEM;
			break;
		}
		printk(TAG_LOG": driver name: %s probing\n", name_device);

		r_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (!r_mem) {
			printk(TAG_LOG": Failed to get memory resource\n");
			rc = -ENODEV;
			break;
		}

		dvc = (DeviceInfo_t*) kmalloc(sizeof(*dvc), GFP_KERNEL);
		if (!dvc) {
			printk(TAG_LOG": Failed to allocate memory for device info structure\n");
			rc = -ENOMEM;
			break;
		}
		dev_set_drvdata(dev, dvc);
		dvc->mem_start = r_mem->start;
		dvc->mem_end = r_mem->end;
		dvc->pl_dev = NULL;
		dvc->dev = NULL;
		dvc->irq_mm2s = 0;
		dvc->irq_s2mm = 0;

		if (!request_mem_region(dvc->mem_start, dvc->mem_end - dvc->mem_start + 1, name_device)) {
			printk(TAG_LOG": Failed to request memory region\n");
			rc = -EBUSY;
			break;
		}

		dvc->base_addr = ioremap(dvc->mem_start, dvc->mem_end - dvc->mem_start + 1);
		if (!dvc->base_addr) {
			printk(TAG_LOG": Failed to re-map memory region\n");
			rc = -EIO;
			break;
		}

		mem_locked = 1;
		r_irq_0 = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
		r_irq_1 = platform_get_resource(pdev, IORESOURCE_IRQ, 1);

		if (r_irq_1) {
			dvc->irq_s2mm = r_irq_1->start;
			rc = request_irq(dvc->irq_s2mm, &dgd_axi_dma_irq_s2mm, 0, name_irq_s2mm, dvc);
			if (rc) {
				printk(TAG_LOG": Failed to request interrupt %d\n", dvc->irq_s2mm);
				rc = -EBUSY;
				break;
			}
		}
		else {
			printk(TAG_LOG": irq1 not found. Interrupt not specified or there is only one DMA channel enabled\n");
		}

		if (!r_irq_0) {
			printk(TAG_LOG": no interrupt found. Device at 0x%08lx mapped to 0x%08lx\n", dvc->mem_start, dvc->base_addr);
			return 0;
		}

		if (dvc->irq_s2mm == r_irq_1->start) {
			dvc->irq_mm2s = r_irq_0->start;
			rc = request_irq(dvc->irq_mm2s, &dgd_axi_dma_irq_mm2s, 0, name_irq_mm2s, dvc);
		}
		else {
			// we always need s2mm interrupt
			dvc->irq_s2mm = r_irq_0->start;
			rc = request_irq(dvc->irq_s2mm, &dgd_axi_dma_irq_s2mm, 0, name_irq_s2mm, dvc);
		}

		if (rc) {
			printk(TAG_LOG": Failed to request interrupt %d\n", dvc->irq_mm2s);
			rc = -EBUSY;
			break;
		}

		printk(TAG_LOG": dgd-axi-dma at 0x%08lx mapped to 0x%08lx, irq_mm2s = %d, irq_s2mm = %d\n",
			dvc->mem_start, dvc->base_addr, dvc->irq_mm2s, dvc->irq_s2mm
		);

		rc = alloc_chrdev_region(&devt, 0, 1, name_device);
		if (rc < 0) {
			printk(TAG_LOG": failed to allocate chrdev_region\n");
			break;
		}
		dvc->devt = devt;
		cdev_init(&dvc->cdev, &driver_fops);
		dvc->cdev.owner = THIS_MODULE;
		retval = cdev_add(&dvc->cdev, devt, probe_counter + 1);
		if (retval) {
			printk(TAG_LOG": failed to add char device\n");
			break;
		}
		
		dvc->driver_class = class_create(THIS_MODULE, name_device);
		if (IS_ERR(dvc->driver_class)) {
			printk(TAG_LOG": failed to create device class\n");
			rc = PTR_ERR(dvc->driver_class);
			break;
		}
		
		dvc->dev = device_create(dvc->driver_class, dev, devt, NULL, "%s%d", name_device, 0);
		if (IS_ERR(dvc->dev)) {
			printk(TAG_LOG": failed to create device\n");
			rc = PTR_ERR(dvc->dev);
			break;
		}
		
		dvc->pl_dev = dev;
		dvc->dma_device_number = probe_counter;
		probe_counter++;

		dvc->size_buffer = 0;
		dvc->buffers_mm2s.idx_current = 0;
		dvc->buffers_mm2s.size = 0;
		dvc->buffers_mm2s.phy = NULL;
		dvc->buffers_mm2s.virt = NULL;

		dvc->buffers_s2mm.idx_current = 0;
		dvc->buffers_s2mm.size = 0;
		dvc->buffers_s2mm.phy = NULL;
		dvc->buffers_s2mm.virt = NULL;

		dvc->count_transfer_blocks_mm2s = 0;
		dvc->count_transfer_blocks_s2mm = 0;
		dvc->mmap_s2mm = 0;
		dvc->mmap_mm2s = 0;
		dvc->mmap_idx = 0;
		dvc->print_log = 1;

		if (name_device != NULL) {
			kfree(name_device);
			name_device = NULL;
		}
		if (name_irq_s2mm != NULL) {
			kfree(name_irq_s2mm);
			name_irq_s2mm = NULL;
		}
		if (name_irq_mm2s != NULL) {
			kfree(name_irq_mm2s);
			name_irq_mm2s = NULL;
		}
		return 0;
	} while(0);
	
	printk(TAG_LOG": dgd-axi-dma free resources after fail\n");
	
	free_device_info(&dvc, dev, mem_locked);
	/* no need to call (leads to crashes in other modules)
	if (r_irq_0 != NULL) {
		kfree(r_irq_0);
		r_irq_0 = NULL;
	}
	if (r_irq_1 != NULL) {
		kfree(r_irq_1);
		r_irq_1 = NULL;
	}
	if (r_mem != NULL) {
		kfree(r_mem);
		r_mem = NULL;
	}
	*/
	if (name_device != NULL) {
		kfree(name_device);
		name_device = NULL;
	}
	if (name_irq_s2mm != NULL) {
		kfree(name_irq_s2mm);
		name_irq_s2mm = NULL;
	}
	if (name_irq_mm2s != NULL) {
		kfree(name_irq_mm2s);
		name_irq_mm2s = NULL;
	}
	return rc;
}

static int dgd_axi_dma_remove(struct platform_device* pdev) {
	struct device* dev = &pdev->dev;
	DeviceInfo_t* lp = dev_get_drvdata(dev);
	free_device_info(&lp, dev, 1);
	return 0;
}


static struct of_device_id dgd_axi_dma_match[] = {
	{ .compatible = TAG_AXI_DMA_DTS, },
	{ /* end of list */ },
};
MODULE_DEVICE_TABLE(of, dgd_axi_dma_match);


static struct platform_driver dgd_axi_dma_driver = {
	.driver = {
		.name 			= DRIVER_NAME,
		.owner 			= THIS_MODULE,
		.of_match_table	= dgd_axi_dma_match,
	},
	.probe		= dgd_axi_dma_probe,
	.remove		= dgd_axi_dma_remove,
};

static int __init dgd_axi_dma_init(void) {
	return platform_driver_register(&dgd_axi_dma_driver);
}

static void __exit dgd_axi_dma_exit(void) {
	platform_driver_unregister(&dgd_axi_dma_driver);
}



module_init(dgd_axi_dma_init);
module_exit(dgd_axi_dma_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("DanGld dangladste@gmail.com");
MODULE_DESCRIPTION("dgd-axi-dma - platform driver for Xilinx AXI-DMA.");
MODULE_VERSION("0.01");
