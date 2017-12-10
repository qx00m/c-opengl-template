
#include "shared.h"

#define API_EXPORT __declspec(dllexport)

#define X(ret, name, ...)	\
API_EXPORT ret (*name)(__VA_ARGS__) = 0;

OPENGL_FUNCTIONS
SYSTEM_FUNCTIONS
#undef X

////////

struct vec2 { f32 x, y; };
struct vec3 { f32 x, y, z; };

struct vec4
{
	union { f32 x, r; };
	union { f32 y, g; };
	union { f32 z, b; };
	union { f32 w, a; };
};

struct rect2d { f32 x0, y0, x1, y1; };

struct vertex
{
	struct vec3 position;
	struct vec2 texcoord;
	struct vec4 color;
};

struct vertex_buffer
{
	i32 max;
	i32 used;
	struct vertex *vertices;
};

struct app_state
{
	u32 vao;
	u32 vbo;

	u32 basic_program;
	i32 basic_uproj;

	u32 texture_program;
	i32 texture_uproj;
	i32 texture_umap;

	u32 atlas;
	i32 atlas_width;
	i32 atlas_height;
	u32 atlas_is_dirty;

	i32 atlas_ymax;
	i32 atlas_x;
	i32 atlas_y;

	u32 *atlas_bits;

	struct vertex_buffer vertex_buffer;

	struct font *console_font;
	struct font *ui_font;
};

////////

internal inline void
opengl_attach_shader(u32 program, const char *src, u32 type)
{
	u32 shader = glCreateShader(type);
	glShaderSource(shader, 1, &src, 0);
	glCompileShader(shader);

	i32 status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (!status) {
		assert(status);
	}

	glAttachShader(program, shader);
	glDeleteShader(shader);
}

internal u32
opengl_program(const char *vs_src, const char *fs_src)
{
	u32 program = glCreateProgram();

	opengl_attach_shader(program, vs_src, GL_VERTEX_SHADER);
	opengl_attach_shader(program, fs_src, GL_FRAGMENT_SHADER);

	glLinkProgram(program);

	i32 status;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (!status) {
		assert(status);
	}

	return program;
}

internal struct glyph *
render_glyph(struct app_state *state, struct font *font, u32 codepoint)
{
	struct glyph *g = font->glyphs;
	{
		struct glyph *end = g + font->glyphs_used;
		while (g != end) {
			if (g->codepoint == codepoint)
				return g;
			++g;
		}
	}

	assert(font->glyphs_used != font->glyphs_max);
	++font->glyphs_used;

	g->codepoint = codepoint;
	g->xadv = sys_render_glyph(font, codepoint);

	i32 xmin = state->atlas_width;
	i32 ymin = state->atlas_height;
	i32 xmax = 0;
	i32 ymax = 0;

	u32 *p = font->bits;
	for (i32 y = 0; y < font->bitmap_height; ++y) {
		for (i32 x = 0; x < font->bitmap_width; ++x) {
			if (*p) {
				if (x < xmin) xmin = x;
				if (x > xmax) xmax = x;
				if (y < ymin) ymin = y;
				if (y > ymax) ymax = y;
			}
			++p;
		}
	}

	if (xmin <= xmax) {
		g->dx = xmin - font->default_x;
		g->dy = ymin - font->default_y;

		i32 w = xmax - xmin + 1;
		i32 h = ymax - ymin + 1;

		if (state->atlas_x + w > state->atlas_width) {
			state->atlas_x = 0;
			state->atlas_y += state->atlas_ymax;
			state->atlas_ymax = 0;
		}

		if (h > state->atlas_ymax)
			state->atlas_ymax = h;

		u32 *srcrow = font->bits + ymin * font->bitmap_width + xmin;
		u32 *dstrow = state->atlas_bits + state->atlas_y * state->atlas_width + state->atlas_x;

		for (i32 y = 0; y < h; ++y) {
			u32 *src = srcrow;
			u32 *dst = dstrow;
			for (i32 x = 0; x < w; ++x) {
				if (*src) {
					u32 c = *src & 0xFF;
					*dst = (c << 24) | (c << 16) | (c << 8) | c;
				}
				++src;
				++dst;
			}
			srcrow += font->bitmap_width;
			dstrow += state->atlas_width;
		}

		g->x0 = state->atlas_x;
		g->y0 = state->atlas_y;
		g->x1 = g->x0 + w;
		g->y1 = g->y0 + h;

		state->atlas_x += w;
		state->atlas_is_dirty = true;
	}
	else {
		g->dx = 0;
		g->dy = 0;
		g->x0 = 0;
		g->y0 = 0;
		g->x1 = 0;
		g->y1 = 0;
	}

	return g;
}

internal inline void
upload_vertices(struct app_state *state)
{
	glBindBuffer(GL_ARRAY_BUFFER, state->vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(struct vertex) * state->vertex_buffer.used, state->vertex_buffer.vertices, GL_STREAM_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

internal void
mesh_rect2d(struct vertex_buffer *buffer, f32 x0, f32 y0, f32 x1, f32 y1, f32 z, f32 u0, f32 v0, f32 u1, f32 v1, struct vec4 color)
{
	assert(buffer->used + 6 <= buffer->max);

	struct vertex *v = buffer->vertices + buffer->used;
	buffer->used += 6;

	v[0] = (struct vertex){ { x0, y0, z }, { u0, v0 }, color };
	v[1] = (struct vertex){ { x1, y0, z }, { u1, v0 }, color };
	v[2] = (struct vertex){ { x0, y1, z }, { u0, v1 }, color };

	v[3] = (struct vertex){ { x1, y0, z }, { u1, v0 }, color };
	v[4] = (struct vertex){ { x1, y1, z }, { u1, v1 }, color };
	v[5] = (struct vertex){ { x0, y1, z }, { u0, v1 }, color };
}

internal void
render_ascii(struct app_state *state, struct font *font, const char *s, f32 x, f32 y, f32 z, struct vec4 color)
{
	f32 w = (f32)state->atlas_width;
	f32 h = (f32)state->atlas_height;

	state->vertex_buffer.used = 0;

	x = (f32)(i32)(x + 0.5f);
	y = (f32)(i32)(y + 0.5f);

	while (*s) {
		u32 codepoint = (u32)*s;

		struct glyph *glyph = render_glyph(state, font, codepoint);
		if (glyph) {
			f32 x0 = (f32)(x + glyph->dx);
			f32 y0 = (f32)(y + glyph->dy);
			f32 x1 = x0 + (glyph->x1 - glyph->x0);
			f32 y1 = y0 + (glyph->y1 - glyph->y0);

			f32 u0 = glyph->x0 / w;
			f32 v0 = glyph->y0 / h;
			f32 u1 = glyph->x1 / w;
			f32 v1 = glyph->y1 / h;

			mesh_rect2d(&state->vertex_buffer, x0, y0, x1, y1, z, u0, v0, u1, v1, color);

			x += glyph->xadv;
		}

		++s;
	}

	if (state->vertex_buffer.used == 0)
		return;

	if (state->atlas_is_dirty) {
		glBindTexture(GL_TEXTURE_2D, state->atlas);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, state->atlas_width, state->atlas_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, state->atlas_bits);
		glBindTexture(GL_TEXTURE_2D, 0);
		state->atlas_is_dirty = false;
	}

	upload_vertices(state);

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glBindTexture(GL_TEXTURE_2D, state->atlas);
	glUseProgram(state->texture_program);
	glDrawArrays(GL_TRIANGLES, 0, state->vertex_buffer.used);
	glUseProgram(0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_BLEND);
}

internal inline void
render_rect2d(struct app_state *state, struct rect2d position, f32 z, struct vec4 color)
{
	state->vertex_buffer.used = 0;
	mesh_rect2d(&state->vertex_buffer, position.x0, position.y0, position.x1, position.y1, z, 0.f, 0.f, 0.f, 0.f, color);

	upload_vertices(state);

	glUseProgram(state->basic_program);
	glDrawArrays(GL_TRIANGLES, 0, state->vertex_buffer.used);
	glUseProgram(0);
}

API_EXPORT void *
reload(void *userdata)
{
	if (userdata)
		return userdata;

	struct app_state *state = sys_allocate(sizeof(struct app_state), _Alignof(struct app_state));

	glGenVertexArrays(1, &state->vao);
	glBindVertexArray(state->vao);

	glGenBuffers(1, &state->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, state->vbo);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(struct vertex), (void *)offsetof(struct vertex, position));
	glVertexAttribPointer(1, 2, GL_FLOAT, false, sizeof(struct vertex), (void *)offsetof(struct vertex, texcoord));
	glVertexAttribPointer(2, 4, GL_FLOAT, false, sizeof(struct vertex), (void *)offsetof(struct vertex, color));

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	const char *basic_vs_src = "#version 330\n"
		"\n"
		"layout(location = 0) in vec3 vs_position;\n"
		"layout(location = 1) in vec2 vs_texcoord;\n"
		"layout(location = 2) in vec4 vs_color;\n"
		"\n"
		"out vec4 fs_color;\n"
		"\n"
		"uniform mat4 proj;\n"
		"\n"
		"void main(void)\n"
		"{\n"
		"	fs_color = vs_color;\n"
		"	gl_Position = proj * vec4(vs_position, 1);\n"
		"}\n";

	const char *basic_fs_src = "#version 330\n"
		"\n"
		"in vec4 fs_color;\n"
		"\n"
		"out vec4 frag_color;\n"
		"\n"
		"void main(void)\n"
		"{\n"
		"	frag_color = fs_color;\n"
		"}\n";
	state->basic_program = opengl_program(basic_vs_src, basic_fs_src);
	state->basic_uproj = glGetUniformLocation(state->basic_program, "proj");
	assert(state->basic_uproj != -1);

	const char *texture_vs_src = "#version 330\n"
		"\n"
		"layout(location = 0) in vec3 vs_position;\n"
		"layout(location = 1) in vec2 vs_texcoord;\n"
		"layout(location = 2) in vec4 vs_color;\n"
		"\n"
		"out vec2 fs_texcoord;\n"
		"out vec4 fs_color;\n"
		"\n"
		"uniform mat4 proj;\n"
		"\n"
		"void main(void)\n"
		"{\n"
		"	fs_texcoord = vs_texcoord;\n"
		"	fs_color = vs_color;\n"
		"	gl_Position = proj * vec4(vs_position, 1);\n"
		"}\n";

	const char *texture_fs_src = "#version 330\n"
		"\n"
		"in vec2 fs_texcoord;\n"
		"in vec4 fs_color;\n"
		"\n"
		"out vec4 frag_color;\n"
		"\n"
		"uniform sampler2D texture_map;\n"
		"\n"
		"void main(void)\n"
		"{\n"
		"	frag_color = texture(texture_map, fs_texcoord) * fs_color;\n"
		"}\n";
	state->texture_program = opengl_program(texture_vs_src, texture_fs_src);
	state->texture_uproj = glGetUniformLocation(state->texture_program, "proj");
	assert(state->texture_uproj != -1);
	state->texture_umap = glGetUniformLocation(state->texture_program, "texture_map");
	assert(state->texture_umap != -1);

	state->atlas_width = 512;
	state->atlas_height = 512;
	state->atlas_is_dirty = false;
	state->atlas_bits = allocate(u32, state->atlas_width * state->atlas_height);
	state->atlas_ymax = 0;
	state->atlas_x = 0;
	state->atlas_y = 0;

	glGenTextures(1, &state->atlas);
	glBindTexture(GL_TEXTURE_2D, state->atlas);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, state->atlas_width, state->atlas_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, state->atlas_bits);
	glBindTexture(GL_TEXTURE_2D, 0);

	glEnable(GL_FRAMEBUFFER_SRGB);

	state->vertex_buffer.max = 1024 * 64;
	state->vertex_buffer.used = 0;
	state->vertex_buffer.vertices = allocate(struct vertex, state->vertex_buffer.max);

	// load assets
	state->console_font = sys_create_font(L"Courier New", 10);
	state->ui_font = sys_create_font(L"Verdana", 8);

	for (u32 codepoint = ' '; codepoint < 127; ++codepoint) {
		render_glyph(state, state->console_font, codepoint);
		render_glyph(state, state->ui_font, codepoint);
	}

	return state;
}

API_EXPORT void
render(void *userdata, i32 window_width, i32 window_height)
{
	struct app_state *state = userdata;

	glViewport(0, 0, window_width, window_height);

	f32 sx = 2.f / window_width;
	f32 sy = 2.f / window_height;
	f32 proj[16] = {
		  sx,  0.f, 0.f, 0.f,
		 0.f,   sy, 0.f, 0.f,
		 0.f,  0.f, 1.f, 0.f,
		-1.f, -1.f, 0.f, 1.f,
	};
	glUseProgram(state->basic_program);
	glUniformMatrix4fv(state->basic_uproj, 1, false, proj);
	glUseProgram(state->texture_program);
	glUniformMatrix4fv(state->texture_uproj, 1, false, proj);
	glUseProgram(0);

	glClearColor(0.02f, 0.02f, 0.02f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT);

	////////

	struct vec4 white_color = { 1.f, 1.f, 1.f, 1.f };
	struct vec4 red_color = { 1.f, 0.f, 0.f, 1.f };

	const char *s = "The quick brown fox jumps over the lazy dog.";
	render_ascii(state, state->console_font, s, 100.f, 80.f, 0.f, white_color);
	render_ascii(state, state->ui_font, s, 100.f, 100.f, 0.f, white_color);

	struct rect2d bounds = { 0.f, (f32)window_height - 32.f, (f32)window_width, (f32)window_height };
	render_rect2d(state, bounds, 0.f, red_color);
}

int _fltused = 0;
