/**/
#version 460

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 outColor;

layout(binding = 10) uniform sampler2D texSampler;

vec3 sRGBToLinear(vec3 color) {
    return mix(color / 12.92, pow((color + vec3(0.055)) / vec3(1.055), vec3(2.4)), step(vec3(0.04045), color));
}

vec3 linearTosRGB(vec3 color) {
    return mix(color * 12.92, 1.055 * pow(color, vec3(1.0 / 2.4)) - vec3(0.055), step(vec3(0.0031308), color));
}

void main()
{
  vec4 Color = vec4( texture(texSampler, texCoord) );
  Color.rgb = linearTosRGB(Color.rgb);

  if ( dot( Color, vec4( 0.33, 0.34, 0.33, 0.0) ) < 1.0 ) Color = vec4( 0.0, 0.0, 0.0, 1.0 );

  outColor = vec4(Color.xyz, 1.0);
}
