
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define assert(x)		\
do {				\
	if (!(x)) {		\
		__debugbreak();	\
	}			\
} while (0)

#define internal static
#define unused(x) ((void)(x))

#define allocate(type, n) sys_allocate((n) * sizeof(type), _Alignof(type))

typedef int16_t i16;
typedef int32_t i32;
typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float f32;

#define GL_TRIANGLES            0x0004
#define GL_COLOR_BUFFER_BIT	0x00004000
#define GL_FLOAT                0x1406
#define GL_ARRAY_BUFFER		0x8892
#define GL_FRAGMENT_SHADER      0x8B30
#define GL_VERTEX_SHADER        0x8B31
#define GL_FRAMEBUFFER_SRGB	0x8DB9
#define GL_COMPILE_STATUS       0x8B81
#define GL_LINK_STATUS          0x8B82
#define GL_STREAM_DRAW          0x88E0
#define GL_TEXTURE_2D           0x0DE1
#define GL_TEXTURE_MAG_FILTER   0x2800
#define GL_TEXTURE_MIN_FILTER   0x2801
#define GL_TEXTURE_WRAP_S       0x2802
#define GL_TEXTURE_WRAP_T       0x2803
#define GL_CLAMP_TO_EDGE        0x812F
#define GL_NEAREST              0x2600
#define GL_SRGB8_ALPHA8         0x8C43
#define GL_RGBA                 0x1908
#define GL_UNSIGNED_BYTE        0x1401
#define GL_BLEND                0x0BE2
#define GL_ONE_MINUS_SRC_ALPHA  0x0303
#define GL_ONE                  1

#define BUTTON_LEFT	0x01
#define BUTTON_RIGHT 	0x02

struct glyph
{
	u32 codepoint;

	i32 dx;
	i32 dy;

	i32 xadv;

	i32 x0, y0, x1, y1;
};

struct font
{
	void *sys;

	i32 default_x;
	i32 default_y;

	i32 bitmap_width;
	i32 bitmap_height;
	u32 *bits;

	i32 ascent;
	i32 descent;
	i32 height;
	i32 external_leading;

	i32 glyphs_max;
	i32 glyphs_used;
	struct glyph *glyphs;
};

#define SYSTEM_FUNCTIONS	\
	X(void *, sys_allocate, size_t n, size_t alignment)	\
	X(void, sys_deallocate, void *p, size_t n, size_t alignment)	\
	X(struct font *, sys_create_font, const wchar_t *name, i32 pixel_height)	\
	X(i32, sys_render_glyph, struct font *font, u32 codepoint)	\
	/* end */

#define CODE_FUNCTIONS	\
	X(void *, reload, void *userdata)	\
	X(void, render, void *userdata, i32 window_width, i32 window_height)	\
	X(void, mouse, void *userdata, i32 x, i32 y, i32 dz, u32 buttons)	\
	X(void, keyboard, void *userdata, u32 codepoint)	\
	/* end */

#define OPENGL_FUNCTIONS	\
	X(void, glEnable, u32 cap)	\
	X(void, glDisable, u32 cap)	\
	X(void, glGenVertexArrays, i32 n, u32 *arrays)	\
	X(void, glBindVertexArray, u32 array)	\
	X(void, glGenBuffers, i32 n, u32 *buffers)	\
	X(void, glBindBuffer, u32 target, u32 buffer)	\
	X(void, glBufferData, u32 target, ptrdiff_t size, const void *data, u32 usage)	\
	X(u32, glCreateProgram, void)	\
	X(u32, glCreateShader, u32 shaderType)	\
	X(void, glAttachShader, u32 program, u32 shader)	\
	X(void, glDeleteShader, u32 shader)	\
	X(void, glShaderSource, u32 shader, i32 count, const char *const* string, const i32 *length)	\
	X(void, glCompileShader, u32 shader)	\
	X(void, glGetProgramiv, u32 program, u32 pname, i32 *params)	\
	X(void, glGetShaderiv, u32 shader, u32 pname, i32 *params)	\
	X(void, glClear, u32 mask)	\
	X(void, glClearColor, f32 red, f32 green, f32 blue, f32 alpha)	\
	X(void, glViewport, i32 x, i32 y, i32 width, i32 height)	\
	X(void, glVertexAttribPointer, u32 index, i32 size, u32 type, u8 normalized, i32 stride, const void *pointer)	\
	X(void, glEnableVertexAttribArray, u32 index)	\
	X(void, glLinkProgram, u32 program)	\
	X(void, glDrawArrays, u32 mode, i32 first, i32 count)	\
	X(void, glUseProgram, u32 program)	\
	X(i32, glGetUniformLocation, u32 program, const char *name)	\
	X(void, glUniformMatrix4fv, i32 location, i32 count, u8 transpose, const f32 *value)	\
	X(void, glGenTextures, i32 n, u32 *textures)	\
	X(void, glTexParameteri, u32 target, u32 pname, i32 param)	\
	X(void, glBindTexture, u32 target, u32 texture)	\
	X(void, glTexImage2D, u32 target, i32 level, i32 internalFormat, i32 width, i32 height, i32 border, u32 format, u32 type, const void *data)	\
	X(void, glBlendFunc, u32 sfactor, u32 dfactor)	\
	X(void, glGetProgramInfoLog, u32 program, i32 maxLength, i32 *length, char *infoLog)	\
	X(void, glGetShaderInfoLog, u32 shader, i32 maxLength, i32 *length, char *infoLog)	\
	/* end */

internal inline i32
max_i32(i32 a, i32 b)
{
	return a > b ? a : b;
}

internal inline char *
copy_string(char *b, char *e, const char *s)
{
	if (b == e)
		return b;

	while (b + 1 != e && *s)
		*b++ = *s++;

	*b = 0;
	return b;
}

internal char *
to_string_u64(char *b, char *e, u64 num, u32 base, i32 minwidth, char fillchar)
{
	if (b == e)
		return b;

	char digits[64];
	char *p = digits;

	do
		*p++ = "0123456789ABCDEF"[num % base];
	while (num /= base);

	i32 n = max_i32(0, minwidth - (i32)(p - digits));
	while (b + 1 != e && n--)
		*b++ = fillchar;

	while (b + 1 != e && p != digits)
		*b++ = *--p;

	*b = 0;
	return b;
}
