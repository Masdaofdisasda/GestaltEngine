/**/
#version 460

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec3 outColor;

layout(binding = 10) uniform sampler2D texSampler;

void main()
{
  vec4 Color = vec4( texture(texSampler, texCoord) );

  outColor = Color.rgb;
}
