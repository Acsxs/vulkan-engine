#version 450
#extension GL_GOOGLE_include_directive : require
#include "input_structures.glsl"

#define PI 3.1415926538
#define MEDIUMP_FLT_MAX    65504.0
#define saturateMediump(x) min(x, MEDIUMP_FLT_MAX)

layout (location = 0) in vec4 inColor;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inViewVec;
layout (location = 3) in mat3 TBN;

layout (location = 0) out vec4 outFragColor;

float map(float value, float min1, float max1, float min2, float max2) {
  return min2 + (value - min1) * (max2 - min2) / (max1 - min1);
}
vec4 map(vec4 value, float min1, float max1, float min2, float max2) {
  return min2 + (value - min1) * (max2 - min2) / (max1 - min1);
}

float D_GGX(float roughness, float NoH, const vec3 n, const vec3 h) {
    vec3 NxH = cross(n, h);
    float a = NoH * roughness;
    float k = roughness / (dot(NxH, NxH) + a * a);
    float d = k * k * (1.0 / PI);
    return saturateMediump(d);
}

float V_SmithGGXCorrelatedFast(float NoV, float NoL, float roughness) {
    float a = roughness;
    float GGXV = NoL * (NoV * (1.0 - a) + a);
    float GGXL = NoV * (NoL * (1.0 - a) + a);
    return 0.5 / (GGXV + GGXL);
}

vec3 F_Schlick(float u, vec3 f0, float f90) {
    return f0 + (vec3(f90) - f0) * pow(1.0 - u, 5.0);
}
vec3 F_Schlick(float u, vec3 f0) {
    float f = pow(1.0 - u, 5.0);
    return f + f0 * (1.0 - f);
}

vec4 getSpecular(vec3 normal, vec3 lightIn, vec3 lightOut, vec4 specularGlossiness) {
	vec3 halfVector = normalize(lightIn+lightOut);
	float NoH = dot(normal, halfVector);
	float NoV = dot(normal, lightOut);
	float NoL = dot(normal, lightIn);
	float roughness = 1-specularGlossiness[1];
	float u = dot(lightIn, lightOut);
	vec3 f0 = vec3(specularGlossiness[0], specularGlossiness[0], specularGlossiness[0]);
	vec3 specular = D_GGX(roughness, NoH, normal, halfVector) * V_SmithGGXCorrelatedFast(NoV,NoL,roughness) * F_Schlick(u, f0);
	specular = specular/(4*NoV*NoL);

	return vec4(specular,1.f);
}

vec4 getDiffuse(vec3 normal, vec3 lightIn, vec4 albedo) {
	return albedo * dot(normal, lightIn);
}

vec4 getBRDF(vec3 normal, vec3 lightIn, vec3 lightOut, vec4 albedo, vec4 specularGlossiness){
	// vec3 halfVector = normalize(lightIn+lightOut);
	
	vec4 diffuse = getDiffuse(normal, lightIn, albedo);
	vec4 specular = getSpecular(normal, lightIn, lightOut, specularGlossiness);
	return diffuse+specular;
	// vec3 specular =
}

void main() 
{
	vec3 toSun = sceneData.sunlightDirection.xyz;
	vec3 toView = inViewVec;

	vec4 color = inColor * texture(colorTex,inUV);
	vec4 occGlossSpec = materialData.specGlossFactors * texture(specGlossTex, inUV);

	vec3 normal = texture(normalTex, inUV).rgb;
	normal = normal * 2.0 - 1.0;   
	normal = normalize(TBN * normal);

	vec4 ambient = color * sceneData.ambientColor;

	// outFragColor = vec4(normal,1.f);
	outFragColor = getBRDF(normal, toSun, toView, color, occGlossSpec) * dot(normal, toSun.xyz) * sceneData.sunlightColor + ambient;
	// outFragColor = getSpecular(inNormal, toSun, toView, occGlossSpec);

	// vec3 albedo = color * diffuseStrength * sceneData.sunlightColor.w;


	// vec4 specular = vec4(0,0,0,0);
	// if (specularStrength > occGlossSpec.y) {
	// 	vec4 specular = map(vec4(sceneData.sunlightColor.xyz*specularStrength, 1.f), occGlossSpec.y, 1, 0, 1);
	// }
	

	// outFragColor = map(vec4(halfVector,1.f), -1, 1, 0, 1);
	// outFragColor = map(vec4(albedo + ambient, 1.f),0,1,0, specularStrength) + map(specular,0,1,specularStrength,1);
	// outFragColor = map(vec4(inNormal,1.f), -1, 1, 0, 1);
	// outFragColor = vec4(occGlossSpec, 1.f);
	// outFragColor = specular;
}