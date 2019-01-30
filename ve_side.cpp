//
// /opt/nec/ve/bin/nc++ -std=gnu++11 -o ve_side ve_side.cpp
// 

#include <iostream>
#include <string>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <sys/mman.h>

extern "C" {
#include <vhshm.h>
#include <vedma.h>
}

#include "ve_inst.h"


int main(int argc, char* argv[])
{
	struct shmid_ds ds;
	int segid = -1;
	int err = 0;

        if (argc < 3) {
		std::cout << "Arguments needed! Pass <key> and <size>." << std::endl;
		return 1;
	}

	int shm_key = atoi(argv[1]);
	size_t size = (size_t)atoi(argv[2]);


	std::cout << "VE: (shm_key = " << shm_key << ", size = " << size << ")" << std::endl;

	//
	// determine shm segment ID from its key
	//
	segid = vh_shmget(shm_key, size, SHM_HUGETLB);
	if (segid == -1) {
		std::cout << "VE: vh_shmget failed, reason: " << strerror(errno) << std::endl;
		return 2;
	}

	std::cout << "VE: segid = " << segid << std::endl;

	uint64_t remote_vehva = 0;
	void *remote_addr = nullptr;

	//
	// attach shared memory VH address space and register it to DMAATB,
	// the region is accessible for DMA unter its VEHVA remote_vehva
	//
        remote_addr = vh_shmat(segid, NULL, 0, (void **)&remote_vehva);

	if (remote_addr == nullptr) {
		std::cout << "VE: (remote_addr == nullptr) " << std::endl;
		return 3;
	}
	if (remote_vehva == (uint64_t)-1) {
		std::cout << "VE: (remote_vehva == -1) " << std::endl;
		return 4;
	}

	std::cout << "VE: remote_addr = " << std::hex << remote_addr << std::endl;
	std::cout << "VE: remote_vehva = " << std::hex << remote_vehva << std::endl;

	//
	// a little buffer for word-wise transfer, aligned to 8-byte boundary
	//
	uint64_t buff[32];

	//
	// read some data 8-byte-wise from the VH shm segment with "lhm"
	//
	for (auto i = 0; i < 32; i++)
		buff[i] = ve_inst_lhm((void *)((uint64_t)remote_vehva + i*sizeof(uint64_t)));

	std::cout << "VE read from VH sysV shm segment ('lhm'): " << (char *)&buff[0] << std::endl;
 
	sprintf((char *)buff, "the quick brown fox jumps over the lazy dog");

	//
	// write some data 8-byte-wise to the VH shm segment with "shm"
	//
	for (auto i = 0; i < 32; i++)
		ve_inst_shm((void *)((uint64_t)remote_vehva + i*sizeof(uint64_t)), buff[i]);

	std::cout << "VE wrote to VE sysV shm segment ('shm'): " << (char *)&buff[0] << std::endl;

 	std::cout << "VE: intializing DMA" << std::endl;


	//
	// Initialize DMA
	//
	err = ve_dma_init();
	if (err)
		std::cout << "Failed to initialize DMA" << std::endl;

	//
	// Allocate local VE buffer of same size as sysV shm segment on VH
	//
	std::cout << "VE: allocating local VE buffer" << std::endl;

	void *local_buff = nullptr;
	uint64_t local_vehva = 0;
	local_buff = mmap(NULL, size, PROT_READ | PROT_WRITE,
			  MAP_PRIVATE | MAP_ANONYMOUS | MAP_64MB, -1, 0);
	if (local_buff == nullptr) {
		std::cout << "VE: (local_buff == nullptr) " << std::endl;
		return 5;
	}
	//
	// register local buffer for DMA
	//
	local_vehva = ve_register_mem_to_dmaatb(local_buff, size);
	if (local_vehva == (uint64_t)-1) {
		std::cout << "VE: local_vehva == -1 " << std::endl;
		return 6;
	}

	sleep(2);
        //
	// sync (copy over) VH shm segment to VE local buffer by DMA
	//
	std::cout << "VE: syncing VH shm segment to local_buff" << std::endl;
	err = ve_dma_post_wait(local_vehva, remote_vehva, size);
	if (err)
		std::cout << "ve_dma_post_wait has failed!"
			  << " err = " << err << std::endl;
	std::cout << "VE: local_buff: " << (char *)local_buff << std::endl;

	for (int i = 0; i < size / sizeof(uint64_t); i++)
		((uint64_t *)local_buff)[i] = 0x0000616761676167;

	std::cout << "VE: local_buff: " << (char *)local_buff << std::endl;

        //
	// sync (copy over) VE local buffer to VH shm segment by DMA
	//
	std::cout << "VE: syncing local_buff to VH shm segment" << std::endl;
	err = ve_dma_post_wait(remote_vehva, local_vehva, size);
	if (err)
		std::cout << "ve_dma_post_wait has failed!"
			  << " err = " << err << std::endl;
	sleep(1);
	//
	// unregister local buffer from DMAATB
	//
	err = ve_unregister_mem_from_dmaatb(local_vehva);
	if (err)
		std::cout << "Failed to unregister local buffer from DMAATB" << std::endl;

	//
	// detach VH sysV shm segment
	//
	err = vh_shmdt(remote_addr);
	if (err)
		std::cout << "Failed to detach from VH sysV shm" << std::endl;

	return 0;
}

