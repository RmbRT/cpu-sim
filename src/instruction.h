#ifndef __instruction_h_defined
#define __instruction_h_defined

#include "data_layout.h"


enum Argument : u8 {
	ARG_NONE,
	ARG_IMMEDIATE,
	ARG_REG0, ARG_REG_A = ARG_REG0,
	ARG_REG_B,
	ARG_REG_C,
	ARG_REG_D,

	ARG_END
};

enum Operation : u8 {
	OP_NONE,
	OP_TERMINATE,

	OP_COPY,
	OP_SWAP,

	OP_LOAD_U1,
	OP_LOAD_S1,
	OP_LOAD_U2,
	OP_LOAD_S2,
	OP_LOAD_U4,
	OP_LOAD_S4,
	OP_LOAD_8,

	OP_STORE_1,
	OP_STORE_2,
	OP_STORE_4,
	OP_STORE_8,

	OP_ADD,
	OP_SUB,
	OP_SMUL,
	OP_UMUL,
	OP_SDIV,
	OP_UDIV,
	OP_SMOD,
	OP_UMOD,

	OP_AND,
	OP_OR,
	OP_XOR,
	OP_NOT,
	OP_NEG,

	OP_SHL,
	OP_SHR_U,
	OP_SHR_S,

	OP_SX1,
	OP_SX2,
	OP_SX4,
	OP_ZX1,
	OP_ZX2,
	OP_ZX4,

	OP_ROTL1,
	OP_ROTL2,
	OP_ROTL4,
	OP_ROTL8,
	OP_ROTR1,
	OP_ROTR2,
	OP_ROTR4,
	OP_ROTR8,

	OP_CMP, // set flags

	// set boolean value from flags
	OP_BOOL_LESS_U,
	OP_BOOL_LESS_S,
	OP_BOOL_GREATER_U,
	OP_BOOL_GREATER_S,
	OP_BOOL_LESS_EQUAL_U,
	OP_BOOL_LESS_EQUAL_S,
	OP_BOOL_GREATER_EQUAL_U,
	OP_BOOL_GREATER_EQUAL_S,
	OP_BOOL_EQUAL,
	OP_BOOL_NOT_EQUAL,

	OP_INVERT_BOOL,
	OP_TO_BOOL,

	OP_JUMP,
	OP_JUMP_IF_TRUE,
	OP_JUMP_IF_FALSE,

	OP_END
};

extern struct String const k_operation_mnemonics[OP_END];

// op dst, src1, src2 -- one immediate is allowed. dst must be a register.
struct Instruction {
	enum Operation op;
	enum Argument dst;
	enum Argument src[2];
	u64 immediate;
};

enum Flags : u8 {
	FLAG_EQUAL,
	FLAG_SIGNED_LESS,
	FLAG_UNSIGNED_LESS,
};

struct CPUState {
	u64 flags;
	u64 registers[4];

	bool running;
	i64 termination_code;

	struct Instruction const * program;
	unsigned program_size;
	unsigned ip;

	void * memory;
	u32 memory_size;

	char const * trap_message;
};

void execute_instruction(struct CPUState * cpu, struct Instruction const * instruction);

#endif