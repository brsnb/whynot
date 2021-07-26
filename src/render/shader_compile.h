/*
===========================================================================

whynot::shader_compile.h: loads and compiles shaders into SPIR-V

===========================================================================
*/

#pragma once

#include "render_types.h"

// types + forward decls
typedef struct wn_shader_source_t
{
    const uint8_t* source;
    size_t source_size;
    uint8_t* spirv;
    size_t spirv_size;
    wn_shader_stage stage;
    const char* entry;
} wn_shader_source_t;

typedef struct wn_render_shader_compiler_o wn_render_shader_compiler_o;

// api
wn_render_shader_compiler_o wn_render_shader_compiler_init(void);

wn_result wn_render_shader_compiler_shutdown(wn_render_shader_compiler_o* compiler);

wn_result wn_render_shader_compiler_compile(
    const wn_render_shader_compiler_o* compiler,
    const char* filename,
    wn_shader_stage stage,
    const char* entry,
    wn_shader_source_t* shader);
