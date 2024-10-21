#version 450
#extension GL_KHR_vulkan_glsl: enable
#extension GL_EXT_buffer_reference : require


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

	gl_Position = renderPosition;

	outColor = v.color;	
}