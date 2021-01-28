#include "MultiCoreComputer.h"

int main()
{
	char* fileNames[4] = { "prog1.asm", "prog2.asm", "prog3.asm", "prog4.asm" };

	Computer comp = CreateNewComputer();
	initializeComputer(comp, fileNames);
	runComputer(comp);
	destroyComputer(comp);
	
	return 0;
}