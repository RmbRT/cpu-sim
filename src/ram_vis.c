#include "ram_vis.h"
#include "vertex.h"
#include "common.h"

#include <rmbrt-font/font.h>
#include <rmbrt-ui/view.h>

extern struct DrawBuffer g_imBuf_lines;
extern struct DrawBuffer g_imBuf;

u8 draw_cell(i32 xp, i32 yp, enum DataType type, u64 contents, bool highlight) {
	unsigned cells = k_data_types[type].size;
	unsigned len;

	u32 utoa_into(char *, u64, char);
	u32 itoa_into(char *, i64, char);

	static char buffer[32];
	//unsigned maxlen = 3 * cells;
	switch(type)
	{
	case DT_RAW: case DT_RAW_IGNORE: {
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
		[DT_CHAR] = { 0xffcceecc, 0xff006600 },
		[DT_BOOL] = { 0xffccccee, 0xff000000 },
		[DT_RAW]  = { 0xffb8b8b8, 0xff000000 },
		[DT_RAW_IGNORE]  = { 0xffb8b8b8, 0xff888888 }
	};

	u32 bg = k_bg_colour[type].bg + highlight * 0x00111111;
	u32 fg = k_bg_colour[type].font;

	db_rectangle(&g_imBuf_lines, xp, yp, CELL_W * cells, CELL_H, highlight ? 0xff4444ff : 0xff444444);
	db_quad(&g_imBuf,            xp, yp, CELL_W * cells, CELL_H, bg);
	if(type == DT_CHAR) // center chars horizontally
		db_text(buffer, len, xp+2+CELL_W/2*cells - 4*len, yp-2, fg, bg);
	else // right-align numbers.
		db_text(buffer, len, xp+2+CELL_W*cells - 4 - 8*len, yp-2, fg, bg);

	return cells;
}

static bool callback_repaint(struct RUIViewDesc const *, u32 dt_us);
static struct RUIEventReaction callback_on_mousedown(struct RUIViewDesc const *,
	i16 x, i16 y, enum MouseButton, enum ModifierKeys);
static struct RUIEventReaction callback_on_mouseup(struct RUIViewDesc const *,
	i16 x, i16 y, enum MouseButton, enum ModifierKeys);
static struct RUIEventReaction callback_on_mousemove(struct RUIViewDesc const *,
	i16 x, i16 y, enum ModifierKeys);

struct RUIViewDesc ram_view = {
	.callback_repaint = callback_repaint,
	.callback_on_mousedown = callback_on_mousedown,
	.callback_on_mouseup = callback_on_mouseup,
	.callback_on_mousemove = callback_on_mousemove
};
extern struct Memory g_memory;
extern enum Interpret g_interpret;

i64 highlight_addr = -1;

static bool callback_repaint(struct RUIViewDesc const * this, u32)
{
	if(!ram_view.w) ram_view.w = CACHE_LINE_BYTES * CELL_W;
	if(!ram_view.h)
	{
		ram_view.h = CELL_H * ((g_memory.size+CACHE_LINE_BYTES-1) / CACHE_LINE_BYTES);
		ram_view.y -= ram_view.h;
	}

	auto id = rui_enter_view(this);


	bool has_hl = false;
	i32 hl_x, hl_y, hl_w, hl_h;

	for(unsigned i = 0; i < g_memory.size;)
	{
		unsigned x = i % CACHE_LINE_BYTES;
		unsigned y = i / CACHE_LINE_BYTES;

		signed xp = CELL_W * x;
		signed yp = CELL_H * y;

		void const * cell = &g_memory.data[i];
		auto type = g_memory.layout[i];
		if(g_interpret != INTERPRET) type = g_interpret == NO_INTERPRET_DEC ? DT_U1 : DT_RAW;

		u32 cells = k_data_types[type].size;
		bool highlight = (u64) (highlight_addr - (i64)(u64)i) < cells;

		u8 draw_cell(i32, i32, enum DataType, u64, bool);
		draw_cell(this->x + xp-2, this->y + this->h - yp, type, *(u64 const*)cell, highlight);

		if(highlight)
		{
			has_hl = true;
			hl_x = this->x + xp-2;
			hl_y = this->y + this->h - yp;
			hl_w = CELL_W * cells;
			hl_h = CELL_H;
		}

		i += cells;
	}
	if(has_hl)
	{
		db_rectangle(&g_imBuf_lines, hl_x, hl_y, hl_w, hl_h, 0xff4444ff);

		db_rectangle(&g_imBuf_lines, hl_x, this->y + this->h + 4 + CELL_H, hl_w, CELL_H, 0xff4444ff);
		db_rectangle(&g_imBuf_lines, this->x - 40, hl_y, 32, CELL_H, 0xff4444ff);
	}


	rgl_use_pipeline(pipeline);
	db_flush_and_draw(&g_imBuf, vbo);
	db_flush_and_draw_from(&g_imBuf_lines, vbo_lines, vao_lines);
	render_text(rui_frame_w, rui_frame_h);

	rui_leave_view(id);

	return true;
}

static struct RUIEventReaction callback_on_mousedown(struct RUIViewDesc const * this,
	i16 x, i16 y, enum MouseButton, enum ModifierKeys)
{
	struct RUIEventReaction reaction = { .handled = true };

	unsigned cell_row = ((this->h - y) / CELL_H);
	unsigned cell_col = x / CELL_W;

	i64 cell_addr = (i64)(u64) ( cell_row * CACHE_LINE_BYTES + cell_col );

	{
		i64 start = cell_addr - 7;
		if(start < 0) start = 0;

		for(i64 i = start; i <= cell_addr; i++)
		{
			auto type = g_memory.layout[i];
			if(g_interpret != INTERPRET) type = g_interpret == NO_INTERPRET_DEC ? DT_U1 : DT_RAW;
			if(i + k_data_types[type].size > cell_addr)
				cell_addr = i;
		}
	}

	if(highlight_addr != cell_addr) highlight_addr = cell_addr;

	rui_number_textfield_set(&g_reg_x_field, cell_addr);

	reaction.redraw = true;
	return reaction;
}
static struct RUIEventReaction callback_on_mouseup(struct RUIViewDesc const *,
	i16 x, i16 y, enum MouseButton, enum ModifierKeys)
{
	struct RUIEventReaction reaction = { .handled = true };
	(void) x;
	(void) y;

	return reaction;
}
static struct RUIEventReaction callback_on_mousemove(struct RUIViewDesc const * this,
	i16 x, i16 y, enum ModifierKeys)
{

	struct RUIEventReaction reaction = { .handled = true };

	unsigned cell_row = ((this->h - y) / CELL_H);
	unsigned cell_col = x / CELL_W;

	i64 cell_addr = (i64)(u64) ( cell_row * CACHE_LINE_BYTES + cell_col );

	{
		i64 start = cell_addr - 7;
		if(start < 0) start = 0;

		for(i64 i = start; i <= cell_addr; i++)
		{
			auto type = g_memory.layout[i];
			if(g_interpret != INTERPRET) type = g_interpret == NO_INTERPRET_DEC ? DT_U1 : DT_RAW;
			if(i + k_data_types[type].size > cell_addr)
				cell_addr = i;
		}
	}

	if(highlight_addr != cell_addr)
	{
		highlight_addr = (i64)(u64) cell_addr;
		reaction.redraw = true;
	}

	return reaction;
}