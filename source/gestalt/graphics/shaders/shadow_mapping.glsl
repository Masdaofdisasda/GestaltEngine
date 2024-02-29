
float PCF(int kernelSize, vec2 shadowCoord, float depth, sampler2D shadowMap)
{
	float size = 1.0 / float( textureSize(shadowMap, 0 ).x );
	float shadow = 0.0;
	int range = kernelSize / 2;
	for ( int v=-range; v<=range; v++ ) for ( int u=-range; u<=range; u++ )
		shadow += (depth < texture( shadowMap, shadowCoord + size * vec2(u, v) ).r) ? 1.0 : 0.0;
	return shadow / (kernelSize * kernelSize);
}

float shadowFactor(vec4 shadowCoord, sampler2D shadowMap)
{
	vec3 shadowCoords = shadowCoord.xyz / shadowCoord.w;

	float depthBias = -0.001;
	float shadowSample = PCF( 13, shadowCoords.xy, shadowCoords.z + depthBias, shadowMap);

	return mix(0.0005, 1.0, shadowSample);
}