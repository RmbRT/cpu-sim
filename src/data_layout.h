#ifndef __data_layout_h_defined
#define __data_layout_h_defined

#include <range.h>

enum DataType: u8 {
	DT_U1,
	DT_U2,
	DT_U4,
	DT_U8,
	DT_S1,
	DT_S2,
	DT_S4,
	DT_S8,
	DT_CHAR,
	DT_BOOL,
	DT_ANY
};

static struct { int size; bool sign; } const k_data_types[] = {
	[DT_U1] = { 1, false },
	[DT_U2] = { 2, false },
	[DT_U4] = { 4, false },
	[DT_U8] = { 8, false },
	[DT_S1] = { 1, true },
	[DT_S2] = { 2, true },
	[DT_S4] = { 4, true },
	[DT_S8] = { 8, true },
	[DT_CHAR] = { 1, false },
	[DT_BOOL] = { 1, false },
	[DT_ANY] = { 1, false }
};

struct CacheParams {
	u16 latency;
	u8 assoc;
	u8 sets;
};

struct MemoryConfig {
	enum DataType addr_type;

	unsigned max_capacity;
	unsigned page_size;
	unsigned cacheline_size;
	unsigned ram_latency;
	unsigned tlb_size;
	struct CacheParams l1_cache, l2_cache, l3_cache;
};

// We could have a lookup per cell, or we could have a top-down description of the layout.

struct Memory {
	void * data;
	enum DataType * layout;
	unsigned size;
};

struct Variable {
	struct String name;
	u32 address;
	enum DataType type;
};

struct StackFrame {
	u64 start;
	u64 size;

	struct Variable * variables;
	u32 variable_count;
};

#endif