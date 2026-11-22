#include "instruction.h"
#include <env.h>

static u64 get_arg(
	u8 arg,
	struct CPUState const * cpu,
	struct Instruction const * instruction)
{
	switch(instruction->src[arg])
	{
	case ARG_NONE: return 0;
	case ARG_IMMEDIATE: return instruction->immediate;
	case ARG_REG_A: return cpu->registers[0];
	case ARG_REG_B: return cpu->registers[1];
	default: panic("unhandled");
	}
}

struct String const k_operation_mnemonics[OP_END] = {
	[OP_NONE] = String("NOP"),
	[OP_TERMINATE] = String("TERMINATE"),

	[OP_COPY] = String("COPY"),
	[OP_SWAP] = String("SWAP"),

	[OP_LOAD_U1] = String("LOAD_U1"),
	[OP_LOAD_S1] = String("LOAD_S1"),
	[OP_LOAD_U2] = String("LOAD_U2"),
	[OP_LOAD_S2] = String("LOAD_S2"),
	[OP_LOAD_U4] = String("LOAD_U4"),
	[OP_LOAD_S4] = String("LOAD_S4"),
	[OP_LOAD_8]  = String("LOAD_8"),

	[OP_STORE_1] = String("STORE_1"),
	[OP_STORE_2] = String("STORE_2"),
	[OP_STORE_4] = String("STORE_4"),
	[OP_STORE_8] = String("STORE_8"),

	[OP_ADD] = String("ADD"),
	[OP_SUB] = String("SUB"),
	[OP_SMUL] = String("SMUL"),
	[OP_UMUL] = String("UMUL"),
	[OP_SDIV] = String("SDIV"),
	[OP_UDIV] = String("UDIV"),
	[OP_SMOD] = String("SMOD"),
	[OP_UMOD] = String("UMOD"),

	[OP_AND] = String("AND"),
	[OP_OR] = String("OR"),
	[OP_XOR] = String("XOR"),
	[OP_NOT] = String("NOT"),
	[OP_NEG] = String("NEG"),

	[OP_SHL] = String("SHL"),
	[OP_SHR_U] = String("SHR_U"),
	[OP_SHR_S] = String("SHR_S"),

	[OP_SX1] = String("SX1"),
	[OP_SX2] = String("SX2"),
	[OP_SX4] = String("SX4"),
	[OP_ZX1] = String("ZX1"),
	[OP_ZX2] = String("ZX2"),
	[OP_ZX4] = String("ZX4"),

	[OP_ROTL1] = String("ROTL1"),
	[OP_ROTL2] = String("ROTL2"),
	[OP_ROTL4] = String("ROTL4"),
	[OP_ROTL8] = String("ROTL8"),
	[OP_ROTR1] = String("ROTR1"),
	[OP_ROTR2] = String("ROTR2"),
	[OP_ROTR4] = String("ROTR4"),
	[OP_ROTR8] = String("ROTR8"),

	[OP_CMP] = String("CMP"),

	[OP_BOOL_LESS_U] = String("LESS_U"),
	[OP_BOOL_LESS_S] = String("LESS_S"),
	[OP_BOOL_GREATER_U] = String("GREATER_U"),
	[OP_BOOL_GREATER_S] = String("GREATER_S"),
	[OP_BOOL_LESS_EQUAL_U] = String("LESS_EQUAL_U"),
	[OP_BOOL_LESS_EQUAL_S] = String("LESS_EQUAL_S"),
	[OP_BOOL_GREATER_EQUAL_U] = String("GREATER_EQUAL_U"),
	[OP_BOOL_GREATER_EQUAL_S] = String("GREATER_EQUAL_S"),
	[OP_BOOL_EQUAL] = String("EQUAL"),
	[OP_BOOL_NOT_EQUAL] = String("NOT_EQUAL"),

	[OP_INVERT_BOOL] = String("INVERT_BOOL"),
	[OP_TO_BOOL] = String("TO_BOOL"),

	[OP_JUMP] = String("JUMP"),
	[OP_JUMP_IF_TRUE] = String("JUMP_IF_TRUE"),
	[OP_JUMP_IF_FALSE] = String("JUMP_IF_FALSE")
};


static char const * validate_instruction(struct Instruction const * instruction, u8 * argc, u8 * memAccessSize)
{
	static struct { u8 args, optArgs; bool dst; u8 memAccess; } const k_op_validation_table[] = {
		[OP_NONE] = { .args = 0 },
		[OP_TERMINATE] = { .args = 1 },

		[OP_COPY] = { .args = 1, .dst = true },
		[OP_SWAP] = { .args = 2 },

		[OP_LOAD_U1] = { .args = 1, .dst = true, .memAccess=1 },
		[OP_LOAD_S1] = { .args = 1, .dst = true, .memAccess=1 },
		[OP_LOAD_U2] = { .args = 1, .dst = true, .memAccess=2 },
		[OP_LOAD_S2] = { .args = 1, .dst = true, .memAccess=2 },
		[OP_LOAD_U4] = { .args = 1, .dst = true, .memAccess=4 },
		[OP_LOAD_S4] = { .args = 1, .dst = true, .memAccess=4 },
		[OP_LOAD_8]  = { .args = 1, .dst = true, .memAccess=8 },

		[OP_STORE_1] = { .args = 2, .memAccess=1 },
		[OP_STORE_2] = { .args = 2, .memAccess=2 },
		[OP_STORE_4] = { .args = 2, .memAccess=4 },
		[OP_STORE_8] = { .args = 2, .memAccess=8 },

		[OP_ADD] = { .args=2, .dst = true },
		[OP_SUB] = { .args=2, .dst = true },
		[OP_SMUL] = { .args=2, .dst = true },
		[OP_UMUL] = { .args=2, .dst = true },
		[OP_SDIV] = { .args=2, .dst = true },
		[OP_UDIV] = { .args=2, .dst = true },
		[OP_SMOD] = { .args=2, .dst = true },
		[OP_UMOD] = { .args=2, .dst = true },

		[OP_AND] = { .args=2, .dst = true },
		[OP_OR] = { .args=2, .dst = true },
		[OP_XOR] = { .args=2, .dst = true },
		[OP_NOT] = { .args=2, .dst = true },
		[OP_NEG] = { .args=2, .dst = true },

		[OP_SHL] = { .args=2, .dst = true },
		[OP_SHR_U] = { .args=2, .dst = true },
		[OP_SHR_S] = { .args=2, .dst = true },

		[OP_SX1] = { .args=1, .dst = true },
		[OP_SX2] = { .args=1, .dst = true },
		[OP_SX4] = { .args=1, .dst = true },
		[OP_ZX1] = { .args=1, .dst = true },
		[OP_ZX2] = { .args=1, .dst = true },
		[OP_ZX4] = { .args=1, .dst = true },

		[OP_ROTL1] = { .args=2, .dst = true },
		[OP_ROTL2] = { .args=2, .dst = true },
		[OP_ROTL4] = { .args=2, .dst = true },
		[OP_ROTL8] = { .args=2, .dst = true },

		[OP_ROTR1] = { .args=2, .dst = true },
		[OP_ROTR2] = { .args=2, .dst = true },
		[OP_ROTR4] = { .args=2, .dst = true },
		[OP_ROTR8] = { .args=2, .dst = true },

		[OP_CMP] = { .args=2 }, // set flags

		// set boolean value from flags
		[OP_BOOL_LESS_U] = { .dst = true },
		[OP_BOOL_LESS_S] = { .dst = true },
		[OP_BOOL_GREATER_U] = { .dst = true },
		[OP_BOOL_GREATER_S] = { .dst = true },
		[OP_BOOL_LESS_EQUAL_U] = { .dst = true },
		[OP_BOOL_LESS_EQUAL_S] = { .dst = true },
		[OP_BOOL_GREATER_EQUAL_U] = { .dst = true },
		[OP_BOOL_GREATER_EQUAL_S] = { .dst = true },
		[OP_BOOL_EQUAL] = { .dst = true },
		[OP_BOOL_NOT_EQUAL] = { .dst = true },

		[OP_INVERT_BOOL] = { .args=1, .dst = true },

		[OP_JUMP] = { .args=1 },
		[OP_JUMP_IF_TRUE] = { .args=2 },
		[OP_JUMP_IF_FALSE] = { .args=2 }
	};

	if(instruction->op >= OP_END) return "illegal opcode";

	{
		bool has_dst = (instruction->dst != ARG_NONE);
		if(has_dst != k_op_validation_table[instruction->op].dst)
			return "illegal instruction";

		if(instruction->dst >= ARG_END || instruction->dst == ARG_IMMEDIATE)
			return "illegal instruction";
	}

	*argc = 0;
	{
		for(unsigned i = _Countof(instruction->src); i--;)
			if(instruction->src[i] != ARG_NONE)
			{
				*argc = i+1;
				break;
			}

		auto desc = k_op_validation_table[instruction->op];
		if(*argc > desc.args + desc.optArgs)
			return "illegal instruction";
	}

	for(unsigned i = 0; i < *argc; i++)
		if(instruction->src[i] >= ARG_END)
			return "illegal instruction";

	if(instruction->op == OP_SWAP)
	{
		if(instruction->src[0] < ARG_REG0
		|| instruction->src[1] < ARG_REG0)
			return "illegal instruction";
	}

	*memAccessSize = k_op_validation_table[instruction->op].memAccess;

	return NULL;
}

void execute_instruction(struct CPUState * cpu, struct Instruction const * instruction)
{
	if(!cpu->running) return;
	if(cpu->trap_message) return;

	if(cpu->ip >= cpu->program_size) {
		cpu->trap_message = "invalid instruction pointer";
		return;
	}

	u8 argc, memAccessSize;
	if((cpu->trap_message = validate_instruction(instruction, &argc, &memAccessSize)))
		return;

	u64 arg0 = get_arg(0, cpu, instruction);
	u64 arg1 = get_arg(1, cpu, instruction);
	u64 * dst = &cpu->registers[instruction->dst - ARG_REG0];

	// Validate all reads and writes.
	if(memAccessSize)
	{
		if(arg0 + memAccessSize > cpu->memory_size)
		{
			cpu->trap_message = "Out of bounds memory access";
			return;
		}
	}

	switch(instruction->op) {
	case OP_NONE: {} break;
	case OP_TERMINATE: {
		cpu->running = false;
		cpu->termination_code = get_arg(0, cpu, instruction);
		return;
	} break;

	case OP_COPY: *dst = arg0; break;
	case OP_SWAP: {
		cpu->registers[instruction->src[0] - ARG_REG0] = arg1;
		cpu->registers[instruction->src[1] - ARG_REG0] = arg0;
	} break;

	case OP_LOAD_U1: *dst = *(u8 const*)&cpu->memory[arg0]; break;
	case OP_LOAD_S1: *dst = *(i8 const*)&cpu->memory[arg0]; break;
	case OP_LOAD_U2: *dst = *(u16 const*)&cpu->memory[arg0]; break;
	case OP_LOAD_S2: *dst = *(i16 const*)&cpu->memory[arg0]; break;
	case OP_LOAD_U4: *dst = *(u32 const*)&cpu->memory[arg0]; break;
	case OP_LOAD_S4: *dst = *(i32 const*)&cpu->memory[arg0]; break;
	case OP_LOAD_8:  *dst = *(u64 const*)&cpu->memory[arg0]; break;

	case OP_STORE_1: *(u8  *)&cpu->memory[arg0] = arg1; break;
	case OP_STORE_2: *(u16 *)&cpu->memory[arg0] = arg1; break;
	case OP_STORE_4: *(u32 *)&cpu->memory[arg0] = arg1; break;
	case OP_STORE_8: *(u64 *)&cpu->memory[arg0] = arg1; break;

	case OP_ADD: *dst = arg0 + arg1; break;
	case OP_SUB: *dst = arg0 - arg1; break;
	case OP_SMUL: *dst = (i64)arg0 * (i64)arg1; break;
	case OP_UMUL: *dst = arg0 * arg1; break;
	case OP_SDIV: {
			if(!arg1) { cpu->trap_message = "Div0"; return; }
			*dst = (i64)arg0 / (i64)arg1;
		} break;
	case OP_UDIV: {
		if(!arg1) { cpu->trap_message = "Div0"; return; }
		*dst = arg0 / arg1;
	} break;
	case OP_SMOD: {
		if(!arg1) { cpu->trap_message = "Div0"; return; }
		*dst = (i64)arg0 % (i64)arg1;
	} break;
	case OP_UMOD: {
		if(!arg1) { cpu->trap_message = "Div0"; return; }
		*dst = arg0 % arg1;
	} break;

	case OP_AND: *dst = arg0 & arg1; break;
	case OP_OR: *dst = arg0 | arg1; break;
	case OP_XOR: *dst = arg0 ^ arg1; break;
	case OP_NOT: *dst = ~arg0; break;
	case OP_NEG: *dst = -arg0; break;

	case OP_SHL: *dst = arg0 << arg1; break;
	case OP_SHR_U: *dst = arg0 >> arg1; break;
	case OP_SHR_S: *dst = (i64)arg0 >> arg1; break;

	case OP_SX1: *dst = (i64)(i8)arg0; break;
	case OP_SX2: *dst = (i64)(i16)arg0; break;
	case OP_SX4: *dst = (i64)(i32)arg0; break;
	case OP_ZX1: *dst = (u8)arg0; break;
	case OP_ZX2: *dst = (u16)arg0; break;
	case OP_ZX4: *dst = (u32)arg0; break;

	case OP_ROTL1:
		*dst = (u8)( (arg0 << (arg1 & 7)) | ((u8)arg0 >> (8 - (arg1 & 7))) ); break;
	case OP_ROTL2:
		*dst = (u16)( (arg0 << (arg1 & 15)) | ((u16)arg0 >> (16 - (arg1 & 15))) ); break;
	case OP_ROTL4:
		*dst = (u32)( (arg0 << (arg1 & 31)) | ((u32)arg0 >> (32 - (arg1 & 31))) ); break;
	case OP_ROTL8:
		*dst = (u64)( (arg0 << (arg1 & 63)) | ((u64)arg0 >> (64 - (arg1 & 63))) ); break;
	case OP_ROTR1:
		*dst = (u8)( (arg0 << (8 - (arg1 & 7))) | ((u8)arg0 >> (arg1 & 7)) ); break;
	case OP_ROTR2:
		*dst = (u16)( (arg0 << (16 - (arg1 & 15))) | ((u16)arg0 >> (arg1 & 15)) ); break;
	case OP_ROTR4:
		*dst = (u32)( (arg0 << (32 - (arg1 & 31))) | ((u32)arg0 >> (arg1 & 31)) ); break;
	case OP_ROTR8:
		*dst = (u64)( (arg0 << (64 - (arg1 & 63))) | ((u64)arg0 >> (arg1 & 63)) ); break;

	case OP_CMP:
		{
			u64 new_flags = 0;
			new_flags |= (u64)(arg0 == arg1) << FLAG_EQUAL;
			new_flags |= (u64)(arg0 < arg1) << FLAG_UNSIGNED_LESS;
			new_flags |= (u64)((i64)arg0 < (i64)arg1) << FLAG_SIGNED_LESS;

			cpu->flags = new_flags;
		} break;

	case OP_BOOL_LESS_U:
		*dst = (bool) (cpu->flags & _Flag64(FLAG_UNSIGNED_LESS)); break;
	case OP_BOOL_LESS_S:
		*dst = (bool) (cpu->flags & _Flag64(FLAG_SIGNED_LESS)); break;
	case OP_BOOL_GREATER_U:
		*dst = !(cpu->flags & (_Flag64(FLAG_UNSIGNED_LESS) | _Flag64(FLAG_EQUAL))); break;
	case OP_BOOL_GREATER_S:
		*dst = !(cpu->flags & (_Flag64(FLAG_SIGNED_LESS) | _Flag64(FLAG_EQUAL))); break;
	case OP_BOOL_LESS_EQUAL_U:
		*dst = (bool)(cpu->flags & (_Flag64(FLAG_UNSIGNED_LESS) | _Flag64(FLAG_EQUAL))); break;
	case OP_BOOL_LESS_EQUAL_S:
		*dst = (bool)(cpu->flags & (_Flag64(FLAG_SIGNED_LESS) | _Flag64(FLAG_EQUAL))); break;
	case OP_BOOL_GREATER_EQUAL_U:
		*dst = !(cpu->flags & _Flag64(FLAG_UNSIGNED_LESS)); break;
	case OP_BOOL_GREATER_EQUAL_S:
		*dst = !(cpu->flags & _Flag64(FLAG_SIGNED_LESS)); break;
	case OP_BOOL_EQUAL:
		*dst = (bool) (cpu->flags & _Flag64(FLAG_EQUAL)); break;
	case OP_BOOL_NOT_EQUAL:
		*dst = ! (cpu->flags & _Flag64(FLAG_EQUAL)); break;

	case OP_TO_BOOL: *dst = (bool) arg0; break;
	case OP_INVERT_BOOL: *dst = !arg0; break;

	case OP_JUMP: cpu->ip = arg0; break;
	case OP_JUMP_IF_TRUE: { if(arg0) cpu->ip = arg1; } break;
	case OP_JUMP_IF_FALSE: { if(!arg0) cpu->ip = arg1; } break;
	case OP_END: panic("this should never happen");
	}

	cpu->ip++;
}