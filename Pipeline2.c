#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "Pipeline2.h"

Instruction bubble = { S, STALL, 0, 0, 0, False };

Pipeline* createPipeline()
{
	Pipeline *pipe = (Pipeline *) malloc(sizeof(Pipeline));
	return pipe;
}

void destroyPipeline(Pipeline* pipe)
{
	free(pipe);
}

void initializePipeline( Pipeline *pipe, char* fileName, struct MSIBus_ *bus, Cache cache )
{
	int i;

	for (i = 0; i < NumStages; i++)
	{				
		pipe->stageStat[i].stalled = False;
		pipe->stageStat[i].stallNextCycle = False;
		pipe->stageStat[i].delayedWrite = False;
		pipe->stageInst[i].inst = bubble;
	}

	pipe->dataHazardStallCycles = 0;
	pipe->stalledDataHazard = False;
	pipe->flushBranchFlag = False;
	pipe->branchTaken = False;
	pipe->totally_done = False;      // True when halt has propagated through the pipeline
	pipe->interactive_mode = False;  // If True, print registers after each Instruction, wait for enter to be pressed


	pipe->registers[0] = 0;
	pipe->PC = 0;
	pipe->haltIndex = 0;
	pipe->totalCycles = 0;
	pipe->ifUtil = 0;
	pipe->idUtil = 0;
	pipe->exUtil = 0;
	pipe->memUtil = 0;
	pipe->wbUtil = 0;

	pipe->bus = bus;
	pipe->cache = cache;

	progScanner(pipe, fileName);
}

void runPipelineOneCycle(Pipeline* pipe)
{
	char blank[2];

	WB(pipe);
	if (pipe->totally_done) // Case of HALT processed
		return;

	MEM(pipe);
	EX(pipe);
	ID(pipe);
	IF(pipe);
	pipe->totalCycles++;
	if (pipe->interactive_mode)
	{
		printRegisters(pipe);
		printStatistics(pipe);
		printf("Press ENTER to continue.\n");
		fgets(blank, sizeof(blank), stdin);
	}
}

void runPipelineFully( Pipeline* pipe )
{	
	// Then start iterating over the pipelined stages in reverse
	while (!pipe->totally_done)
		runPipelineOneCycle(pipe);

	printStatistics(pipe);
	printRegisters (pipe);
}

void progScanner(Pipeline* pipe, char* filename)
{
	char instruction_string[MAX_INSTR_LEN];
	FILE* instFile = fopen(filename, "r");
	
	if (instFile == NULL) 
		assert(!"Invalid file");

	while ( fgets(instruction_string, MAX_INSTR_LEN, instFile) )
		parser(pipe, instruction_string);

	fclose(instFile);
}

void parser(Pipeline* pipe, char* inst)
{
	int rs, rd, rt, imm;
	int PC = pipe->PC;
	opcode op;
	char* opString;

	trimInstruction(inst);
	// To hold haltSimultion, needs 15 chars
	opString = extractOpcode(inst);
	op = stringToOpcode(opString);

	if ( isRegularType(op) ) 
	{
		rs = extractRegister(inst, 1);
		rt = extractRegister(inst, 2);
		rd = extractRegister(inst, 0);
		pipe->instruction_mem[PC].type = Reg;
		pipe->instruction_mem[PC].op = op;
		pipe->instruction_mem[PC].rs = rs;
		pipe->instruction_mem[PC].rt = rt;
		pipe->instruction_mem[PC].rd = rd;
		if (rt == -1)
		{			
			imm = extractImmediate(inst, 2);
			pipe->instruction_mem[PC].imm = imm;
			pipe->instruction_mem[PC].hasImm = True;
		}
		else
		{
			pipe->instruction_mem[PC].imm = -1;
			pipe->instruction_mem[PC].hasImm = False;
		}		
		pipe->instruction_mem[PC].isHalt = False;
	}	
	else if ( isJal(op) )
	{		
		imm = extractImmediate(inst, 0);
		pipe->instruction_mem[PC].type = J;
		pipe->instruction_mem[PC].op = op;
		pipe->instruction_mem[PC].rs = -1;
		pipe->instruction_mem[PC].rt = -1;
		pipe->instruction_mem[PC].rd = 1;  // Register 1
		pipe->instruction_mem[PC].imm = imm;
		pipe->instruction_mem[PC].hasImm = True;
		pipe->instruction_mem[PC].isHalt = False;
	}	
	else if ( isBranch(op) )
	{
		rs = extractRegister(inst, 1);
		rt = extractRegister(inst, 2);
		imm = extractImmediate(inst, 3);
		pipe->instruction_mem[PC].type = B;
		pipe->instruction_mem[PC].op = op;
		pipe->instruction_mem[PC].rs = rs;
		pipe->instruction_mem[PC].rt = rt;
		pipe->instruction_mem[PC].rd = 1; // Register 1
		pipe->instruction_mem[PC].imm = imm;
		pipe->instruction_mem[PC].hasImm = True;
		pipe->instruction_mem[PC].isHalt = False;
	}
	else if ( isStore(op) )
	{
		rs = extractRegister(inst, 1);
		rt = extractRegister(inst, 2);
		rd = extractRegister(inst, 0);
		pipe->instruction_mem[PC].type = STR;
		pipe->instruction_mem[PC].op = op;
		pipe->instruction_mem[PC].rs = rs;
		pipe->instruction_mem[PC].rt = rt;
		pipe->instruction_mem[PC].rd = rd;
		pipe->instruction_mem[PC].hasImm = False;
		pipe->instruction_mem[PC].isHalt = False;
	}
	else if ( isHalt(op) ) 
	{
		// Run the halt Instruction through the pipeline and then stop
		pipe->instruction_mem[PC].type = H;
		pipe->instruction_mem[PC].op = HALT;
		pipe->instruction_mem[PC].rs = -1;
		pipe->instruction_mem[PC].rt = -1;
		pipe->instruction_mem[PC].rd = -1;		
		pipe->instruction_mem[PC].isHalt = True;
		pipe->instruction_mem[PC].hasImm = False;
		pipe->haltIndex = PC;
	}
	else
		assert(!"Illegal opcode");

	pipe->PC++;
}

void IF( Pipeline* pipe )
{		
	if ( pipe->stageStat[IFStage].stalled == False && pipe->stalledDataHazard == False )
	{		
		if ( pipe->branchTaken ) // Flush when branch taken
		{
			if (pipe->flushBranchFlag)
			{
				pipe->stageInst[IFStage].inst = bubble;
				pipe->branchTaken = False;
			}
		}
		else
		{
			pipe->stageInst[IFStage].inst = pipe->instruction_mem[pipe->PC];
			if (pipe->PC < pipe->haltIndex)
				pipe->PC++;
		}	
		
		if (pipe->stageInst[IFStage].inst.type != S)
			pipe->ifUtil++;

		// Pass the Instruction to the next Stage
		pipe->stageInst[IDStage].inst = pipe->stageInst[IFStage].inst;
	}

	if (pipe->stageStat[IFStage].stallNextCycle)
	{
		pipe->stageStat[IFStage].stalled = True;  // Stall next cycle
		pipe->stageStat[IFStage].stallNextCycle = False;
	}
	
}

void ID( Pipeline* pipe )
{	
	int rs, rt, rd;	
	pipe->branchTaken = False;
	Stage dataHazardStage;

	if (pipe->stalledDataHazard && pipe->dataHazardStallCycles == 0) // finished waiting for data hazard resolution
		pipe->stalledDataHazard = False;

	if (pipe->stageStat[IDStage].stalled == False && pipe->stalledDataHazard == False )
	{		
		if (pipe->stageInst[IDStage].inst.type != S)
			pipe->idUtil++;

		//If there's no hazard
		if ( checkHazard(pipe) < 0 ) 
		{
			rs = pipe->stageInst[IDStage].inst.rs;
			rt = pipe->stageInst[IDStage].inst.rt;
			rd = pipe->stageInst[IDStage].inst.rd;

			if ( rs > NUM_REGS - 1 || rt > NUM_REGS - 1 || rd > NUM_REGS - 1 ||
				 rs < 0 || rt < 0 || rd < 0 )
				assert(!"Invalid register location");

			if ( rd == 1 && pipe->stageInst[IDStage].inst.type != B && pipe->stageInst[IDStage].inst.type != J)
				assert(!"Destination register cannot be R1");

			// Place immediate in rdData
			if ( pipe->stageInst[IDStage].inst.type == B || pipe->stageInst[IDStage].inst.type == J )
				pipe->stageInst[IDStage].inst.rdData = pipe->stageInst[IDStage].inst.imm;

			// If it's a branch, check if it is taken
			if (pipe->stageInst[IDStage].inst.op == BEQ)
			{
				if (pipe->registers[rs] == pipe->registers[rt]) // taken
				{
					pipe->PC = pipe->stageInst[IDStage].inst.rdData & 0x3FF;
					pipe->branchTaken = True;
				}
			}
			else if (pipe->stageInst[IDStage].inst.op == BNE)
			{
				if (pipe->registers[rs] != pipe->registers[rt]) // taken
				{
					pipe->PC = pipe->stageInst[IDStage].inst.rdData & 0x3FF;
					pipe->branchTaken = True;
				}
			}
			else if (pipe->stageInst[IDStage].inst.op == BLT)
			{
				if (pipe->registers[rs] < pipe->registers[rt]) // taken
				{
					pipe->PC = pipe->stageInst[IDStage].inst.rdData & 0x3FF;
					pipe->branchTaken = True;
				}
			}
			else if (pipe->stageInst[IDStage].inst.op == BGT)
			{
				if (pipe->registers[rs] > pipe->registers[rt]) // taken
				{
					pipe->PC = pipe->stageInst[IDStage].inst.rdData & 0x3FF;
					pipe->branchTaken = True;
				}
			}
			else if (pipe->stageInst[IDStage].inst.op == BLE)
			{
				if (pipe->registers[rs] <= pipe->registers[rt]) // taken
				{
					pipe->PC = pipe->stageInst[IDStage].inst.rdData & 0x3FF;
					pipe->branchTaken = True;
				}
			}
			else if (pipe->stageInst[IDStage].inst.op == BGE)
			{
				if (pipe->registers[rs] >= pipe->registers[rt]) // taken
				{
					pipe->PC = pipe->stageInst[IDStage].inst.rdData & 0x3FF;
					pipe->branchTaken = True;
				}
			}
			else if (pipe->stageInst[IDStage].inst.op == JAL)
			{
				pipe->stageInst[IDStage].data = pipe->PC + 1;
				pipe->PC = pipe->stageInst[IDStage].inst.rdData & 0x3FF;				
			}

			// Read the registers
			if ( rs != -1 )
				pipe->stageInst[IDStage].inst.rsData = pipe->registers[rs];
			if ( rt != -1 )
				pipe->stageInst[IDStage].inst.rtData = pipe->registers[rt];
			if ( rd != -1 )
				pipe->stageInst[IDStage].inst.rdData = pipe->registers[rd];

			// Pass the Instruction to the next Stage
			pipe->stageInst[EXStage].inst = pipe->stageInst[IDStage].inst;
				
		}
		else // Insert bubble to EX Stage and stall IF and ID
		{						
			//pipe->stageStat[IFStage].stalled = True;
			//pipe->stageStat[IDStage].stalled = True;
			pipe->stageInst[EXStage].inst = bubble;
			pipe->dataHazardStallCycles--;
		}
	}
	
	
	if (pipe->stalledDataHazard == True)
	{
		dataHazardStage = NumStages - pipe->dataHazardStallCycles;
		if ( pipe->stageStat[ dataHazardStage ].stalled == False ) // Check that the instruction resolving the data hazard is not stalled
			pipe->dataHazardStallCycles--; // counter for data hazard resolution
	}

	if (pipe->stageStat[IDStage].stallNextCycle)
	{
		pipe->stageStat[IDStage].stalled = True;  // Stall next cycle
		pipe->stageStat[IDStage].stallNextCycle = False;
	}
}

void EX( Pipeline* pipe )
{
	int rsData, rtData, rdData;
	int addr;

	if (pipe->stageStat[EXStage].stalled == False)
	{		
		rsData = pipe->stageInst[EXStage].inst.rsData;
		rtData = pipe->stageInst[EXStage].inst.rtData;	
		rdData = pipe->stageInst[EXStage].inst.rdData;

		if (pipe->stageInst[EXStage].inst.type != S)
			pipe->exUtil++;
		
		if (pipe->stageInst[EXStage].inst.op == ADD)
			pipe->stageInst[MEMStage].data = rsData + rtData;
				
		else if (pipe->stageInst[EXStage].inst.op == SUB)
			pipe->stageInst[MEMStage].data = rsData - rtData;
		
		else if (pipe->stageInst[EXStage].inst.op == AND)
			pipe->stageInst[MEMStage].data = rsData & rtData;

		else if (pipe->stageInst[EXStage].inst.op == OR)
			pipe->stageInst[MEMStage].data = rsData | rtData;

		else if (pipe->stageInst[EXStage].inst.op == XOR)
			pipe->stageInst[MEMStage].data = rsData ^ rtData;

		else if (pipe->stageInst[EXStage].inst.op == MUL)
			pipe->stageInst[MEMStage].data = rsData * rtData;
							
		else if (pipe->stageInst[EXStage].inst.op == SLL)
		{
			assert(rtData >= 0);
			pipe->stageInst[MEMStage].data = rsData << rtData;
		}
		else if (pipe->stageInst[EXStage].inst.op == SRL)
		{
			assert(rtData >= 0);
			pipe->stageInst[MEMStage].data = (int)((unsigned int)rsData >> rtData);
		}
		else if (pipe->stageInst[EXStage].inst.op == SRA)
		{
			assert(rtData >= 0);
			pipe->stageInst[MEMStage].data = rsData >> rtData;
		}
		else if (pipe->stageInst[EXStage].inst.op == LW)						
			pipe->stageInst[MEMStage].addr = rsData + rtData;

		else if (pipe->stageInst[EXStage].inst.op == SW)
		{
			pipe->stageInst[MEMStage].addr = rsData + rtData;
			pipe->stageInst[MEMStage].data = rdData;
		}
		else if (pipe->stageInst[EXStage].inst.op == LL)
		{
			pipe->stageInst[MEMStage].addr = rsData + rtData;
			setCoreWatchFlag( pipe->bus, pipe->cache->id, pipe->stageInst[MEMStage].addr );
		}
		else if (pipe->stageInst[EXStage].inst.op == SC)
		{
			addr = rsData + rtData;
			if ( getCoreWatchResult(pipe->bus, pipe->cache->id, addr) == True ) // Core watch flag is OK
			{
				pipe->stageInst[MEMStage].addr = rsData + rtData;
				pipe->stageInst[MEMStage].data = rdData;
				pipe->stageInst[MEMStage].inst.rdData = 1; // the data to be written in WB to rd register
			}
			else // Do not write memory in MEM
			{
				pipe->stageInst[MEMStage].addr = -1;
				pipe->stageInst[MEMStage].data = -1;
				pipe->stageInst[MEMStage].inst.rdData = 0; // the data to be written in WB to rd register
			}
		}
		else
			assert(!"Unrecognized Instruction");
		
				
		// Pass the Instruction to the next Stage
		pipe->stageInst[MEMStage].inst = pipe->stageInst[EXStage].inst;
	}

	if (pipe->stageStat[EXStage].stallNextCycle)
	{
		pipe->stageStat[EXStage].stalled = True;  // Stall next cycle
		pipe->stageStat[EXStage].stallNextCycle = False;
	}
}

void MEM( Pipeline* pipe )
{
	int memData;

	if (pipe->stageStat[MEMStage].stalled == False)
	{	
		if (pipe->stageInst[MEMStage].inst.type != S)
			pipe->memUtil++;

		// Check if the Instruction is LW
		bool isLoadFlag  = isLoad ( pipe->stageInst[MEMStage].inst.op );
		// Check if the Instruction is SW or SC that writes the data cache/memory
		bool isStoreFlag = isStore( pipe->stageInst[MEMStage].inst.op ) && !( pipe->stageInst[MEMStage].inst.op == SC && pipe->stageInst[MEMStage].data == -1 );
				
		if (isLoadFlag)
		{
			if (readFromCache(pipe->cache, pipe->stageInst[MEMStage].addr, &memData) == 1) // Hit on read in cache
			{
				pipe->stageInst[MEMStage].data = memData;
				// Pass the Instruction to the next Stage
				pipe->stageInst[WBStage].inst = pipe->stageInst[MEMStage].inst;
			}
			else // Miss on the data in cache
			{
				freezePipeline(pipe, MEMStage);
				processorRead(pipe->bus, pipe->cache->id, pipe->stageInst[MEMStage].addr);
			}
		}
		else if (isStoreFlag)
		{
			if (writeToCache(pipe->cache, pipe->stageInst[MEMStage].addr, pipe->stageInst[MEMStage].data) == 1)
			{
				// Pass the Instruction to the next Stage
				pipe->stageInst[WBStage].inst = pipe->stageInst[MEMStage].inst;
			}
			// the block is invalid or shared
			else
			{
				freezePipeline(pipe, MEMStage);
				processorWrite(pipe->bus, pipe->cache->id, pipe->stageInst[MEMStage].addr, pipe->stageInst[MEMStage].data);
			}			
		}
		else // Not load or store
			// Pass the Instruction to the next Stage
			pipe->stageInst[WBStage].inst = pipe->stageInst[MEMStage].inst;
				
	}

	if (pipe->stageStat[MEMStage].stallNextCycle)
	{
		pipe->stageStat[MEMStage].stalled = True;  // Stall next cycle
		pipe->stageStat[MEMStage].stallNextCycle = False;
	}
}

void WB( Pipeline* pipe )
{
	// Do the delayed write from previous cycle if any pending
	if (pipe->stageStat[WBStage].delayedWrite)
	{
		pipe->registers[ pipe->stageStat[WBStage].delayedWriteReg ] = pipe->stageStat[WBStage].delayedWriteData;
		pipe->stageStat[WBStage].delayedWrite = False;
	}

	if (pipe->stageStat[WBStage].stalled == False)
	{
		if (pipe->stageInst[WBStage].inst.type != STR && pipe->stageInst[WBStage].inst.type != B && pipe->stageInst[WBStage].inst.type != H &&
			pipe->stageInst[WBStage].inst.rd != 0 && pipe->stageInst[WBStage].inst.rd != 1)
		{
			// Check if JAL
			if (pipe->stageInst[WBStage].inst.op == JAL)
				pipe->stageInst[WBStage].inst.rd = 15;

			// Check if SC
			if (pipe->stageInst[WBStage].inst.op == SC)
				pipe->stageInst[WBStage].data = pipe->stageInst[WBStage].inst.rdData;

			// Store the write for next cycle
			pipe->stageStat[WBStage].delayedWrite = True;
			pipe->stageStat[WBStage].delayedWriteReg = pipe->stageInst[WBStage].inst.rd;
			pipe->stageStat[WBStage].delayedWriteData = pipe->stageInst[WBStage].data;
		}
		if ( pipe->stageInst[WBStage].inst.op == HALT )
			pipe->totally_done = True;
		
		if (pipe->stageInst[WBStage].inst.type != S)
			pipe->wbUtil++;
	}

	if (pipe->stageStat[WBStage].stallNextCycle)
	{
		pipe->stageStat[WBStage].stalled = True;  // Stall next cycle
		pipe->stageStat[WBStage].stallNextCycle = False;
	}
}

// Put all stages except WB in stall mode and wait
// The initiator of the stall is Stage st
void freezePipeline( Pipeline* pipe, Stage st )
{
	int i;

	for (i = IFStage; i < st; i++)
		pipe->stageStat[i].stallNextCycle = True;		
	
	for (i = st; i < NumStages; i++)
		pipe->stageStat[i].stalled = True;
}

// Unfreeze pipeline
void unfreezePipeline(Pipeline* pipe)
{
	int i;

	for (i = 0; i < NumStages; i++)
	{
		pipe->stageStat[i].stalled = False;
		pipe->stageStat[i].stallNextCycle = False;
	}
}

// All of the helper methods used by parser()
void trimInstruction(char* Instruction) 
{
	int Stage = 0; // How many times we've gone from not a space to a space
	int newIndex = 0;
	char temp[100];
	int i;
	bool lastCharWasSpace = True;
	for (i = 0; Instruction[i] != '\0'; i++) 
	{
		if (isAValidCharacter(Instruction[i]))  // Is alphanumeric or a dollar sign or a comma or a dash or a parentheses
		{
			temp[newIndex++] = Instruction[i];
			lastCharWasSpace = False;
		}
		else 
		{ //Basically is a space
			if (Instruction[i] == ';')  // Comment
				break;
			if (!lastCharWasSpace) 
			{
				Stage++;
				if (Stage == 1) {
					temp[newIndex++] = ' ';
				}
			}
			lastCharWasSpace = True;
		}
	}
	temp[newIndex++] = '\0';
	strcpy(Instruction, temp);
}

bool isAValidCharacter(char c) 
{
	return isalnum(c) || c == '$' || c == ',' || c == '-' || c == '(' || c == ')';
}

bool isAValidReg(char c) 
{
	return isalnum(c) || c == '$';
}

char* extractOpcode(char* Instruction) 
{
	char* opcode = (char*) malloc( sizeof(char) * (MAX_OPCODE+1) );
	int i;

	if (opcode == NULL)
		return NULL;

	for (i = 0; Instruction[i] != ' '; i++) {
		opcode[i] = Instruction[i];
	}
	opcode[i] = '\0';
	return opcode;
}

int extractRegister(char* Instruction, int index) 
{
	int i;
	int regIndex = 0;
	int charIndex = 0;
	bool readOpcode = False;
	char reg[MAX_REG_NAME + 1]; //Must be able to hold $rnn, which is 4 characters and the '\0' character

	for (i = 0; Instruction[i] != '\0'; i++) 
	{
		if (readOpcode && Instruction[i] == ',') {
			regIndex++;
		}
		if (readOpcode && isAValidReg(Instruction[i]) && index == regIndex) {
			reg[charIndex++] = Instruction[i];
		}
		if (Instruction[i] == ' ') {
			readOpcode = True;
		}
	}
	reg[charIndex++] = '\0';
	
	//Checking if it's a valid register
	if (reg[0] != '$') {
		assert(!"Register didn't start with a dollar sign");
	}
	//Set regValue to  the string without the dollar sign
	int regVal = regValue(reg + 1);
	//Register is invalid
	if (regVal == -1) 
	{
		assert(!"Unrecognized register name");
	}
	return regVal;
}

int extractImmediate(char* Instruction, int index) 
{
	int i;
	int regIndex = 0;
	int charIndex = 0;
	bool readOpcode = False;
	char immStr[6]; //To be able to hold $zero, which is 5 characters and the null character
	int imm;

	for (i = 0; Instruction[i] != '\0' && Instruction[i] != '('; i++) 
	{
		if (readOpcode && Instruction[i] == ',') {
			regIndex++;
		}
		if (readOpcode && Instruction[i] != ',' && regIndex == index) {
			immStr[charIndex++] = Instruction[i];
		}
		if (Instruction[i] == ' ') {
			readOpcode = True;
		}
	}
	immStr[charIndex++] = '\0';
	//Make sure everything is a digit, or its the first character and its -
	for (i = 0; immStr[i] != '\0'; i++) {
		if (!isdigit(immStr[i])) 
		{
			if (!(i == 0 && immStr[i] == '-')) 
				assert(!"Immediate field contained incorrect value");
		}
	}
	imm = atoi(immStr);
	if ( imm > (1 << 11)-1 || imm < -(1 << 11) )
		assert(!"Immediate field too large");

	return imm;
}

bool isRegularType(opcode opc) 
{
	if ( opc ==  ADD || opc == SUB || opc == AND || opc == OR || opc == XOR || opc == MUL || 
		 opc == SLL || opc == SRL || opc == SRA || opc == LW || opc == LL )		
		return True;

	return False;
}

bool isJal(opcode opc)
{
	if ( opc == JAL )
		return True;

	return False;
}

bool isBranch(opcode opc)
{
	if ( opc == BEQ || opc == BNE || opc == BLT || opc == BGT || opc == BLE || opc == BGE )		 
		return True;

	return False;
}

bool isStore(opcode opc)
{
	if ( opc == SW || opc == SC )
		return True;

	return False;
}

bool isLoad(opcode opc)
{
	if (opc == LW || opc == LL)
		return True;

	return False;
}

bool isHalt(opcode opc)
{
	if ( opc == HALT )
		return True;

	return False;
}

//Returns the location (0-15) of the register from the name
//Note: This takes in a register without the dollar sign, ex. s3, v0, 20, 5, zero
int regValue(char* c) 
{
	/* Check if /* $r0-$r15 (or $R0-$R15) format or $0-$15 format */
	if (c[0] == 'r' || c[0] == 'R') 
		c++;
	
	
	/* All the characters from now till end of c must be numbers
	   If they are, then we convert it to a number and check if its between 0 and 15
	   Otherwise: failure (return -1) */
	int i = 0;
	while ( c[i] != '\0' ) 
	{
		if ( !isdigit( c[i]) )
			return -1;
		i++;
	}

	/* If we've made it here, the string is a number */
	int regIndex = atoi(c);
	if ( regIndex >= 0 && regIndex <= NUM_REGS-1 ) 
		return regIndex;	
	else 
		assert(!"Register number is out of bounds");

	return -1;
}


int checkHazard( Pipeline* pipe )
{
	//If we have a hazard on register 0 or register 1, we don't actually have a hazard
	Instruction inst = pipe->stageInst[IDStage].inst;
	int result;

	if (pipe->stageInst[IDStage].inst.type != S) 
	{
		// Check rs register
		result = checkHazardRegister(pipe, inst.rs);
		if (result != -1)
			return result;		

		// Check rt register
		result = checkHazardRegister(pipe, inst.rt);
		if (result != -1)
			return result;

		// Check rd register, only for store instructions ()
		if (pipe->stageInst[IDStage].inst.type == STR)
		{
			result = checkHazardRegister(pipe, inst.rd);
			if (result != -1)
				return result;
		}
	}
	return -1;
}

int checkHazardRegister(Pipeline* pipe, int reg)
{
	Instruction inst = pipe->stageInst[IDStage].inst;

	// Check rt register
	if (inst.type == Reg || inst.type == B || inst.type == STR)
	{
		if (!pipe->stageStat[EXStage].stalled && pipe->stageInst[EXStage].inst.type != S &&
			reg == pipe->stageInst[EXStage].inst.rd && pipe->stageInst[EXStage].inst.type != STR)
		{
			if (inst.rs != 0 && inst.rs != 1)
			{
				pipe->dataHazardStallCycles = 3;
				pipe->stalledDataHazard = True;
				return reg;
			}
		}

		if (!pipe->stageStat[MEMStage].stalled && pipe->stageInst[MEMStage].inst.type != S &&
			reg == pipe->stageInst[MEMStage].inst.rd && pipe->stageInst[MEMStage].inst.type != STR)
		{
			if (inst.rs != 0 && inst.rs != 1)
			{
				pipe->dataHazardStallCycles = 2;
				pipe->stalledDataHazard = True;
				return reg;
			}
		}

		if (!pipe->stageStat[WBStage].stalled && pipe->stageInst[WBStage].inst.type != S &&
			reg == pipe->stageInst[WBStage].inst.rd && pipe->stageInst[WBStage].inst.type != STR)
		{
			{
				pipe->dataHazardStallCycles = 1;
				pipe->stalledDataHazard = True;
				return reg;
			}
		}
	}

	return -1;
}

opcode stringToOpcode(char* opcode) {
	if (strcmp(opcode, "add") == 0)
		return ADD;
	
	else if (strcmp(opcode, "sub") == 0)
		return SUB;
	
	else if (strcmp(opcode, "and") == 0)
		return AND;
	
	else if (strcmp(opcode, "or") == 0)
		return OR;
	
	else if (strcmp(opcode, "xor") == 0)
		return XOR;

	else if (strcmp(opcode, "mul") == 0)
		return MUL;

	else if (strcmp(opcode, "sll") == 0)
		return SLL;
	
	else if (strcmp(opcode, "sra") == 0)
		return SRA;
	
	else if (strcmp(opcode, "srl") == 0)
		return SRL;

	else if (strcmp(opcode, "beq") == 0)
		return BEQ;
	
	else if (strcmp(opcode, "bne") == 0)
		return BNE;
	
	else if (strcmp(opcode, "blt") == 0)
		return BLT;
	
	else if (strcmp(opcode, "bgt") == 0)
		return BGT;

	else if (strcmp(opcode, "ble") == 0)
		return BLE;

	else if (strcmp(opcode, "bge") == 0)
		return BGE;

	else if (strcmp(opcode, "lw") == 0)
		return LW;

	else if (strcmp(opcode, "sw") == 0)
		return SW;

	else if (strcmp(opcode, "ll") == 0)
		return LL;

	else if (strcmp(opcode, "sc") == 0)
		return SC;

	else if (strcmp(opcode, "halt") == 0) 
		return HALT;
	/*
		else if (strcmp(opcode, "nop") == 0)
			return STALL;
	*/	
	return NONE;
}

void printStatistics( Pipeline* pipe )
{
	printf("IF Utilization: %.2f%%\n",  1.0 * pipe->ifUtil  / pipe->totalCycles * 100);
	printf("ID Utilization: %.2f%%\n",  1.0 * pipe->idUtil  / pipe->totalCycles * 100);
	printf("EX Utilization: %.2f%%\n",  1.0 * pipe->exUtil  / pipe->totalCycles * 100);
	printf("MEM Utilization: %.2f%%\n", 1.0 * pipe->memUtil / pipe->totalCycles * 100);
	printf("WB Utilization: %.2f%%\n",  1.0 * pipe->wbUtil  / pipe->totalCycles * 100);
	printf("Execution Time (Cycles): %d\n", pipe->totalCycles);
}

void printRegisters( Pipeline* pipe )
{
	int i;

	for (i = 0; i < NUM_REGS; i++) 
		printf("Register $R%d: %d\n", i, pipe->registers[i]);	
}
