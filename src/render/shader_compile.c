/*
===========================================================================

whynot::shader_compile.c: loads and compiles shaders into SPIR-V

===========================================================================
*/

#include "shader_compile.h"

#include "core_types.h"
#include "file.inl"
#include "util_vk.inl"

#include "log.h"

#include <shaderc/shaderc.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct wn_render_shader_compiler_o
{
    shaderc_compiler_t compiler;
} wn_shader_compiler_o;

wn_shader_compiler_o wn_shader_compiler_init(void)
{
    shaderc_compiler_t c = shaderc_compiler_initialize();

    if (!c)
    {
        log_error("Could not create shaderc compiler");
        exit(EXIT_FAILURE);
    }

    wn_shader_compiler_o r = {
        .compiler = c,
    };

    return r;
}

wn_result wn_render_shader_compiler_shutdown(wn_render_shader_compiler_o* compiler)
{
    shaderc_compiler_release(compiler->compiler);
    return WN_OK;
}

wn_result wn_render_shader_compiler_compile(
    const wn_render_shader_compiler_o* compiler,
    const char* filename,
    wn_shader_stage stage,
    const char* entry,
    wn_shader_source_t* shader)
{
    log_info("Loading shader: %s...", filename);
    wn_file_src_t content = wn_file_read(filename);
    shaderc_shader_kind kind;
    switch (stage)
    {
    case WN_SHADER_STAGE_COMPUTE:
        kind = shaderc_compute_shader;
        break;
    case WN_SHADER_STAGE_FRAGMENT:
        kind = shaderc_fragment_shader;
        break;
    case WN_SHADER_STAGE_VERTEX:
        kind = shaderc_vertex_shader;
        break;
    default:
        log_fatal("Need to specify a shader stage");
        return WN_ERR;
    }

    const shaderc_compilation_result_t result = shaderc_compile_into_spv(
        compiler->compiler,
        content.data,
        content.size,
        kind,
        filename,
        "main",
        NULL);

    const char* shader_err = shaderc_result_get_error_message(result);

    if (!result)
    {
        log_error("Error compiling shader: %s.", filename);
        return WN_ERR;
    }
    if (shader_err[0] != '\0')
    {
        log_debug("Shader error in compiling %s: %s", filename, shader_err);
        return WN_ERR;
    }

    shader->source = content.data,
    shader->source_size = content.size,
    shader->spirv = (uint32_t*)shaderc_result_get_bytes(result),
    shader->spirv_size = shaderc_result_get_length(result),
    shader->entry = "main",
    shader->stage = stage,

    log_info("Loaded shader: %s!", filename);

    return WN_OK;
}
