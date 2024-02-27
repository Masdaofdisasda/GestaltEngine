/**/
#version 460

layout(location = 0) in  vec2 texCoord;
layout(location = 0) out vec4 outColor;

layout( push_constant ) uniform constants
{
    float adaptation_speed_dark2light;
    float adaptation_speed_light2dark;
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

   float tau = (currentLum > adaptedLum) ? params.adaptation_speed_light2dark : params.adaptation_speed_dark2light;


   //https://google.github.io/filament/Filament.html#imagingpipeline/physicallybasedcamera/adaptation
   float newAdaptation = adaptedLum + (currentLum - adaptedLum) * (1.0 - exp(-params.delta_time * tau));
   
   newAdaptation = clamp(newAdaptation, params.min_luminance, params.max_luminance);

   outColor = vec4(vec3(newAdaptation), 1.0);
}
