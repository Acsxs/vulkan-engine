#version 450
#extension GL_KHR_vulkan_glsl: enable
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "input_structures.glsl"

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outViewVec;
layout (location = 3) out mat3 TBN;


struct Vertex {

	vec3 position;
	float uv_x;
	vec3 normal;
	float uv_y;
	vec4 color;
	vec4 tangent;
	vec4 biTangent;
}; 

layout(buffer_reference, std430) readonly buffer VertexBuffer{ 
	Vertex vertices[];
};

//push constants block
layout( push_constant ) uniform constants
{
	mat4 renderMatrix;
	VertexBuffer vertexBuffer;
    int numWaves;
    float time;
} PushConstants;


vec3 fractionalBrownianMotion(int numWaves, float phaseShiftX , float phaseShiftY, vec3 position){
    vec3 pos = position;
    float sine = 0;
    for (int i=0; i<numWaves; i++){
        sine += (1/i)*sin(i*(time+position.x+phaseShiftX));
        sine += (1/i)*sin(i*(time+position.y+phaseShiftY));
    }
    pos.y += sine;
    return pos
}


void main() 
{

	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
	
	vec4 position = vec4(fractionalBrownianMotion(numWaves,0.f,0.f,v.position), 1.0f);

	vec4 renderPosition = PushConstants.renderMatrix * position;

    vec3 T = normalize((PushConstants.renderMatrix * v.tangent).xyz);
    vec3 B = normalize((PushConstants.renderMatrix * v.biTangent).xyz);
    vec3 N = normalize((PushConstants.renderMatrix * vec4(v.normal,0.f)).xyz);

	gl_Position =  sceneData.viewproj * renderPosition;

	outColor = v.color * materialData.colorFactors;	
	outUV.x = v.uv_x;
	outUV.y = v.uv_y;
	outViewVec = normalize(sceneData.viewPos.xyz - renderPosition.xyz);

	TBN = mat3(T, B, N);
}