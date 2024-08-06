#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_16bit_storage : require
#include "per_frame_structs.glsl"

layout(location = 0) in vec3 TexCoords;
layout(location = 0) out vec4 FragColor;

layout(set = 1, binding = 1) buffer DirLight{
	vec3 color;
	float intensity;
	vec3 direction;
	int viewProjIndex;
} dirLight[2];

layout( push_constant ) uniform constants
{
    //float earthRadius;
    //float atmosphereRadius;
    vec3 betaR; float pad1;
    vec3 betaA; float pad2;
    vec3 betaM; float pad3;
    //float Hr;
    //float Hm;
} params;

// based on https://www.scratchapixel.com/lessons/procedural-generation-virtual-worlds/simulating-sky/simulating-colors-of-the-sky.html

    vec3 sunDirection = vec3(-0.216, 0.941, -0.257);
    vec3 sunIntensity = vec3(2000000.0);

const float PI = 3.141592653589793;

float rayleighPhase(float cosTheta) {
    return 3.0 / (16.0 * PI) * (1.0 + cosTheta * cosTheta);
}

float miePhase(float cosTheta, float g) {
    float g2 = g * g;
    return 3.0 / (8.0 * PI) * ((1.0 - g2) * (1.0 + cosTheta * cosTheta)) / (2.0 + g2) / pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5);
}

// Simplified atmospheric scattering computation
void main() {

    vec3 dir = normalize(TexCoords);
    float cosTheta = dot(dir, normalize(dirLight[0].direction));
    vec3 betaRTheta = params.betaR * rayleighPhase(cosTheta);
    vec3 betaMTheta = params.betaM * miePhase(cosTheta, 0.76); // Assuming g=0.76

    // Here, we simplify the atmospheric scattering equations for real-time purposes
    float intensity = dirLight[0].intensity * 1e6;
    vec3 L = dirLight[0].color * intensity * (betaRTheta + betaMTheta);
    
    vec3 L_multiscatter = L * 0.5; // This is a simplification
 
    L -= params.betaA * 0.002; // This reduces the intensity based on absorption

    L = L + L_multiscatter;

    // Apply simple exposure and gamma correction
    vec3 color = vec3(1.0) - exp(-L * 0.004);

    FragColor = vec4(color, 1.0);
}
