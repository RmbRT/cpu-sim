#include <rgl.h>
#include <shader_util.h>
#include <rmbrt-font/font.h>
#include <rmbrt-ui/view.h>
#include <rmbrt-ui/render.h>
#include <range.h>
#include <input.h>
#include "vertex.h"
#include "shader.h"

#include "data_layout.h"

static char const fragment_shader[] = {
#embed "fragment_shader.glsl"
	,'\n'
};

static char const vertex_shader[] = {
#embed "vertex_shader.glsl"
	,'\n'
};

static RGL_HANDLE(RGLShaderProgram) program;
static RGL_HANDLE(RGLBuffer) vbo;
static RGL_HANDLE(RGLBuffer) vbo_lines;


void on_after_file_drop(void *, u32);

static DECL_ATTR_LOCS(Shader);
static DECL_UNIFORM_LOCS(Shader);

static RGL_HANDLE(RGLPipelineState) pipeline;
static RGL_HANDLE(RGLVertexFetch) vao;
static RGL_HANDLE(RGLVertexFetch) vao_lines;

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

u32 utoa_into(char * buffer, u64 number) {
	u64 num = 10000000000000000000u;
	bool had_digit = false;
	char * write = buffer;
	while(num)
	{
		u8 digit = number / num;
		number = number % num;
		num /= 10;
		if(digit || had_digit)
		{
			*write++ = '0'+digit;
			had_digit = true;
		}
	}
	if(!had_digit)
		*write++ = '0';

	return write - buffer;
}

u32 itoa_into(char * buffer, i64 number) {
	char * write = buffer;
	u64 abs = number;
	if(number < 0)
	{
		*write++ = '-';
		abs = -number;
	}

	write += utoa_into(write, abs);
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

	if(key == PK_PAGE_UP) {
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


static struct DrawBuffer g_imBuf = DB_INIT(struct Vertex, GL_TRIANGLES);
static struct DrawBuffer g_imBuf_lines = DB_INIT(struct Vertex, GL_LINES);

static bool is_animating = false;

static i64 g_max_x_render = 0;
static i64 g_min_y_render = 0;

static u8 _memory[1024];
static enum DataType _layout[1024];
static struct Memory g_memory = { _memory, sizeof(_memory) };

EXPORT bool on_frame(u16 w, u16 h, u32 dt)
{
	rui_begin_frame(w, h);
	//before_frame(w,h, dt);

	(void) g_imBuf;

	last_screen_h = h;

	if(!is_animating) dt = 0;

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

	static char buffer[32];

	db_string(String("CPU Simulator"), 8-g_x_scroll.offset, h-(8-g_y_scroll.offset), 0xff000000, 0xffcccccc);

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
		auto str = String("Hello, world!\r\nTest 1 2 3");
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

	enum { CELL_W = 36, CELL_H = 20, CACHE_LINE_BYTES = 32 };

	for(unsigned i = 0; i*CACHE_LINE_BYTES < g_memory.size; i++)
	{
		signed xp = 8;
		signed yp = CELL_H * i;

		xp -= g_x_scroll.offset;
		yp -= g_y_scroll.offset;

		unsigned len = utoa_into(buffer, i*CACHE_LINE_BYTES);

		db_text(buffer, len, xp+8*(6-len), h-64-yp, 0xff000000, 0xffcccccc);
	}

	for(unsigned i = 0; i < g_memory.size;)
	{
		unsigned x = i % CACHE_LINE_BYTES;
		unsigned y = i / CACHE_LINE_BYTES;

		signed xp = CELL_W * x + 8 + 64;
		signed yp = CELL_H * y;

		xp -= g_x_scroll.offset;
		yp -= g_y_scroll.offset;

		void const * cell = &g_memory.data[i];

		unsigned len;
		auto type = _layout[i];
		unsigned cells = k_data_types[type].size;
		//unsigned maxlen = 3 * cells;
		switch(type)
		{
		case DT_ANY: {
			u8 c = *(u8 const*)cell;
			len = 2;
			buffer[0] = "0123456789abcdef"[c >> 4];
			buffer[1] = "0123456789abcdef"[c & 15];
		} break;
		case DT_U1: len = utoa_into(buffer, *(u8 const*)cell); break;
		case DT_U2: len = utoa_into(buffer, *(u16 const*)cell); break;
		case DT_U4: len = utoa_into(buffer, *(u32 const*)cell); break;
		case DT_U8: len = utoa_into(buffer, *(u64 const*)cell); break;
		case DT_S1: len = itoa_into(buffer, *(i8 const*)cell); break;
		case DT_S2: len = itoa_into(buffer, *(i16 const*)cell); break;
		case DT_S4: len = itoa_into(buffer, *(i32 const*)cell); break;
		case DT_S8: len = itoa_into(buffer, *(i64 const*)cell); break;
		case DT_BOOL:
			{
				u8 val = *(u8 const*)cell;
				if(val == 0)
					len = memcpy_string(buffer, String("no"));
				else if(val == 1)
					len = memcpy_string(buffer, String("yes"));
				else
					len = utoa_into(buffer, val);
			} break;
		case DT_CHAR: {
			len = 1;
			u8 c = *(u8 const*)cell;
			switch(c)
			{
			case 0: { len = memcpy_string(buffer, String("\\0")); } break;
			case '\n': { len = memcpy_string(buffer, String("\\n")); } break;
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

		db_rectangle(&g_imBuf_lines, xp-2, h - 64+2 - yp, CELL_W * cells, 20, 0xff444444);
		db_quad(&g_imBuf,            xp-2, h - 64+2 - yp, CELL_W * cells, 20, k_bg_colour[type].bg);
		db_text(buffer, len,         xp+CELL_W*cells - 4 - 8*len, h - 64 - yp, k_bg_colour[type].font, k_bg_colour[type].bg);

		i += cells;
	}
	rgl_use_pipeline(pipeline);
	db_flush_and_draw(&g_imBuf, vbo);
	db_flush_and_draw_from(&g_imBuf_lines, vbo_lines, vao_lines);
	render_text(w, h);

	db_quad(&g_imBuf, 8+64 - g_x_scroll.offset, h-64+24 + g_y_scroll.offset, CELL_W*CACHE_LINE_BYTES, 20, 0xffdddddd);
	db_quad(&g_imBuf, 8+64 + CELL_W * CACHE_LINE_BYTES - g_x_scroll.offset, h-64+24 + g_y_scroll.offset, CELL_W*7, 20, 0xffaaaaaa);
	rgl_use_pipeline(pipeline);
	db_flush_and_draw(&g_imBuf, vbo);

	for(unsigned i = 0; i < CACHE_LINE_BYTES + 7; i++)
	{
		signed xp = CELL_W * i + 8 + 64;
		signed yp = CELL_H * -1;

		xp -= g_x_scroll.offset;
		yp -= g_y_scroll.offset;

		unsigned len = utoa_into(buffer, i);

		db_text(buffer, len, xp+8*(4-len), h-64-yp, 0xff000000, i >= CACHE_LINE_BYTES ? 0xffaaaaaa : 0xffdddddd);
	}
	render_text(w, h);

	glFinish();

	//after_frame();

	g_time += dt;

	is_animating = _is_scrolling;

	rui_end_frame();
	return !is_animating;
}