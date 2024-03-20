#version 450

#extension GL_GOOGLE_include_directive : require
#include "per_frame_structs.glsl"
#include "input_structures.glsl"

layout(location = 0) in vec3 TexCoords;
layout(location = 0) out vec4 FragColor;

    vec3 sunDirection = vec3(-0.216, 0.941, -0.257);
    float earthRadius = 6371e6;
    float atmosphereRadius = 6471e6;
    vec3 betaR = vec3(5.8e-6, 13.5e-6, 33.1e-6);
    vec3 betaM = vec3(21e-6);
    float Hr = 8000.0;
    float Hm = 1200.0;
    vec3 sunIntensity = vec3(2000000.0);

const float PI = 3.141592653589793;

// Calculate Rayleigh phase function
float rayleighPhase(float cosTheta) {
    return 3.0 / (16.0 * PI) * (1.0 + cosTheta * cosTheta);
}

// Calculate Mie phase function
float miePhase(float cosTheta, float g) {
    float g2 = g * g;
    return 3.0 / (8.0 * PI) * ((1.0 - g2) * (1.0 + cosTheta * cosTheta)) / (2.0 + g2) / pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5);
}

// Simplified atmospheric scattering computation
void main() {
    vec3 dir = normalize(TexCoords);
    float cosTheta = dot(dir, normalize(sunDirection));
    vec3 betaRTheta = betaR * rayleighPhase(cosTheta);
    vec3 betaMTheta = betaM * miePhase(cosTheta, 0.76); // Assuming g=0.76

    // Here, we simplify the atmospheric scattering equations for real-time purposes
    // and assume single scattering only for demonstration.
    vec3 L = sunIntensity * (betaRTheta + betaMTheta);
    
    // Apply simple exposure and gamma correction
    vec3 color = vec3(1.0) - exp(-L * 0.004);
    //color = pow(color, vec3(1.0 / 2.2)); // Assuming gamma=2.2

    FragColor = vec4(color, 1.0);
}
