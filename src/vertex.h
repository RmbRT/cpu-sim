#include <rgl_shader_util.h>
#include <imbuf.h>
#include "shader.h"

RGL_MAKE_VERTEX2(Vertex, Shader,
	(RGL_VXT_I16S2, vert_position),
	(RGL_VXT_U8N4_U32, vert_colour)
);


struct Quad {
	struct Vertex v0, v1, v2, v3, v4, v5;
};

// for triangles / rects
static inline void centered_quad(struct Vertex * array, float cx, float cy, float w, float h, u32 colour)
{
	float hw = w/2;
	float hh = h/2;

	i16 l = (cx - hw) * 4,  r = (cx + hw) * 4;
	i16 t = (cy + hh) * 4,  b = (cy - hh) * 4;
	*(struct Quad *) array = (struct Quad) {
		{ { .x = l, .y = t }, colour },
		{ { .x = r, .y = t }, colour },
		{ { .x = l, .y = b }, colour },

		{ { .x = l, .y = b }, colour },
		{ { .x = r, .y = t }, colour },
		{ { .x = r, .y = b }, colour }
	};
}

static inline void db_centered_quad(
	struct DrawBuffer * buffer,
	float cx, float cy,
	float w, float h,
	u32 colour)
{
	centered_quad(DB_APPEND(buffer, struct Vertex, 6), cx, cy, w, h, colour);
}

static inline void quad(struct Vertex * array, float x, float y, float w, float h, u32 colour)
{
	i16 l = (x) * 4,  r = (x + w) * 4;
	i16 t = (y) * 4,  b = (y - h) * 4;
	*(struct Quad *) array = (struct Quad) {
		{ { .x = l, .y = t }, colour },
		{ { .x = r, .y = t }, colour },
		{ { .x = l, .y = b }, colour },

		{ { .x = l, .y = b }, colour },
		{ { .x = r, .y = t }, colour },
		{ { .x = r, .y = b }, colour }
	};
}

static inline void db_quad(
	struct DrawBuffer * buffer,
	float x, float y,
	float w, float h,
	u32 colour)
{
	quad(DB_APPEND(buffer, struct Vertex, 6), x, y, w, h, colour);
}

// for lines.
static inline void rectangle(struct Vertex * array, float x, float y, float w, float h, u32 colour)
{
	i16 l = 2+(x) * 4,  r = 2+(x + w) * 4;
	i16 t = 2+(y) * 4,  b = 2+(y - h) * 4;
	*array++ = (struct Vertex) { { .x = l, .y = t }, colour };
	*array++ = (struct Vertex) { { .x = r, .y = t }, colour };

	*array++ = (struct Vertex) { { .x = r, .y = t }, colour };
	*array++ = (struct Vertex) { { .x = r, .y = b }, colour };

	*array++ = (struct Vertex) { { .x = r, .y = b }, colour };
	*array++ = (struct Vertex) { { .x = l, .y = b }, colour };

	*array++ = (struct Vertex) { { .x = l, .y = b }, colour };
	*array++ = (struct Vertex) { { .x = l, .y = t }, colour };
}

static inline void db_rectangle(
	struct DrawBuffer * buffer,
	float x, float y,
	float w, float h,
	u32 colour)
{
	rectangle(DB_APPEND(buffer, struct Vertex, 8), x, y, w, h, colour);
}