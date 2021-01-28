#ifndef MSI_BUS_H
#define MSI_BUS_H

#include "Shared.h"
#include "Pipeline2.h"
#include "Cache.h"
#include "Memory.h"

typedef enum {Core0Id = 0, Core1Id, Core2Id, Core3Id, MEMId = NUM_CORES} BusOrigId;
typedef enum {NoCommand = 0, BusRd, BusRdx, Flush } BusCommand;
typedef enum {BusFail = -1, BusSuccess = 0, BusWait } BusStatus;
typedef enum {NotWatched = -1, Core0SC = 0, Core1SC, Core2SC, Core3SC, Watched} WatchFlag;

struct Pipeline;
struct MSIBus_
{
	struct Pipeline *pipes[NUM_CORES];
	Cache caches[NUM_CORES];
	Memory mem;
	BusOrigId busOrigid;
	BusCommand busCmd;
	int busAddr;
	int busData;
	bool busBusy;
	int busWaitCycles;
	FILE* BusTraceFile;
	char coreWatchFlags[NUM_CORES][MEM_SIZE];
};
typedef struct MSIBus_* MSIBus;
/*
struct BusTrace_
{
	int currCycle;
	BusOrigId busOrigid;
	BusCommand busCmd;
	
};
typedef struct BusTrace_* BusTrace;
*/
MSIBus createMSIBus();
void destroyMSIBus( MSIBus bus );
void initializeMSIBus(MSIBus bus, struct Pipeline* pipes[], Cache caches[], Memory mem);
void processorRead   ( MSIBus bus, BusOrigId coreId, int address );
void processorWrite  ( MSIBus bus, BusOrigId coreId, int address, int data );
void busRd ( MSIBus bus, BusOrigId coreId, int address );
void busRdX( MSIBus bus, BusOrigId coreId, int address );
void advanceMSIBusClock(MSIBus bus, MemStatus memStatus);
void setCoreWatchFlag  (MSIBus bus, BusOrigId coreId, unsigned int addr);
WatchFlag getCoreWatchResult(MSIBus bus, BusOrigId coreId, unsigned int addr);

void openFileForBusTrace();
void busTrace(MSIBus bus, BusOrigId coreId, int address);
void flush(MSIBus bus, BusOrigId coreId, int address, int data);
#endif
