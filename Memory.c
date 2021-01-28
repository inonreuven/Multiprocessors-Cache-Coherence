#include "Shared.h"
#include "Memory.h"

Memory createNewMemory()
{
	Memory mem;

	/* Create data memory */
	mem = (Memory)malloc( sizeof(struct Memory_) );
	if (mem == NULL)
	{
		fprintf(stderr, "Could not allocate data memory.\n");
		return NULL;
	}

	mem->memBusy = False;
	mem->memWaitCycles = 0;

	return mem;
}
void freeMemory(Memory mem) 
{
	mem->memBusy = False;
	mem->memWaitCycles = 0;
	mem->curOp = NoMemOperation;
}
void destroyMemory( Memory mem)
{
	free(mem);
}

/* Start read operation on the memory */
int readMemory(Memory mem, int address)
{
	if (mem->memBusy == True)
		return 0;

	mem->curOp = MemRead;
	mem->curAddress = address;
	mem->memBusy = True;
	mem->memWaitCycles = MEM_LATENCY;

	return 1;
}

/* Start read operation on the memory */
int writeMemory(Memory mem, int address, int data)
{
	if (mem->memBusy == True)
		return 0;

	mem->curOp = MemWrite;
	mem->curAddress = (unsigned int)address;
	mem->curData = data;
	mem->memBusy = True;
	mem->memWaitCycles = MEM_LATENCY;

	return 1;
}

MemStatus advanceMemoryClock(Memory mem)
{
	if (mem->memBusy == False || mem->memWaitCycles == 0)
		return NoMemOperation;

	mem->memWaitCycles--;
	if (mem->memWaitCycles == 0)
	{
		switch (mem->curOp)
		{
			case MemWrite:
				mem->data[mem->curAddress] = mem->curData;
				return MemWriteFinished;
			case MemRead:
				mem->curData = mem->data[mem->curAddress];
				return MemReadFinished;
		}
	}

	return NoMemOperation;
}
