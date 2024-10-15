
float PCF(int kernelSize, vec2 shadowCoord, float depth, sampler2D shadowMap)
{
	float size = 1.0 / float( textureSize(shadowMap, 0 ).x );
	float shadow = 0.0;
	int range = kernelSize / 2;
	for ( int v=-range; v<=range; v++ ) for ( int u=-range; u<=range; u++ )
		shadow += (depth < texture( shadowMap, shadowCoord + size * vec2(u, v) ).r) ? 1.0 : 0.0;
	return shadow / (kernelSize * kernelSize);
}

vec2 poissonDisk[16] = vec2[](
    vec2(-0.94201624, -0.39906216),
    vec2( 0.94558609, -0.76890725),
    vec2(-0.09418410, -0.92938870),
    vec2( 0.34495938,  0.29387760),
    vec2(-0.91588581,  0.45771432),
    vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543,  0.27676845),
    vec2( 0.97484398,  0.75648379),
    vec2( 0.44323325, -0.97511554),
    vec2( 0.53742981, -0.47373420),
    vec2( 0.60752861,  0.25711949),
    vec2( 0.80085912,  0.67452367),
    vec2(-0.12044660,  0.97802539),
    vec2(-0.95612167, -0.28248548),
    vec2(-0.75623487, -0.48246617),
    vec2(-0.50596235,  0.82061111)
);


float PCF_Poisson(int kernelSize, vec2 shadowCoord, float depth, sampler2D shadowMap) {
    float size = 1.0 / float(textureSize(shadowMap, 0).x);
    float shadow = 0.0;
    for (int i = 0; i < kernelSize; ++i) {
        vec2 offset = poissonDisk[i] * size;
        shadow += (depth < texture(shadowMap, shadowCoord + offset).r) ? 1.0 : 0.0;
    }
    return shadow / float(kernelSize);
}

float shadowFactor(vec4 shadowCoord, sampler2D shadowMap, float depthBias)
{
	vec3 shadowCoords = shadowCoord.xyz / shadowCoord.w;

	float shadowSample = PCF_Poisson( 13, shadowCoords.xy, shadowCoords.z - depthBias, shadowMap);

	return mix(0.00001, 1.0, shadowSample);
}

float calculateDynamicBias(vec3 normal, vec3 lightDir) {
    return max(0.01 * (1.0 - dot(normal, -lightDir)), 0.005);  
}