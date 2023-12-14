#version 330 core

// ================================================================================================
// VERTEX SHADER
// ================================================================================================
#ifdef COMPILE_VS

in vec3 position;
out vec2 texCoord;

void main()
{
	gl_Position = vec4(position, 1.0);
	texCoord = vec2(position.xy) * 0.5 + 0.5;
}

#endif

// ================================================================================================
// FRAGMENT SHADER
// ================================================================================================
#ifdef COMPILE_FS

// This shader performs downsampling on a texture,
// as taken from Call Of Duty method, presented at ACM Siggraph 2014.
// This particular method was customly designed to eliminate
// "pulsating artifacts and temporal stability issues".

// Remember to add bilinear minification filter for this texture!
// Remember to use a floating-point texture format (for HDR)!
// Remember to use edge clamping for this texture!

uniform sampler2D srcTex0;
uniform vec2 invSrcSize;

in vec2 texCoord;
layout (location = 0) out vec3 downsample;

void main()
{
	float x = invSrcSize.x;
	float y = invSrcSize.y;

	// Take 13 samples around current texel:
	// a - b - c
	// - j - k -
	// d - e - f
	// - l - m -
	// g - h - i
	// === ('e' is the current texel) ===
	vec3 a = texture(srcTex0, vec2(texCoord.x - 2*x, texCoord.y + 2*y)).rgb;
	vec3 b = texture(srcTex0, vec2(texCoord.x,       texCoord.y + 2*y)).rgb;
	vec3 c = texture(srcTex0, vec2(texCoord.x + 2*x, texCoord.y + 2*y)).rgb;

	vec3 d = texture(srcTex0, vec2(texCoord.x - 2*x, texCoord.y)).rgb;
	vec3 e = texture(srcTex0, vec2(texCoord.x,       texCoord.y)).rgb;
	vec3 f = texture(srcTex0, vec2(texCoord.x + 2*x, texCoord.y)).rgb;

	vec3 g = texture(srcTex0, vec2(texCoord.x - 2*x, texCoord.y - 2*y)).rgb;
	vec3 h = texture(srcTex0, vec2(texCoord.x,       texCoord.y - 2*y)).rgb;
	vec3 i = texture(srcTex0, vec2(texCoord.x + 2*x, texCoord.y - 2*y)).rgb;

	vec3 j = texture(srcTex0, vec2(texCoord.x - x, texCoord.y + y)).rgb;
	vec3 k = texture(srcTex0, vec2(texCoord.x + x, texCoord.y + y)).rgb;
	vec3 l = texture(srcTex0, vec2(texCoord.x - x, texCoord.y - y)).rgb;
	vec3 m = texture(srcTex0, vec2(texCoord.x + x, texCoord.y - y)).rgb;

	// Apply weighted distribution:
	// 0.5 + 0.125 + 0.125 + 0.125 + 0.125 = 1
	// a,b,d,e * 0.125
	// b,c,e,f * 0.125
	// d,e,g,h * 0.125
	// e,f,h,i * 0.125
	// j,k,l,m * 0.5
	// This shows 5 square areas that are being sampled. But some of them overlap,
	// so to have an energy preserving downsample we need to make some adjustments.
	// The weights are the distributed, so that the sum of j,k,l,m (e.g.)
	// contribute 0.5 to the final color output. The code below is written
	// to effectively yield this sum. We get:
	// 0.125*5 + 0.03125*4 + 0.0625*4 = 1
	downsample = e*0.125;
	downsample += (a+c+g+i)*0.03125;
	downsample += (b+d+f+h)*0.0625;
	downsample += (j+k+l+m)*0.125;
}

#endif
