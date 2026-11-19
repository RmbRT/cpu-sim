#include "ram_vis.h"
#include "vertex.h"

#include <rmbrt-font/font.h>

extern struct DrawBuffer g_imBuf_lines;
extern struct DrawBuffer g_imBuf;

u8 draw_cell(i32 xp, i32 yp, enum DataType type, u64 contents) {
	unsigned cells = k_data_types[type].size;
	unsigned len;

	u32 utoa_into(char *, u64, char);
	u32 itoa_into(char *, i64, char);

	static char buffer[32];
	//unsigned maxlen = 3 * cells;
	switch(type)
	{
	case DT_ANY: {
		u8 c = contents;
		len = 2;
		buffer[0] = "0123456789abcdef"[c >> 4];
		buffer[1] = "0123456789abcdef"[c & 15];
	} break;
	case DT_U1: len = utoa_into(buffer, (u8) contents, ','); break;
	case DT_U2: len = utoa_into(buffer, (u16)contents, ','); break;
	case DT_U4: len = utoa_into(buffer, (u32)contents, ','); break;
	case DT_U8: len = utoa_into(buffer, (u64)contents, ','); break;
	case DT_S1: len = itoa_into(buffer, (i8) contents, ','); break;
	case DT_S2: len = itoa_into(buffer, (i16)contents, ','); break;
	case DT_S4: len = itoa_into(buffer, (i32)contents, ','); break;
	case DT_S8: len = itoa_into(buffer, (i64)contents, ','); break;
	case DT_BOOL:
		{
			u8 val = contents;
			if(val == 0)
				len = memcpy_string(buffer, String("no"));
			else if(val == 1)
				len = memcpy_string(buffer, String("yes"));
			else
				len = utoa_into(buffer, val, 0);
		} break;
	case DT_CHAR: {
		len = 1;
		u8 c = contents;
		switch(c)
		{
		case 0: { len = memcpy_string(buffer, String("END")); } break;
		case '\n': { len = memcpy_string(buffer, String("EOL")); } break;
		case '\r': { len = memcpy_string(buffer, String("\\r")); } break;
		default:
			{
				if(c >= 128 || c < 32)
				{
					len = 2;
					buffer[0] = "0123456789abcdef"[c >> 4];
					buffer[1] = "0123456789abcdef"[c & 15];
				} else { len = 1; buffer[0] = c; }
			} break;
		}
	}
	}

	static const struct { u32 bg, font; } k_bg_colour[] = {
		[DT_U1]   = { 0xffeecccc, 0xff000000 },
		[DT_U2]   = { 0xffeecccc, 0xff000000 },
		[DT_U4]   = { 0xffeecccc, 0xff000000 },
		[DT_U8]   = { 0xffeecccc, 0xff000000 },
		[DT_S1]   = { 0xffcceeee, 0xff000000 },
		[DT_S2]   = { 0xffcceeee, 0xff000000 },
		[DT_S4]   = { 0xffcceeee, 0xff000000 },
		[DT_S8]   = { 0xffcceeee, 0xff000000 },
		[DT_CHAR] = { 0xffcceecc, 0xff000000 },
		[DT_BOOL] = { 0xffccccee, 0xff000000 },
		[DT_ANY]  = { 0xffb8b8b8, 0xff888888 }
	};

	db_rectangle(&g_imBuf_lines, xp, yp, CELL_W * cells, 20, 0xff444444);
	db_quad(&g_imBuf,            xp, yp, CELL_W * cells, 20, k_bg_colour[type].bg);
	if(type == DT_CHAR)
		db_text(buffer, len,     xp+2+CELL_W/2*cells - 4*len, yp-2, k_bg_colour[type].font, k_bg_colour[type].bg);
	else
		db_text(buffer, len,     xp+2+CELL_W*cells - 4 - 8*len, yp-2, k_bg_colour[type].font, k_bg_colour[type].bg);

	return cells;
}