#version 430
layout(row_major) uniform;

layout(binding = 0) uniform sampler2D SourceTexture;

in vec4 gl_FragCoord;
out vec4 f_color;

void main()
{
    f_color = texelFetch(SourceTexture, ivec2(gl_FragCoord.xy), 0);
}
