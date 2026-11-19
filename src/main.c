#include <rgl.h>
#include <shader_util.h>
#include <rmbrt-font/font.h>
#include <rmbrt-ui/view.h>
#include <rmbrt-ui/render.h>
#include <rmbrt-ui/mouse.h>
#include <rmbrt-ui/elem.h>
#include <rmbrt-ui/textfield.h>
#include <range.h>
#include <input.h>
#include "vertex.h"
#include "shader.h"

#include "data_layout.h"
#include "ram_vis.h"
#include "common.h"

static char const fragment_shader[] = {
#embed "fragment_shader.glsl"
	,'\n'
};

static char const vertex_shader[] = {
#embed "vertex_shader.glsl"
	,'\n'
};

RGL_HANDLE(RGLShaderProgram) program;
RGL_HANDLE(RGLBuffer) vbo;
RGL_HANDLE(RGLBuffer) vbo_lines;


void on_after_file_drop(void *, u32);

static DECL_ATTR_LOCS(Shader);
static DECL_UNIFORM_LOCS(Shader);

RGL_HANDLE(RGLPipelineState) pipeline;
RGL_HANDLE(RGLVertexFetch) vao;
RGL_HANDLE(RGLVertexFetch) vao_lines;

static void init_memory();

EXPORT void on_start()
{
	rui_init();
	init_font();

	// GL setup

	// SHADER
	rgl_new_shader_programs(1, &program);
	rgl_shader_program_compile_vertex_fragment_program_from_glsl(
		program,
		vertex_shader, sizeof(vertex_shader)-1,
		fragment_shader, sizeof(fragment_shader)-1,
		ATTR_COUNT(Shader), ATTR_NAMES(Shader), ATTR_LOCS(Shader).array,
		UNIFORM_COUNT(Shader), UNIFORM_NAMES(Shader), UNIFORM_LOCS(Shader).array,
		0);

	void setup_normal_vao();
	setup_normal_vao();

	struct RGLPipelineConfig config = {
		.bake_control = RGL_ALL_PIPELINE_FIELDS,
		.program = program,
		.vertex_fetch = vao
	};

		struct RGLRasteriserConfig raster = RGL_DEFAULT_RASTER_CONFIG;
		raster.depth_test = RGL_DEPTH_DISABLED;
	rgl_new_rasterisers(1, &config.rasteriser, &raster);

	rgl_new_pipelines(1, &pipeline, &config);

	init_memory();
}

void setup_normal_vao()
{
	static RGL_HANDLE(RGLVertexLayout) s_vertex_layout;
	rgl_new_vertex_layouts(1, &s_vertex_layout, &RGL_VERTEX_LAYOUT(Vertex));

	static struct RGLVertexBindingDesc binding_desc;
	RGL_VERTEX_LINK_LOCATIONS(Vertex, Shader, &binding_desc);
	RGL_HANDLE(RGLVertexBinding) binding;
	rgl_new_vertex_bindings(1, &binding, &binding_desc);

	rgl_new_buffers(1, &vbo,
		&(struct RGLBufferParams const) { s_vertex_layout, GL_TRIANGLES });
	rgl_new_buffers(1, &vbo_lines,
		&(struct RGLBufferParams const) { s_vertex_layout, GL_LINES });

	rgl_new_vertex_fetches(1, &vao, &(struct RGLVertexFetchDesc const) {
		.primitive = GL_TRIANGLES,
		.source_count = 1,
		.sources = {{ .buffer = vbo, .binding = binding }}
	});

	rgl_new_vertex_fetches(1, &vao_lines, &(struct RGLVertexFetchDesc const) {
		.primitive = GL_LINES,
		.source_count = 1,
		.sources = {{ .buffer = vbo_lines, .binding = binding }}
	});
}


u64 parse_int(struct String text) {
	u64 acc = 0;
	for(u32 i = 0; i < text.length; i++)
		acc = 10 * acc + (text.ptr[i] - '0');
	return acc;
}

u32 utoa_into(char * buffer, u64 number, char ksep) {
	u64 num =   10000000000000000000u;
	u32 k   = 0b01001001001001001000u;
	u8 i = 20;
	bool had_digit = false;
	char * write = buffer;
	while(num)
	{
		--i;
		u8 digit = number / num;
		number = number % num;
		num /= 10;
		if(digit || had_digit)
		{
			*write++ = '0'+digit;
			had_digit = true;
			if(ksep && (k & (1<<i))) *write++ = ksep;
		}

	}
	if(!had_digit)
		*write++ = '0';

	return write - buffer;
}

u32 itoa_into(char * buffer, i64 number, char ksep) {
	char * write = buffer;
	u64 abs = number;
	if(number < 0)
	{
		*write++ = '-';
		abs = -number;
	}

	write += utoa_into(write, abs, ksep);
	return (write - buffer);
}

IMPORT void schedule_repaint(bool force_repaint);

static struct Scroll {
	i64 offset;

	i64 _target_offset;
	i64 _offset;

	u64 scroll_start_time;
	u64 scroll_end_time;
} g_x_scroll = {}, g_y_scroll = {};

enum { SCROLL_EASE_DUR = 1000000 / 16, SCROLL_TOTAL_DUR = 1000000 / 12 };

static bool is_scrolling(struct Scroll * scroll, u64 now)
{
	return scroll->scroll_end_time > now;
}

static void scroll_add(struct Scroll * scroll, i64 dist, u64 now)
{
	if(!is_scrolling(scroll, now))
			scroll->scroll_start_time = now;
	scroll->scroll_end_time = now + SCROLL_TOTAL_DUR;
	scroll->_target_offset += dist << 16;
}

static void scroll_set(struct Scroll * scroll, i64 offset)
{
	scroll->offset = offset;
	scroll->_target_offset = offset << 16;
	scroll->_offset = offset << 16;
	scroll->scroll_start_time = 0;
	scroll->scroll_end_time = 0;
}

static void scroll_add_instant(struct Scroll * scroll, i64 dist)
{
	scroll->_target_offset += dist << 16;
	scroll->_offset += dist << 16;
}

static void scroll_animate(struct Scroll * scroll, u64 now, u64 dt)
{
	if(!is_scrolling(scroll, now))
	{
		scroll->_offset = scroll->_target_offset;
		scroll->offset = scroll->_offset >> 16;
		return;
	}

	u64 time_left = scroll->scroll_end_time - now;
	i64 distance_left = scroll->_target_offset - scroll->_offset;

	if(dt < time_left)
	{
		scroll->_offset += (i64)(distance_left * dt) / (i64)time_left;
		scroll->offset = scroll->_offset >> 16;
	} else
	{
		scroll->_offset = scroll->_target_offset;
		scroll->offset = scroll->_offset >> 16;
	}
}

static u16 last_screen_h = 0;
static u64 g_time = 0;

enum Interpret g_interpret = INTERPRET;


EXPORT bool on_mouse_press(enum MouseButton button, i32 x, i32 y, enum ModifierKeys mods)
{
	mouse_handle_press(button, x, y);
	bool ret = rui_handle_mouse_press(x, (i32)rui_frame_h - y, button, mods);
	if(rui_needs_redraw()) schedule_repaint(true);
	return ret;
}

EXPORT bool on_mouse_release(enum MouseButton button, i32 x, i32 y, enum ModifierKeys mods)
{
	mouse_handle_release(button, x, y);
	bool ret = rui_handle_mouse_release(x, (i32)rui_frame_h - y, button, mods);
	if(rui_needs_redraw()) schedule_repaint(true);
	return ret;
}

EXPORT bool on_mouse_move(i32 x, i32 y, enum ModifierKeys mods)
{
	mouse_handle_move(x, y, mods);
	bool ret = rui_handle_mouse_move(x, (i32)rui_frame_h - y, mods);
	if(rui_needs_redraw()) schedule_repaint(true);
	return ret;
}


EXPORT u8 on_keydown(enum PhysicalKeyboard key, u32 unicode, enum ModifierKeys modifiers)
{
	(void) key;
	(void) unicode;
	(void) modifiers;

#ifdef RMBRT_RUNTIME_WASM
	// suppress keys that the browser would handle.
	if(key == PK_F5
	|| (modifiers == MK_CTRL && (unicode & ~32) == 'R')
	|| (modifiers == MK_ALT && (u8)(unicode - '0') < 10)
	|| key == PK_CONTEXT)
		return true;
#endif

	if(rui_handle_keydown(key, unicode, modifiers))
	{
		if(rui_needs_redraw())
			schedule_repaint(true);
	} else if(key == PK_PAGE_UP) {
		scroll_add(&g_y_scroll, -(last_screen_h / 3), g_time);
		schedule_repaint(true);
	} else if(key == PK_PAGE_DOWN) {
		scroll_add(&g_y_scroll, last_screen_h / 3, g_time);
		schedule_repaint(true);
	} else if(key == PK_UP)
	{
		scroll_add_instant(&g_y_scroll, -16);

		schedule_repaint(true);
	} else if(key == PK_DOWN)
	{
		scroll_add_instant(&g_y_scroll, 16);

		schedule_repaint(true);
	} else if(key == PK_LEFT)
	{
		scroll_add(&g_x_scroll, -40, g_time);

		schedule_repaint(true);
	} else if(key == PK_RIGHT)
	{
		scroll_add(&g_x_scroll, 40, g_time);

		schedule_repaint(true);
	} else return false;

	return true;
}


struct DrawBuffer g_imBuf = DB_INIT(struct Vertex, GL_TRIANGLES);
struct DrawBuffer g_imBuf_lines = DB_INIT(struct Vertex, GL_LINES);

static bool is_animating = false;

static i64 g_max_x_render = 0;
static i64 g_min_y_render = 0;

static u8 _memory[1024];
static enum DataType _layout[1024];
struct Memory g_memory = { _memory, _layout, sizeof(_memory) };

static void init_memory()
{
	unsigned seed = 0x133bacdf;
	for(unsigned i = 0; i < _Countof(_memory); i++)
	{
		seed = seed * seed % 0xbcadef35 + i;
		*(u8 *)&_memory[i] = seed;
		_layout[i] = DT_U1; // seed % _Countof(k_data_types);
	}

	memset(_layout, DT_ANY, sizeof(_layout));
	memset(_layout+sizeof(_layout) - 8, DT_ANY, 8);

	{
		auto str = String("Hello, world!\nTest 1 2 3");
		_layout[0] = DT_U2;
		_layout[2] = DT_U2;
		*(u16 *)&_memory[0] = 4;
		*(u16 *)&_memory[2] = str.length;
		memset(_layout+4, DT_CHAR, str.length +1);
		_memory[4+memcpy_string(_memory+4, str)] = 0;
		_layout[32] = DT_S8;
		_layout[560] = DT_S4;
		_layout[560+32] = DT_U8;
		_layout[560+32+8] = DT_U8;
		_layout[560+64] = DT_S4;
		_layout[560+64+4] = DT_U4;
	}
}

/* TODO:
	- [X] refactor layout logic into separate file & functions.
	- [X] use Views, so we can handle clicking and hovering of cells (one view for the memory grid, not per cell).
	- [ ] when hovering a cell, we want to see its possible interpretations, and also explanations for things like EOL / \n, END / \0. And we want to show the ranges of types.
	- [X] add registers.
	- [X] add instructions & execution.
	- [ ] add visualisation of active instruction (inputs, results, sources, destinations, description).
	- [ ] extend font with lowercase letters.
	- [ ] add console for printing, and maybe for typing.
	- [ ] add text field for code.
		- [ ] add assembly language.
		- [ ] add higher language. C-like?
	- [ ] add pointers, hovering over a pointer will highlight the cell it points to.
	- [ ] add tutorial (maybe we can make the book as part of the program, instead of a separate document). We could even turn this into a puzzle game.
	- [ ] add automatic saving of the last state.
	- [ ] undo/redo?
	- [ ] interactive programs? audio output?
	- [ ] performance simulation, cache simulation, etc..
*/

static i64 g_reg_a = 0;
static i64 g_reg_b = 0;
struct RUINumberTextField g_reg_x_field = {
	.min = -0x8000000000000000,
	.max =  0x7fffffffffffffff,
	.default_value = 0
};
extern i64 highlight_addr;

EXPORT bool on_frame(u16 w, u16 h, u32 dt)
{
	static char buffer[256];

	rui_begin_frame(w, h);
	//before_frame(w,h, dt);

	(void) g_imBuf;

	last_screen_h = h;

	if(!is_animating) dt = 0;
	is_animating = false;


	bool _is_scrolling = is_scrolling(&g_x_scroll, g_time) || is_scrolling(&g_y_scroll, g_time);

	scroll_animate(&g_x_scroll, g_time, dt);
	scroll_animate(&g_y_scroll, g_time, dt);

	if(g_x_scroll.offset >= g_max_x_render) scroll_set(&g_x_scroll, g_max_x_render-1);
	if(g_x_scroll.offset <= -w) scroll_set(&g_x_scroll, -w+1);

	if(g_y_scroll.offset >= -g_min_y_render+h) scroll_set(&g_y_scroll, -g_min_y_render+h-1);
	if(g_y_scroll.offset <= -h) scroll_set(&g_y_scroll, -h+1);

	g_max_x_render = 64 * 28 + 64;
	g_min_y_render = h + 16/2;

	union {
		u32 word;
		struct { u8 a, b, g, r; };
	} const bg = { 0xccccccff };
	glColorMask(1,1,1,1); // fully clear the buffer.
		glClearColor(bg.r/255.f, bg.g/255.f, bg.b/255.f, bg.a/255.f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glColorMask(1,1,1,0); // do not modify alpha when rendering. WebGL uses the alpha during compositing and might automatically premultiply the alpha of the colour buffer.

	glViewport(0,0,w,h);

	rgl_use_pipeline(pipeline);
	glUniform2f(UNIFORM_LOCS(Shader).screen_size, (float) w * 4, (float) h * 4);

	{
		static struct {
			struct String text;
			enum Interpret value;
		} const k_interpret_buttons[] = {
			{ String("Show Interpretation"), INTERPRET },
			{ String("Raw View (Hexadecimal)"), NO_INTERPRET_HEX },
			{ String("Raw View (Decimal)"), NO_INTERPRET_DEC }
		};

		struct u16Quad q = {{8, 200}, {200, 24}};
		db_string(String("View options"), q.pos.x, q.pos.y, 0xff000000, 0xffcccccc);
		q.pos.y -= 24;

		for(unsigned i = 0; i < _Countof(k_interpret_buttons); i++)
		{
			if(rui_draw_button(k_interpret_buttons[i].text, q))
				g_interpret = k_interpret_buttons[i].value;
			q.pos.y -= 30;
		}
	}

	{
		enum Command : u8 {
			CMD_NOP,
			CMD_SET_A,
			CMD_LOAD_A_U1,
			CMD_LOAD_A_U2,
			CMD_LOAD_A_U4,
			CMD_LOAD_A_U8,
			CMD_LOAD_A_S1,
			CMD_LOAD_A_S2,
			CMD_LOAD_A_S4,
			CMD_LOAD_A_S8,

			CMD_SET_B,
			CMD_LOAD_B_U1,
			CMD_LOAD_B_U2,
			CMD_LOAD_B_U4,
			CMD_LOAD_B_U8,
			CMD_LOAD_B_S1,
			CMD_LOAD_B_S2,
			CMD_LOAD_B_S4,
			CMD_LOAD_B_S8,

			CMD_ADD,
			CMD_SUB,
			CMD_MUL,
			CMD_B_SET_A,
			CMD_A_SET_B,
			CMD_X_SET_A,
			CMD_X_SET_B,
			CMD_SWAP_A_B,

			CMD_STORE_A_AT_X_1,
			CMD_STORE_A_AT_X_2,
			CMD_STORE_A_AT_X_4,
			CMD_STORE_A_AT_X_8,

			CMD_STORE_X_AT_A_1,
			CMD_STORE_X_AT_A_2,
			CMD_STORE_X_AT_A_4,
			CMD_STORE_X_AT_A_8
		};

		static struct {
			struct String text;
			enum Command value;
		} const k_command_buttons[] = {
			{ String("A = X"), CMD_SET_A },
			{ String("B = X"), CMD_SET_B },
			{ String("A = B"), CMD_A_SET_B },
			{ String("B = A"), CMD_B_SET_A },
			{ String("X = A"), CMD_X_SET_A },
			{ String("X = B"), CMD_X_SET_B },
			{ String("Swap A and B"), CMD_SWAP_A_B },

			{ String("Compute"), 0 },
			{ String("A = A + B"), CMD_ADD },
			{ String("A = A - B"), CMD_SUB },
			{ String("A = A * B"), CMD_MUL },

			{ String("Load from memory"), 0 },
			{ String("A = u8  @ [X]"), CMD_LOAD_A_U1 },
			{ String("B = u8  @ [X]"), CMD_LOAD_B_U1 },
			{ String("A = i8  @ [X]"), CMD_LOAD_A_S1 },
			{ String("B = i8  @ [X]"), CMD_LOAD_B_S1 },
			{ String("A = u16 @ [X]"), CMD_LOAD_A_U2 },
			{ String("B = u16 @ [X]"), CMD_LOAD_B_U2 },
			{ String("A = i16 @ [X]"), CMD_LOAD_A_S2 },
			{ String("B = i16 @ [X]"), CMD_LOAD_B_S2 },
			{ String("A = u32 @ [X]"), CMD_LOAD_A_U4 },
			{ String("B = u32 @ [X]"), CMD_LOAD_B_U4 },
			{ String("A = i32 @ [X]"), CMD_LOAD_A_S4 },
			{ String("B = i32 @ [X]"), CMD_LOAD_B_S4 },
			{ String("A = u64 @ [X]"), CMD_LOAD_A_U8 },
			{ String("B = u64 @ [X]"), CMD_LOAD_B_U8 },
			{ String("A = i64 @ [X]"), CMD_LOAD_A_S8 },
			{ String("B = i64 @ [X]"), CMD_LOAD_B_S8 },

			{ String("Store to memory"), 0 },
			{ String("u8  @ [X] = A"), CMD_STORE_A_AT_X_1 },
			{ String("u8  @ [A] = X"), CMD_STORE_X_AT_A_1 },
			{ String("u16 @ [X] = A"), CMD_STORE_A_AT_X_2 },
			{ String("u16 @ [A] = X"), CMD_STORE_X_AT_A_2 },
			{ String("u32 @ [X] = A"), CMD_STORE_A_AT_X_4 },
			{ String("u32 @ [A] = X"), CMD_STORE_X_AT_A_4 },
			{ String("u64 @ [X] = A"), CMD_STORE_A_AT_X_8 },
			{ String("u64 @ [A] = X"), CMD_STORE_X_AT_A_8 },
		};

		struct u16Quad q = {{w-178, h-16}, {128, 24}};

		{
			auto q_ = q;
			q_.pos.x = q.pos.x-q.size.x-8;
			db_string(String("A:"), q_.pos.x -32, q_.pos.y-2, 0xff000000, 0xffcccccc);
			draw_cell(q_.pos.x, q_.pos.y, DT_U8, g_reg_a, false);
			q_.pos.y -= CELL_H;
			draw_cell(q_.pos.x, q_.pos.y, DT_S8, g_reg_a, false);
			q_.pos.y -= CELL_H;
			for(unsigned i = 0; i < 8; i++)
			{
				u8 c = g_reg_a >> (8*i);
				draw_cell(q_.pos.x + CELL_W * i, q_.pos.y, DT_U1, c, false);
			}
			q_.pos.y -= CELL_H;
			for(unsigned i = 0; i < 8; i++)
			{
				u8 c = g_reg_a >> (8*i);
				draw_cell(q_.pos.x + CELL_W * i, q_.pos.y, DT_ANY, c, false);
			}
			q_.pos.y -= CELL_H;
			for(unsigned i = 0; i < 8; i++)
			{
				u8 c = g_reg_a >> (8*i);
				draw_cell(q_.pos.x + CELL_W * i, q_.pos.y, DT_CHAR, c, false);
			}
			q_.pos.y -= CELL_H;
			q.pos.y = q_.pos.y;
		}
		q.pos.y -= 8;
		{
			auto q_ = q;
			q_.pos.x = q.pos.x-q.size.x-8;
			db_string(String("B:"), q_.pos.x -32, q_.pos.y-2, 0xff000000, 0xffcccccc);
			draw_cell(q_.pos.x, q_.pos.y, DT_U8, g_reg_b, false);
			q_.pos.y -= CELL_H;
			draw_cell(q_.pos.x, q_.pos.y, DT_S8, g_reg_b, false);
			q_.pos.y -= CELL_H;
			for(unsigned i = 0; i < 8; i++)
			{
				u8 c = g_reg_b >> (8*i);
				draw_cell(q_.pos.x + CELL_W * i, q_.pos.y, DT_U1, c, false);
			}
			q_.pos.y -= CELL_H;
			for(unsigned i = 0; i < 8; i++)
			{
				u8 c = g_reg_b >> (8*i);
				draw_cell(q_.pos.x + CELL_W * i, q_.pos.y, DT_ANY, c, false);
			}
			q_.pos.y -= CELL_H;
			for(unsigned i = 0; i < 8; i++)
			{
				u8 c = g_reg_b >> (8*i);
				draw_cell(q_.pos.x + CELL_W * i, q_.pos.y, DT_CHAR, c, false);
			}
			q_.pos.y -= CELL_H;
			q.pos.y = q_.pos.y;
		}
		q.pos.y -= 24;

		rgl_use_pipeline(pipeline);
		db_flush_and_draw(&g_imBuf, vbo);
		db_flush_and_draw_from(&g_imBuf_lines, vbo_lines, vao_lines);
		render_text(w, h);
		rgl_use_pipeline(pipeline);

		db_string(String("Commands"), q.pos.x - q.size.x - 8, q.pos.y, 0xff000000, 0xffcccccc);
		//q.pos.y -= 24;

		db_string(String("X: "), q.pos.x, q.pos.y, 0xff000000, 0xffcccccc);
		rui_number_textfield_draw(&g_reg_x_field, q.pos.x + 3*8, q.pos.y, q.size.x-3*8);
		i64 input_value = rui_number_textfield_get(&g_reg_x_field);
		q.pos.y -= 30;


		enum Command cmd = CMD_NOP;
		unsigned start = 0;
		for(unsigned i = 0; i < _Countof(k_command_buttons); i++)
		{
			unsigned j = i - start;

			auto q_ = q;

			if(k_command_buttons[i].value == CMD_NOP)
			{
				if((j & 1) == 1) q.pos.y -= 30;
				if(i) q.pos.y -= 4;
				q_ = q;
				q_.pos.x -= q.size.x + 8;
				start = i+1; j = 1;

				db_string(k_command_buttons[i].text, q_.pos.x, q_.pos.y, 0xff000000, 0xffcccccc);

				q.pos.y -= 18;
			} else
			{
				if((j & 1) == 0)
					q_.pos.x -= q.size.x + 8;
				if(rui_draw_button(k_command_buttons[i].text, q_))
					cmd = k_command_buttons[i].value;
				if((j & 1) == 1)
					q.pos.y -= 30;
			}

		}

		if(cmd != CMD_NOP) is_animating = true;

		switch(cmd)
		{
		case CMD_NOP: break;
		case CMD_SET_A: { g_reg_a = input_value; logint("X", input_value); } break;
		case CMD_LOAD_A_U1: { g_reg_a = *(u8 const*)&_memory[input_value]; } break;
		case CMD_LOAD_A_U2: { g_reg_a = *(u16 const*)&_memory[input_value]; } break;
		case CMD_LOAD_A_U4: { g_reg_a = *(u32 const*)&_memory[input_value]; } break;
		case CMD_LOAD_A_U8: { g_reg_a = *(u64 const*)&_memory[input_value]; } break;
		case CMD_LOAD_A_S1: { g_reg_a = *(i8 const*)&_memory[input_value]; } break;
		case CMD_LOAD_A_S2: { g_reg_a = *(i16 const*)&_memory[input_value]; } break;
		case CMD_LOAD_A_S4: { g_reg_a = *(i32 const*)&_memory[input_value]; } break;
		case CMD_LOAD_A_S8: { g_reg_a = *(i64 const*)&_memory[input_value]; } break;

		case CMD_SET_B: { g_reg_b = input_value; } break;
		case CMD_LOAD_B_U1: { g_reg_b = *(u8 const*)&_memory[input_value]; } break;
		case CMD_LOAD_B_U2: { g_reg_b = *(u16 const*)&_memory[input_value]; } break;
		case CMD_LOAD_B_U4: { g_reg_b = *(u32 const*)&_memory[input_value]; } break;
		case CMD_LOAD_B_U8: { g_reg_b = *(u64 const*)&_memory[input_value]; } break;
		case CMD_LOAD_B_S1: { g_reg_b = *(i8 const*)&_memory[input_value]; } break;
		case CMD_LOAD_B_S2: { g_reg_b = *(i16 const*)&_memory[input_value]; } break;
		case CMD_LOAD_B_S4: { g_reg_b = *(i32 const*)&_memory[input_value]; } break;
		case CMD_LOAD_B_S8: { g_reg_b = *(i64 const*)&_memory[input_value]; } break;

		case CMD_ADD: { g_reg_a += g_reg_b; } break;
		case CMD_SUB: { g_reg_a -= g_reg_b; } break;
		case CMD_MUL: { g_reg_a *= g_reg_b; } break;
		case CMD_B_SET_A: { g_reg_b = g_reg_a; } break;
		case CMD_A_SET_B: { g_reg_a = g_reg_b; } break;
		case CMD_X_SET_A: { rui_number_textfield_set(&g_reg_x_field, g_reg_a); } break;
		case CMD_X_SET_B: { rui_number_textfield_set(&g_reg_x_field, g_reg_b); } break;
		case CMD_SWAP_A_B: { auto tmp = g_reg_a; g_reg_a = g_reg_b; g_reg_b = tmp; } break;

		case CMD_STORE_A_AT_X_1: { *(u8 *) &_memory[input_value] = g_reg_a; } break;
		case CMD_STORE_A_AT_X_2: { *(u16 *)&_memory[input_value] = g_reg_a; } break;
		case CMD_STORE_A_AT_X_4: { *(u32 *)&_memory[input_value] = g_reg_a; } break;
		case CMD_STORE_A_AT_X_8: { *(u64 *)&_memory[input_value] = g_reg_a; } break;

		case CMD_STORE_X_AT_A_1: { *(u8 *) &_memory[g_reg_a] = input_value; } break;
		case CMD_STORE_X_AT_A_2: { *(u16 *)&_memory[g_reg_a] = input_value; } break;
		case CMD_STORE_X_AT_A_4: { *(u32 *)&_memory[g_reg_a] = input_value; } break;
		case CMD_STORE_X_AT_A_8: { *(u64 *)&_memory[g_reg_a] = input_value; } break;
		default: { panic("Forgot to implement this command!"); }
		}
		if(cmd != CMD_NOP) logint("X", input_value);
	}


	db_string(String("CPU Simulator"), 8-g_x_scroll.offset, h-(8-g_y_scroll.offset), 0xff000000, 0xffcccccc);

	if(highlight_addr != -1)
	{ // memory table top-left index
		signed xp = 8;
		signed yp = CELL_H * -1;

		xp -= g_x_scroll.offset;
		yp -= g_y_scroll.offset;

		unsigned len = utoa_into(buffer, highlight_addr, 0);

		db_text(buffer, len, xp+8*(4-len), h-64-yp -2, 0xff000000, 0xffcccccc);
	}

	// memory table side bar.
	for(unsigned i = 0; i*CACHE_LINE_BYTES < g_memory.size; i++)
	{
		signed xp = 8;
		signed yp = CELL_H * i;

		xp -= g_x_scroll.offset;
		yp -= g_y_scroll.offset;

		unsigned len = utoa_into(buffer, i*CACHE_LINE_BYTES, 0);

		db_text(buffer, len, xp+8*(4-len), h-64-yp -2, 0xff000000, 0xffcccccc);
	}


	{ // memory table header bar
		db_quad(&g_imBuf, 8+48 - g_x_scroll.offset, h-64+24 + g_y_scroll.offset, CELL_W*CACHE_LINE_BYTES, 20, 0xffdddddd);
		db_quad(&g_imBuf, 8+48 + CELL_W * CACHE_LINE_BYTES - g_x_scroll.offset, h-64+24 + g_y_scroll.offset, CELL_W*7, 20, 0xffaaaaaa);
		rgl_use_pipeline(pipeline);
		db_flush_and_draw(&g_imBuf, vbo);

		// memory table header bar labels
		for(unsigned i = 0; i < CACHE_LINE_BYTES + 7; i++)
		{
			signed xp = CELL_W * i + 8 + 48;
			signed yp = CELL_H * -1;

			xp -= g_x_scroll.offset;
			yp -= g_y_scroll.offset;

			unsigned len = utoa_into(buffer, i, 0);

			db_text(buffer, len, xp+8*(4-len) - 8, h-64-yp, 0xff000000, i >= CACHE_LINE_BYTES ? 0xffaaaaaa : 0xffdddddd);
		}
	}
	rgl_use_pipeline(pipeline);
	db_flush_and_draw(&g_imBuf, vbo);
	db_flush_and_draw_from(&g_imBuf_lines, vbo_lines, vao_lines);
	render_text(w, h);

	{ // memory table
		extern struct RUIViewDesc ram_view;
		i32 top = rui_frame_h - 64;
		ram_view.x = 56 - g_x_scroll.offset;
		ram_view.y = top + g_y_scroll.offset - ram_view.h;
		ram_view.callback_repaint(&ram_view, dt);
	}

	rgl_use_pipeline(pipeline);
	db_flush_and_draw(&g_imBuf, vbo);
	db_flush_and_draw_from(&g_imBuf_lines, vbo_lines, vao_lines);
	render_text(w, h);


	glFinish();

	//after_frame();

	g_time += dt;

	is_animating |= _is_scrolling;

	rui_end_frame();
	mouse_after_frame(w, h);
	return !is_animating;
}