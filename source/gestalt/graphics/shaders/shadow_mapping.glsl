
float PCF(int kernelSize, vec2 shadowCoord, float depth, sampler2D shadowMap)
{
	float size = 1.0 / float( textureSize(shadowMap, 0 ).x );
	float shadow = 0.0;
	int range = kernelSize / 2;
	for ( int v=-range; v<=range; v++ ) for ( int u=-range; u<=range; u++ )
		shadow += (depth < texture( shadowMap, shadowCoord + size * vec2(u, v) ).r) ? 1.0 : 0.0;
	return shadow / (kernelSize * kernelSize);
}

float shadowFactor(vec4 shadowCoord, sampler2D shadowMap, float depthBias)
{
	vec3 shadowCoords = shadowCoord.xyz / shadowCoord.w;

	float shadowSample = PCF( 13, shadowCoords.xy, shadowCoords.z - depthBias, shadowMap);

	return mix(0.00001, 1.0, shadowSample);
}

float calculateDynamicBias(vec3 normal, vec3 lightDir) {
    return max(0.01 * (1.0 - dot(normal, -lightDir)), 0.005);  
}