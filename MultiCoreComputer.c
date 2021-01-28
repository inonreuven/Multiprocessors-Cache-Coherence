#include "MultiCoreComputer.h"

Computer CreateNewComputer()
{
	Computer comp = (Computer)malloc( sizeof(struct MultiCoreComputer) );

	if (comp == NULL)
	{
		fprintf(stderr, "Could not allocate memory for computer.\n");
		return NULL;
	}

	return comp;
}

void initializeComputer(Computer comp, char* fileNames[] )
{
	int i;

	/* Create and initialize caches */
	for (i = 0; i < NUM_CORES; i++)
	{
		/* Create and initialize cache i */
		comp->caches[i] = getNewCache(i);
	}

	/* Create data memory */
	comp->mem = createNewMemory();

	/* Initialize MSI bus */
	comp->bus = createMSIBus();
	initializeMSIBus(comp->bus, comp->pipes, comp->caches, comp->mem);

	/* Create and initialize pipelines */
	for (i = 0; i < NUM_CORES; i++)
	{		
		/* Create and initialize pipeline i */
		comp->pipes[i] = createPipeline();
		initializePipeline(comp->pipes[i], fileNames[i], comp->bus, comp->caches[i] );
	}	
}

void destroyComputer( Computer comp )
{
	int i;

	for (i = 0; i < NUM_CORES; i++)
	{
		/* Create cache i */
		destroyCache(comp->caches[i]);

		/* Initialize pipeline i */
		destroyPipeline(comp->pipes[i]);

	}
	/* Destroy data memory */
	destroyMemory(comp->mem);

	/* Destroy MSI bus */
	destroyMSIBus(comp->bus);

	/* Destroy computer */
	free(comp);
}

void runComputer( Computer comp)
{
	int i;
	MemStatus memStatus;
	bool done = False;
	int clockCnt = 0;

	while (!done)
	{
		done = True;
		for (i = 0; i < NUM_CORES; i++)
			if (!comp->pipes[i]->totally_done)
			{
				done = False;
				runPipelineOneCycle(comp->pipes[i]);
			}
		memStatus = advanceMemoryClock(comp->mem);
		advanceMSIBusClock(comp->bus, memStatus);
		clockCnt++;
	}
}