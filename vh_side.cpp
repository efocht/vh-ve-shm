//
// g++ -std=c++11 -o vh_side vh_side.cpp
//
// Compile ve and vh side programs.
//
// Run eg. with:
// ./vh_side 1234
// ./ve_side 1234 $((64*1024*1024))
//
// You must run the VH program first! It is the one which allocates the shm segment.

#include <iostream>
#include <string>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>


int main(int argc, char* argv[])
{
	int err = 0;
	struct shmid_ds ds;

	if (argc < 2) {
		std::cout << "Argument needed! Pass <key>." << std::endl;
		return 1;
	}

	int shm_key = atoi(argv[1]);
	size_t size = 64 * 1024 * 1024;

	std::cout << "VH: (shm_key = " << shm_key << ", size = " << size << ")" << std::endl;

	int segid = shmget(shm_key, size, IPC_CREAT | SHM_HUGETLB | S_IRWXU); 
	assert(segid != -1);

	err = shmctl(segid, IPC_STAT, &ds);
	std::cout << "VH: (ds.shm_perm.__key = " << ds.shm_perm.__key << ")" << std::endl;

	// attach shared memory to VH address space
	void* local_addr = nullptr;

	local_addr = shmat(segid, NULL, 0);
	std::cout << "VH: local_addr = " << std::hex << local_addr << std::endl;
	assert(local_addr != (void *) -1);
	sprintf((char *)local_addr, "%s", "TEST from the VH");

	std::cout << std::dec;
	std::cout << "Now start the ve_side program pasting the line below:" << std::endl
		  << "./ve_side " << shm_key << " " << size << std::endl;
 
	// wait until 2 attaches on shm segment, that means: VE also attached to it
	do {
		shmctl(segid, IPC_STAT, &ds);
		if (ds.shm_nattch > 1)
			break;
	} while (1);
	// give the VE 1 second time for its first change
	sleep(1);
	// print string from shm segment
	std::cout << "SHM on VH: " << (char *)local_addr << std::endl;

	// wait until only 1 attaches on shm segment, i.e. the VE side has finished
	do {
		shmctl(segid, IPC_STAT, &ds);
		if (ds.shm_nattch == 1)
			break;
	} while (1);

	// read a msg from the DMA buffer and output it
	std::cout << "SHM on VH: " << (char *)local_addr << std::endl;

	err = shmdt(local_addr);
	assert(err == 0);
	err = shmctl(segid, IPC_RMID, NULL);
	if (err < 0)
		std::cout << "Failed to remove SHM segment ID " << segid << std::endl;
	return 0;
}

