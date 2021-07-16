/*
===========================================================================

whynot::shader_compile.c: loads and compiles shaders into SPIR-V

===========================================================================
*/

#include "shader_compile.h"
#include "core_types.h"
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

    wn_shader_compiler_o r = {
        .compiler = c,
    };

    return r;
}

wn_result shutdown(wn_render_shader_compiler_o* compiler)
{
    shaderc_compiler_release(compiler->compiler);
    return WN_OK;
}

wn_result wn_render_shader_compiler_compile(
    wn_render_shader_compiler_o* compiler,
    uint8_t* shader_bytes,
    wn_shader_stage stage,
    char* entry,
    wn_shader_source_t* shader)
{
    log_info("Loading shader: %s...", file_name);
    wn_file_source_t content = wn_util_read_file(file_name);
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
        loader->compiler,
        content.content,
        content.size,
        kind,
        file_name,
        "main",
        NULL);

    const char* shader_err = shaderc_result_get_error_message(result);

    if (!result)
    {
        log_error("Error compiling shader: %s.", file_name);
        exit(EXIT_FAILURE);
    }
    if (shader_err[0] != '\0')
    {
        log_debug("Shader error in compiling %s: %s", file_name, shader_err);
    }

    wn_shader_t shader = {
        .source = content,
        .entry = "main",
        .shader_stage = shader_stage,
        .size = shaderc_result_get_length(result),
        .spirv = (uint32_t*)shaderc_result_get_bytes(result),
    };

    log_info("Loaded shader: %s!", file_name);

    return shader;

}
