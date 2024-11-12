/**/
#version 460

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec3 outColor;

layout(binding = 0) uniform sampler2D inColor;
layout(binding = 1) uniform sampler2D texLuminance;

void main()
{
  vec4 color = vec4( texture(inColor, texCoord) );
  
    float avgLuminance = max(texture(texLuminance, vec2(0.5, 0.5)).r, 0.0000001);
  
	float brightness = max(max(color.r, color.g), color.b);
	outColor = brightness > 2.0 * avgLuminance ? color.rgb : vec3(0.0);

}
