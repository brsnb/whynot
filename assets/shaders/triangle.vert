#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform _mvp {
    mat4 model;
    mat4 view;
    mat4 proj;
} mvp;

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec2 in_tex_coord0;

layout(location = 0) out vec2 out_tex_coord0;

void main() {
    gl_Position = mvp.proj * mvp.view * mvp.model * vec4(in_pos, 1.0);
    out_tex_coord0 = in_tex_coord0;
}
