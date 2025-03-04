/**/
#version 460

layout(location = 0) in  vec2 texCoord;
layout(location = 0) out float outColor;

layout( push_constant ) uniform constants
{
    float adaptation_speed_dark2light;
    float adaptation_speed_light2dark;
    float delta_time;
    float min_luminance;
    float max_luminance;
} params;

layout(set = 0, binding = 0) uniform sampler2D currentLuminance;
layout(set = 0, binding = 1) uniform sampler2D adaptedLuminance;

float sigmoid(float x) {
    return 1.0 / (1.0 + exp(-x));
}

void main()
{
   float currentLum = texture(currentLuminance, vec2(0.5, 0.5)).x;
   float adaptedLum = texture(adaptedLuminance, vec2(0.5, 0.5)).x;

   float adaptationTime = (currentLum < adaptedLum) ? params.adaptation_speed_light2dark : params.adaptation_speed_dark2light;
    float tau = 1.0 / adaptationTime;


   //https://google.github.io/filament/Filament.html#imagingpipeline/physicallybasedcamera/adaptation
   float newAdaptation = adaptedLum + (currentLum - adaptedLum) * (1.0 - exp(-params.delta_time * tau));
   
   newAdaptation = clamp(newAdaptation, params.min_luminance, params.max_luminance);

   outColor = newAdaptation;
}

