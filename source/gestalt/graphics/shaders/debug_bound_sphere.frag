#version 450

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vCenter;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    vec3 center;
    float radius;
} bounds;

void main()
{
    float distanceFromCenter = length(vPosition.xy - vCenter.xy);
    if (distanceFromCenter > bounds.radius) {
        discard; // Discard fragments outside the radius
    } else {
        outColor = vec4(0.0, 1.0, 0.0, 0.3);
    }

}

