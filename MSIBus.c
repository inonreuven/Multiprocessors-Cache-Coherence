#include "Shared.h"
#include "MSIBus.h"

MSIBus createMSIBus()
{
	MSIBus bus = (MSIBus)malloc(sizeof(struct MSIBus_));
	return bus;
}

void destroyMSIBus(MSIBus bus)
{
	free(bus);
}

void initializeMSIBus(MSIBus bus, struct Pipeline* pipes[], Cache caches[], Memory mem)
{
	int i,j;

	for (i = 0; i < NUM_CORES; i++)
	{
		bus->pipes[i] = pipes[i];
		bus->caches[i] = caches[i];
	}
	bus->mem = mem;
	bus->busCmd = NoCommand;
	bus->busBusy = False;
	bus->busWaitCycles = 0;
	bus->BusTraceFile = openFileForBusTrace();
	/* Initialize core watch flags */
	for (i = 0; i < NUM_CORES; i++)
		for (j = 0; j < MEM_SIZE; j++)
			bus->coreWatchFlags[i][j] = NotWatched; // Not watched
}
FILE* openFileForBusTrace()
{
	FILE *fptr = fopen("bustrace.txt", "w");
	if (fptr == NULL)
	{
		printf("Error!");
		exit(1);
	}
	return fptr;
}

//this function been called every memory read/write transiction
void busTrace(MSIBus bus, BusOrigId coreId, int address)
{
	fprintf(bus->BusTraceFile, "%d %d %d %d %d\n", 
			bus->pipes[coreId]->totalCycles, bus->busOrigid, bus->busCmd, bus->busAddr, bus->busData);
}

void processorRead(MSIBus bus, BusOrigId coreId, int address)
{
	char Bits[NUM_MSI_BITS];
	
	if (coreId < 0 || coreId >= NUM_CORES)
	{
		fprintf(stderr, "Error in function processorRead: core id is out of range");
		return;
	}

	getMSIBits(bus->caches[coreId], address, Bits);
	// the block is invalid, move to share
	// invalid: normal busRd transction since the block is not in the cache
	if (Bits[I_BIT] == 1)
	{
		Bits[I_BIT] = 0;
		Bits[S_BIT] = 1;
		Bits[M_BIT] = 0;
		setMSIBits(bus->caches[coreId], address, Bits); // MSI bits update is done before the end of busRdX
		busRd(bus, coreId, address);
	}

}

void processorWrite(MSIBus bus, BusOrigId coreId, int address, int data )
{
	char Bits[NUM_MSI_BITS];

	if (coreId < 0 || coreId >= NUM_CORES)
	{
		fprintf(stderr, "Error in function processorRead: core id is out of range");
		return;
	}

	getMSIBits(bus->caches[coreId], address, Bits);
	
	if (Bits[I_BIT] == 1 || Bits[S_BIT] == 1)
	{
		Bits[I_BIT] = 0;
		Bits[S_BIT] = 1;
		Bits[M_BIT] = 0;
		setMSIBits(bus->caches[coreId], address, Bits); // MSI bits update is done before the end of busRdX
		busRdX(bus, coreId, address);
	}
}

void busRd(MSIBus bus, BusOrigId coreId, int address )
{
	int i;
	int data, status;
	char foundInAnyCache = 0;	

	for (i = 0; i < NUM_CORES; i++)
	{
		status = readFromCache( bus->caches[i], address, &data );
		if (status == 1) /* Hit in another cache */
		{
			bus->busData = data;
			foundInAnyCache = 1;

			char Bits[NUM_MSI_BITS];
			getMSIBits(bus->caches[i], address, Bits);
			if (Bits[M_BIT] == 1)
			{
				Bits[I_BIT] = 0;
				Bits[S_BIT] = 1;
				Bits[M_BIT] = 0;
				setMSIBits(bus->caches[i], address, Bits); 
				//flush(MSIBus bus, i, address, data)
			}

		}
	}
	if (foundInAnyCache == 0)
		bus->busData = readMemory(bus->mem, address);
}

//write data to memory
void flush(MSIBus bus, BusOrigId coreId, int address, int data)
{
	int writeStatus = writeMemory(bus->mem, address, data);
	if (writeStatus == 1)
	{
		writeStatus = MemStatus advanceMemoryClock(bus->mem);
		if (writeStatus == 1)
			freeMemory(bus->mem);
	}
}

void busRdX( MSIBus bus, BusOrigId coreId, int address )
{
	int i;
	int data, status;
	char foundInAnyCache = 0;

	for (i = 0; i < NUM_CORES; i++)
	{
		status = readFromCache(bus->caches[i], address, &data);
		if (status == 1) /* Hit in another cache */
		{
			bus->busData = data;
			foundInAnyCache = 1;
		}
	}
	if (foundInAnyCache == 0)
		bus->busData = readMemory(bus->mem, address);
}

void advanceMSIBusClock(MSIBus bus, MemStatus memStatus)
{
	if ( bus->busBusy && bus->busWaitCycles > 0 )
		bus->busWaitCycles--;
	// Unfreeze pipelines
}

void setCoreWatchFlag(MSIBus bus, BusOrigId coreId, unsigned int addr)
{
	bus->coreWatchFlags[coreId][addr] = Watched;
}

bool getCoreWatchResult(MSIBus bus, BusOrigId coreId, unsigned int addr)
{
	int i;

	if (bus->coreWatchFlags[coreId][addr] == (char)NotWatched || bus->coreWatchFlags[coreId][addr] == (char)Watched ||
		bus->coreWatchFlags[coreId][addr] == (char)coreId )
	{
		for (i = 0; i < NUM_CORES; i++)
			if ( bus->coreWatchFlags[coreId][addr] == Watched )
				bus->coreWatchFlags[coreId][addr] = (char)coreId; // Mark as successfull SC operation

		return True; // allow sc command
	}
	
	return False;
}