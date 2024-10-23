#version 450
#extension GL_KHR_vulkan_glsl: enable
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "input_structures.glsl"

layout (location = 0) out vec4 outColor;


struct Vertex {
	vec4 position;
	vec4 color;
}; 

layout(buffer_reference, std430) readonly buffer VertexBuffer{ 
	Vertex vertices[];
};

//push constants block
layout( push_constant ) uniform constants
{
	mat4 renderMatrix;
	VertexBuffer vertexBuffer;
} PushConstants;



void main() 
{
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
	
	vec4 position = v.position;

	vec4 renderPosition = PushConstants.renderMatrix * position;

	gl_Position = sceneData.viewproj * renderPosition;

	outColor = v.color;	
}

// void main() 
// {
// 	//const array of positions for the triangle
// 	const vec3 positions[3] = vec3[3](
// 		vec3(1.f,1.f, 0.0f),
// 		vec3(-1.f,1.f, 0.0f),
// 		vec3(0.f,-1.f, 0.0f)
// 	);

// 	//const array of colors for the triangle
// 	const vec3 colors[3] = vec3[3](
// 		vec3(1.0f, 0.0f, 0.0f), //red
// 		vec3(0.0f, 1.0f, 0.0f), //green
// 		vec3(00.f, 0.0f, 1.0f)  //blue
// 	);

// 	//output the position of each vertex
// 	gl_Position = vec4(positions[gl_VertexIndex], 1.0f);
// 	outColor = vec4(colors[gl_VertexIndex], 1.f);
// }