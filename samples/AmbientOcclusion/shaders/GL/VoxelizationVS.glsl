#version 430
layout(row_major) uniform;

layout(std140, binding = 0) uniform GlobalConstants
{
    mat4x4 g_ViewProjMatrix;
};

layout(location = 0) in vec3 a_position;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
    gl_Position = vec4(a_position.xyz, 1.0) * g_ViewProjMatrix;
} 
