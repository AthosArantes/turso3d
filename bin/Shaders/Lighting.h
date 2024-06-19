#extension GL_ARB_texture_cube_map_array : enable

#include <BRDF.h>

uniform sampler2DShadow dirShadowTex8;
uniform sampler2DShadow shadowTex9;
uniform samplerCubeArray faceSelectionTex10;
uniform usampler3D clusterTex11;

uniform samplerCube iemTex12; // Irradiance Environment Map
uniform samplerCube pmremTex13; // Prefiltered Mipmaped Radiance Environment Map
uniform sampler2D brdfTex14; // BRDF LUT map

const vec3 DIELECTRIC_FRESNEL = vec3(0.04);

vec3 CalculateClusterPos(const in vec2 screenPos, const in float depth)
{
	return vec3(
		screenPos.x,
		screenPos.y,
		sqrt(depth)
	);
}

float SampleShadowMap(sampler2DShadow shadowTex, const in vec4 shadowPos, const in vec4 parameters)
{
#ifdef HQSHADOW
	vec4 offsets1 = vec4(2.0 * parameters.xy * shadowPos.w, 0.0, 0.0);
	vec4 offsets2 = vec4(2.0 * parameters.x * shadowPos.w, -2.0 * parameters.y * shadowPos.w, 0.0, 0.0);
	vec4 offsets3 = vec4(2.0 * parameters.x * shadowPos.w, 0.0, 0.0, 0.0);
	vec4 offsets4 = vec4(0.0, 2.0 * parameters.y * shadowPos.w, 0.0, 0.0);

	return smoothstep(0.0, 1.0, (
		textureProjLod(shadowTex, shadowPos, 0.0) +
		textureProjLod(shadowTex, shadowPos + offsets1, 0.0) +
		textureProjLod(shadowTex, shadowPos - offsets1, 0.0) +
		textureProjLod(shadowTex, shadowPos + offsets2, 0.0) +
		textureProjLod(shadowTex, shadowPos - offsets2, 0.0) +
		textureProjLod(shadowTex, shadowPos + offsets3, 0.0) +
		textureProjLod(shadowTex, shadowPos - offsets3, 0.0) +
		textureProjLod(shadowTex, shadowPos + offsets4, 0.0) +
		textureProjLod(shadowTex, shadowPos - offsets4, 0.0)
	) * 0.1111);
#else
	vec4 offsets1 = vec4(parameters.xy * shadowPos.w, 0.0, 0.0);
	vec4 offsets2 = vec4(parameters.x * shadowPos.w, -parameters.y * shadowPos.w, 0.0, 0.0);

	return (
		textureProjLod(shadowTex, shadowPos + offsets1, 0.0) +
		textureProjLod(shadowTex, shadowPos - offsets1, 0.0) +
		textureProjLod(shadowTex, shadowPos + offsets2, 0.0) +
		textureProjLod(shadowTex, shadowPos - offsets2, 0.0)
	) * 0.25;
#endif
}

vec4 GetPointShadowPos(const in uint index, const in vec3 lightVec)
{
	vec4 pointParameters = lights[index].shadowMatrix[0];
	vec4 pointParameters2 = lights[index].shadowMatrix[1];
	float zoom = pointParameters2.x;
	float q = pointParameters2.y;
	float r = pointParameters2.z;

	vec3 axis = textureLod(faceSelectionTex10, vec4(lightVec, 0.0), 0.0).xyz;
	vec4 adjust = textureLod(faceSelectionTex10, vec4(lightVec, 1.0), 0.0);
	float depth = abs(dot(lightVec, axis));
	vec3 normLightVec = (lightVec / depth) * zoom;
	vec2 coords = vec2(dot(normLightVec.zxx, axis), dot(normLightVec.yzy, axis)) * adjust.xy + adjust.zw;
	return vec4(coords * pointParameters.xy + pointParameters.zw, q + r / depth, 1.0);
}

float SampleDirectionalShadow(const in vec4 worldPos)
{
	vec4 shadowSplits = dirLightShadowSplits;
	vec4 shadowParameters = dirLightShadowParameters;

	if (shadowParameters.z < 1.0 && worldPos.w < shadowSplits.y)
	{
		int matIndex = worldPos.w > shadowSplits.x ? 1 : 0;

		mat4 shadowMatrix = dirLightShadowMatrices[matIndex];
		float shadowFade = shadowParameters.z + clamp((worldPos.w - shadowSplits.z) * shadowSplits.w, 0.0, 1.0);

		return clamp(shadowFade + SampleShadowMap(dirShadowTex8, vec4(worldPos.xyz, 1.0) * shadowMatrix, shadowParameters), 0.0, 1.0);
	}

	return 1.0;
}

float SampleShadow(const in uint index, const in vec4 worldPos, const in vec3 lightVec, const in vec3 lightDir)
{
	vec4 shadowParameters = lights[index].shadowParameters;
	vec4 lightAttenuation = lights[index].attenuation;

	if (lightAttenuation.y > 0.0)
	{
		vec3 lightSpotDirection = lights[index].direction.xyz;
		float spotEffect = dot(lightDir, lightSpotDirection);
		float spotAtten = (spotEffect - lightAttenuation.y) * lightAttenuation.z;
		if (spotAtten <= 0.0) {
			return -1.0;
		}

		if (shadowParameters.z < 1.0) {
			return spotAtten * clamp((shadowParameters.z + SampleShadowMap(shadowTex9, vec4(worldPos.xyz, 1.0) * lights[index].shadowMatrix, shadowParameters)), 0.0, 1.0);
		}

		return spotAtten;

	} else if (shadowParameters.z < 1.0) {
		return clamp(shadowParameters.z + SampleShadowMap(shadowTex9, GetPointShadowPos(index, lightVec), shadowParameters), 0.0, 1.0);
	}

	return 1.0;
}

// ================================================================================================
// HIGH-LEVEL FUNCTIONS
// ================================================================================================
vec3 CalculateDirLight(const in vec4 worldPos, const in vec3 normal, const in vec3 albedo, const in vec3 f0, const in float metallic, const in float roughness)
{
	vec3 V = normalize(cameraPosition - worldPos.xyz);
	vec3 L = normalize(dirLightDirection);
	vec3 H = normalize(V + L);

	float NdotV = max(dot(normal, V), 0.0);
	float NdotH = max(dot(normal, H), 0.0);
	float NdotL = max(dot(normal, L), 0.0);
	float HdotV = max(dot(H, V), 0.0);

	float shadow = max(SampleDirectionalShadow(worldPos), 0.0);

	return CookTorranceBRDF(NdotV, NdotH, NdotL, HdotV, dirLightColor.rgb, albedo, f0, metallic, roughness) * shadow;
}

vec3 CalculateLight(const in uint index, const in vec4 worldPos, const in vec3 normal, const in vec3 albedo, const in vec3 f0, const in float metallic, const in float roughness)
{
#ifdef LIGHTMASK
	if ((lights[index].viewMask & modelLightMask) == 0u) {
		return vec3(0.0);
	}
#endif

	vec3 V = normalize(cameraPosition - worldPos.xyz);

	vec3 lightPosition = lights[index].position.xyz;
	vec4 lightAttenuation = lights[index].attenuation;
	vec4 lightColor = lights[index].color;
	vec3 lightDirection = lights[index].direction.xyz;

	vec3 lightVec = lightPosition - worldPos.xyz;
	vec3 scaledLightVec = lightVec * lightAttenuation.x;
	vec3 L = normalize(lightVec);
	vec3 H = normalize(V + L);

	float NdotV = max(dot(normal, V), 0.0);
	float NdotH = max(dot(normal, H), 0.0);
	float NdotL = max(dot(normal, L), 0.0);
	float HdotV = max(dot(H, V), 0.0);

	float attenuation = max(1.0 - dot(scaledLightVec, scaledLightVec), 0.0);
	if (attenuation == 0.0) {
		return vec3(0.0);
	}

	float shadow = max(SampleShadow(index, worldPos, lightVec, L), 0.0);

	return CookTorranceBRDF(NdotV, NdotH, NdotL, HdotV, lightColor.rgb, albedo, f0, metallic, roughness) * attenuation * shadow;
}

vec3 CalculateLighting(const in vec4 worldPos, const in vec2 screenPos, const in vec3 normal, const in vec3 albedo, const in float metallic, const in float roughness)
{
	vec3 f0 = mix(DIELECTRIC_FRESNEL, albedo, metallic);

	// Ambient lighting
	vec3 ambient = albedo * ambientColor.rgb;
	{
		vec3 V = normalize(cameraPosition - worldPos.xyz);
		float NdotV = max(dot(normal, V), 0.0);

		vec3 F = FresnelSchlickRoughness(NdotV, f0, roughness);
		vec3 R = reflect(-V, normal);

		vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
		vec3 irradiance = texture(iemTex12, normal).rgb;
		vec3 diffuse = irradiance * albedo * kD;

		// Sample both the pre-filter map and the BRDF lut and combine them together as per the Split-Sum approximation to get the IBL specular part.
		vec3 specIrradiance = textureLod(pmremTex13, R, roughness * iblParameters.x).rgb;
		vec2 specBRDF = texture(brdfTex14, vec2(NdotV, roughness)).rg;
		vec3 specularIBL = (F * specBRDF.x + specBRDF.y) * specIrradiance;

		ambient += diffuse + specularIBL;
	}

	vec3 color = CalculateDirLight(worldPos, normal, albedo, f0, metallic, roughness);

	uvec4 lightClusterData = texture(clusterTex11, CalculateClusterPos(screenPos, worldPos.w));
	while (lightClusterData.x > 0U) {
		color += CalculateLight((lightClusterData.x & 0xffU), worldPos, normal, albedo, f0, metallic, roughness);
		lightClusterData.x >>= 8U;
	}
	while (lightClusterData.y > 0U) {
		color += CalculateLight((lightClusterData.y & 0xffU), worldPos, normal, albedo, f0, metallic, roughness);
		lightClusterData.y >>= 8U;
	}
	while (lightClusterData.z > 0U) {
		color += CalculateLight((lightClusterData.z & 0xffU), worldPos, normal, albedo, f0, metallic, roughness);
		lightClusterData.z >>= 8U;
	}
	while (lightClusterData.w > 0U) {
		color += CalculateLight((lightClusterData.w & 0xffU), worldPos, normal, albedo, f0, metallic, roughness);
		lightClusterData.w >>= 8U;
	}

	return ambient + color;
}
