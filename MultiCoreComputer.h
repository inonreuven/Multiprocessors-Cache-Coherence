#ifndef MULTI_CORE_COMPUTER_H
#define MULTI_CORE_COMPUTER_H

#include "Shared.h"
#include "Pipeline2.h"
#include "Cache.h"
#include "MSIBus.h"

struct MultiCoreComputer
{
	Pipeline* pipes[NUM_CORES];
	Cache caches[NUM_CORES];
	MSIBus bus;
	Memory mem;
};
typedef struct MultiCoreComputer* Computer;

Computer CreateNewComputer();
void initializeComputer(Computer comp, char* fileNames[]);
void destroyComputer(Computer comp);
void runComputer(Computer comp);

#endif