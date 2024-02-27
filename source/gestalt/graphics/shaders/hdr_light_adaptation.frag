/**/
#version 460

layout(location = 0) in  vec2 texCoord;
layout(location = 0) out vec4 outColor;

layout( push_constant ) uniform constants
{
    float adaptation_speed;
    float delta_time;
    float min_luminance;
    float max_luminance;
} params;

layout(binding = 10) uniform sampler2D currentLuminance;
layout(binding = 11) uniform sampler2D adaptedLuminance;

void main()
{
   float currentLum = texture(currentLuminance, vec2(0.5, 0.5)).x;
   float adaptedLum = texture(adaptedLuminance, vec2(0.5, 0.5)).x;

   float newAdaptation = adaptedLum + (currentLum - adaptedLum) * (1.0 - exp(-params.delta_time * params.adaptation_speed));

   newAdaptation = min(max(params.min_luminance, newAdaptation), min(params.max_luminance, newAdaptation));

   outColor = vec4(vec3(newAdaptation), 1.0);
}
