#ifndef __ram_vis_h_defined
#define __ram_vis_h_defined

#include "data_layout.h"

enum { CELL_W = 36, CELL_H = 20, CACHE_LINE_BYTES = 32 };


u8 draw_cell(i32, i32, enum DataType, u64);

#endif