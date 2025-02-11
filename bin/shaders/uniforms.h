layout(std140) uniform PerViewData0
{
	mat3x4 viewMatrix;
	mat4x4 projectionMatrix;
	mat4x4 viewProjMatrix;
	vec4 depthParameters;
	vec4 cameraPosition;
	vec4 ambientColor;
	vec4 iblParameters; // x = max lod level of PMREM texture
	vec4 dirLightDirection;
	vec4 dirLightColor;
	vec4 dirLightShadowSplits;
	vec4 dirLightShadowParameters;
	mat4x4 dirLightShadowMatrices[2];
};

struct Light
{
	vec4 position;
	vec4 direction;
	vec4 attenuation;
	vec4 color;
	uint viewMask;
	vec4 shadowParameters;
	mat4 shadowMatrix;
};

layout(std140) uniform LightData1
{
	Light lights[256];
};

#ifdef SKINNED
	layout(std140) uniform PerObjectData2
	{
		mat3x4 skinMatrices[96];
	};
#endif

// Preset uniforms
uniform mat3x4 worldMatrix;
uniform uint lightMask;
