#ifndef MEMORY_H_
#define MEMORY_H_

#include "Shared.h"

#define MEM_SIZE (1<<20)  /* 2^20 words */
#define MEM_LATENCY 64

typedef enum {MemRead, MemWrite} MemOperation;
typedef enum {NoMemOperation=-1, MemReadFinished, MemWriteFinished } MemStatus;

struct Memory_
{
	int data[MEM_SIZE];
	bool memBusy;
	MemOperation curOp;
	unsigned int curAddress;
	int curData;
	int memWaitCycles;
};

typedef struct Memory_* Memory;

Memory createNewMemory();
void destroyMemory(Memory mem);
int readMemory(Memory mem, int address);
int writeMemory(Memory mem, int address, int data);
MemStatus advanceMemoryClock(Memory mem);
void freeMemory(Memory mem);

#endif