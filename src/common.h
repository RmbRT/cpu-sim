#ifndef __common_h_defined
#define __common_h_defined

#include <rgl.h>
#include <rmbrt-ui/textfield.h>

enum Interpret {
	INTERPRET,
	NO_INTERPRET_HEX,
	NO_INTERPRET_DEC
};

extern RGL_HANDLE(RGLPipelineState) pipeline;
extern RGL_HANDLE(RGLBuffer) vbo, vbo_lines;
extern RGL_HANDLE(RGLVertexFetch) vao, vao_lines;
extern struct DrawBuffer g_imBuf, g_imBuf_lines;

extern struct Memory g_memory;
extern enum Interpret g_interpret;

extern struct RUINumberTextField g_reg_x_field;



#endif