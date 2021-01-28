#ifndef PIPELINE_H_
#define PIPELINE_H_

#include "Shared.h"
#include "Cache.h"
#include "MSIBus.h"

#define MAX_OPCODE 4
#define NUM_REGS 16
#define MAX_REG_NAME 4
#define MAX_INSTR_LEN 100
#define MAX_INSTRUCTIONS 1024

typedef enum { Reg, J, B, STR, H, S } instruction_type;  /* Regular is Reg, JAL is J, branch instructions are B,
														  SW and SC are STR, halt is H  and stall is S */
typedef enum 
{
	NONE = -2, STALL = -1, ADD = 0, SUB = 1, AND = 2, OR = 3, XOR = 4, MUL = 5, SLL = 6, SRA = 7, SRL = 8, BEQ = 9, BNE = 10,
	BLT = 11, BGT = 12, BLE = 13, BGE = 14, JAL = 15, LW = 16, SW = 17, LL = 18, SC = 19, HALT = 20, NOP = 30
} opcode;

typedef enum { IFStage = 0, IDStage, EXStage, MEMStage, WBStage, NumStages } Stage;

typedef struct 
{
	instruction_type type;
	opcode op;
	int rs, rt, rd;
	bool hasImm;
	int imm;
	int rsData, rtData, rdData;
	bool isHalt;
} Instruction;

typedef struct
{
	bool valid;
	int data;
	int addr;
	Instruction inst;
} StageInstruction;

typedef struct
{
	bool stalled;
	bool stallNextCycle;
	bool delayedWrite;
	int delayedWriteReg;
	int delayedWriteData;
} StageStatus;

struct MSIBus;
struct Pipeline
{
	struct MSIBus_* bus;
	Cache cache;
	int registers[NUM_REGS];
	Instruction instruction_mem[MAX_INSTRUCTIONS];

	/* Instructions in each of the stages */
	StageInstruction stageInst[NumStages];

	/* Stage status */
	StageStatus stageStat[NumStages];

	int dataHazardStallCycles;
	bool stalledDataHazard;
	bool flushBranchFlag;
	bool branchTaken;
	bool totally_done;      // True when halt has propagated through the pipeline
	bool interactive_mode;  // If True, print registers after each Instruction, wait for enter to be pressed

	int PC;
	int haltIndex;
	int totalCycles;
	int ifUtil;
	int idUtil;
	int exUtil;
	int memUtil;
	int wbUtil;
};
typedef struct Pipeline Pipeline;
typedef struct Pipeline* PipelinePtr;

Pipeline* createPipeline();
void destroyPipeline(Pipeline* pipe);
void initializePipeline(Pipeline* pipe, char* fileName, struct MSIBus_* bus, Cache cache);
void progScanner( Pipeline* pipe, char* filename );
void parser( Pipeline* pipe, char* inst );
void trimInstruction(char* inst);
bool isAValidCharacter(char ch);
bool isAValidReg(char ch);
char* extractOpcode(char* inst);
int extractRegister(char* inst, int num);
int extractImmediate(char* inst, int num);
opcode stringToOpcode(char* opcode);
bool isRegularType(opcode opc);
bool isJal(opcode opc);
bool isBranch(opcode opc);
bool isStore(opcode opc);
bool isLoad(opcode opc);
bool isHalt(opcode opc);
int regValue(char* inst);
int checkHazard( Pipeline* pipe );
int checkHazardRegister(Pipeline* pipe, int reg);

void runPipelineOneCycle(Pipeline* pipe);
void runPipelineFully(Pipeline* pipe);
// Put all stages in stall mode and wait
// The initiator of the stall is Stage st
void freezePipeline(Pipeline *pipe, Stage st);
// Unfreeze pipeline
void unfreezePipeline( Pipeline* pipe );

void printStatistics( Pipeline* pipe );
void printRegisters ( Pipeline* pipe );

//Stage declarations
void IF ( Pipeline* pipe );
void ID ( Pipeline* pipe );
void EX ( Pipeline* pipe );
void MEM( Pipeline* pipe );
void WB ( Pipeline* pipe );

#endif
