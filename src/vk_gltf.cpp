#include "vk_gltf.h"

#define STB_IMAGE_IMPLEMENTATION 
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h"

#include "vk_device.h"

VkFilter getVkFilterMode(int32_t filterMode)
{
	switch (filterMode) {
	case -1:
	case 9728:
		return VK_FILTER_NEAREST;
	case 9729:
		return VK_FILTER_LINEAR;
	case 9984:
		return VK_FILTER_NEAREST;
	case 9985:
		return VK_FILTER_NEAREST;
	case 9986:
		return VK_FILTER_LINEAR;
	case 9987:
		return VK_FILTER_LINEAR;
	}

	std::cerr << "Unknown filter mode for getVkFilterMode: " << filterMode << std::endl;
	return VK_FILTER_NEAREST;
}

VkSamplerAddressMode getVkWrapMode(int32_t wrapMode)
{
	switch (wrapMode) {
	case -1:
	case 10497:
		return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	case 33071:
		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	case 33648:
		return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	}

	std::cerr << "Unknown wrap mode for getVkWrapMode: " << wrapMode << std::endl;
	return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

void VulkanGLTFModel::init(VulkanDevice* device, std::string filename, MetallicRoughnessMaterialWriter writer) {
	tinygltf::Model glTFInput;
	tinygltf::TinyGLTF gltfContext;
	std::string error, warning;

	bool fileLoaded = gltfContext.LoadASCIIFromFile(&glTFInput, &error, &warning, filename);


	std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 }
	};

	descriptorPool.init(device->logicalDevice, materialReferences.size(), sizes);

	std::vector<uint32_t> indices;
	std::vector<Vertex> vertices;

	if (fileLoaded) {
		loadImages(device, glTFInput);
		loadMaterials(device, glTFInput,  writer);
		loadTextures(device, glTFInput);
		const tinygltf::Scene& scene = glTFInput.scenes[0];
		for (size_t i = 0; i < scene.nodes.size(); i++) {
			const tinygltf::Node node = glTFInput.nodes[scene.nodes[i]];
			loadNode(node, glTFInput, nullptr, indices, vertices);
		}
		for (std::shared_ptr<Node> node : topNodes) {
			node.get()->refreshTransform(glm::mat4(1.f));
		}
	}
	else {
		std::cerr << "File not loaded: " << error << " | " << warning;
		throw;
	}

	const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
	const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

	meshBuffers.indexBuffer = device->createBuffer(vertexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
	meshBuffers.vertexBuffer = device->createBuffer(indexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

	VkBufferDeviceAddressInfo deviceAdressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,.buffer = meshBuffers.vertexBuffer.buffer };
	meshBuffers.vertexBufferAddress = vkGetBufferDeviceAddress(device->logicalDevice, &deviceAdressInfo);

	AllocatedBuffer staging = device->createBuffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	void* data = staging.allocation->GetMappedData();

	// copy vertex buffer
	memcpy(data, vertices.data(), vertexBufferSize);
	// copy index buffer
	memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);

	device->immediateSubmit([&](VkCommandBuffer* commandBuffer) {
		VkBufferCopy vertexCopy{ 0 };
		vertexCopy.dstOffset = 0;
		vertexCopy.srcOffset = 0;
		vertexCopy.size = vertexBufferSize;

		vkCmdCopyBuffer(*commandBuffer, staging.buffer, meshBuffers.vertexBuffer.buffer, 1, &vertexCopy);

		VkBufferCopy indexCopy{ 0 };
		indexCopy.dstOffset = 0;
		indexCopy.srcOffset = vertexBufferSize;
		indexCopy.size = indexBufferSize;

		vkCmdCopyBuffer(*commandBuffer, staging.buffer, meshBuffers.indexBuffer.buffer, 1, &indexCopy);
		}, 
		VulkanDevice::TRANSFER
	);

	device->destroyBuffer(staging);
}

void VulkanGLTFModel::loadImages(VulkanDevice* device, tinygltf::Model& input)
{
	// Images can be stored inside the glTF (which is the case for the sample model), so instead of directly
	// loading them from disk, we fetch them from the glTF loader and upload the buffers
	images.resize(input.images.size());
	for (size_t i = 0; i < input.images.size(); i++) {
		tinygltf::Image& glTFImage = input.images[i];
		// Get the image data from the glTF loader

		unsigned char* bufferData = nullptr;

		bool deleteBuffer = false;

		// We convert RGB-only images to RGBA, as most devices don't support RGB-formats in Vulkan
		if (glTFImage.component == 3) {
			uint64_t bufferSize = glTFImage.width * glTFImage.height * 4;
			bufferData = new unsigned char[bufferSize];
			unsigned char* rgba = bufferData;
			unsigned char* rgb = &glTFImage.image[0];
			for (size_t i = 0; i < glTFImage.width * glTFImage.height; ++i) {
				memcpy(rgba, rgb, sizeof(unsigned char) * 3);
				rgba += 4;
				rgb += 3;
			}
			deleteBuffer = true;
		}

		else {
			bufferData = &glTFImage.image[0];
		}
		// Load texture from image buffer
		
		images[i] = (device->createImage(bufferData, VkExtent3D{ (uint16_t)glTFImage.width,(uint16_t)glTFImage.height, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false));
		if (deleteBuffer) {
			delete[] bufferData;
		}
	}

}


void VulkanGLTFModel::loadSamplers(VulkanDevice* device, tinygltf::Model& input)
{
	samplerInfos.resize(input.samplers.size());
	for (size_t i = 0; i < input.samplers.size(); i++) {
		tinygltf::Sampler sampler = input.samplers[i];

		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = getVkFilterMode(sampler.magFilter);
		samplerInfo.minFilter = getVkFilterMode(sampler.minFilter);
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.addressModeU = getVkWrapMode(sampler.wrapS);
		samplerInfo.addressModeV = getVkWrapMode(sampler.wrapT);
		samplerInfo.addressModeW = getVkWrapMode(sampler.wrapT);
		samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
		samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		samplerInfo.maxAnisotropy = 1.0;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.maxLod = 1.0;
		samplerInfo.maxAnisotropy = 8.0f;
		samplerInfo.anisotropyEnable = VK_TRUE;

		samplerInfos[i] = samplerInfo;
	}
}


void VulkanGLTFModel::loadTextures(VulkanDevice* device, tinygltf::Model& input)
{
	textures.resize(input.textures.size());
	for (size_t i = 0; i < input.textures.size(); i++) {
		tinygltf::Texture texture = input.textures[i];
		textures[i].image = &images[texture.source];

		VkSamplerCreateInfo samplerInfo = {};
		if (texture.sampler == -1) {
			// No sampler specified, use a default one
			samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerInfo.magFilter = VK_FILTER_LINEAR;
			samplerInfo.minFilter = VK_FILTER_LINEAR;
			samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
			samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
			samplerInfo.maxAnisotropy = 1.0;
			samplerInfo.anisotropyEnable = VK_FALSE;
			samplerInfo.maxLod = 1.0;
			samplerInfo.maxAnisotropy = 8.0f;
			samplerInfo.anisotropyEnable = VK_TRUE;
		}
		else {
			samplerInfo = samplerInfos[texture.sampler];
		}

		VK_CHECK(vkCreateSampler(device->logicalDevice, &samplerInfo, nullptr, &textures[i].sampler));
	}
}

void VulkanGLTFModel::loadMaterials(VulkanDevice* device, tinygltf::Model& input, MetallicRoughnessMaterialWriter writer)
{
	uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 1));
	uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
	std::array<uint32_t, 16 * 16 > pixels; //for 16x16 checkerboard texture
	for (int x = 0; x < 16; x++) {
		for (int y = 0; y < 16; y++) {
			pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}
	defaultImage = device->createImage(pixels.data(), VkExtent3D{ 16, 16, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
	VkSamplerCreateInfo defaultSamplerInfo = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	defaultSamplerInfo.magFilter = VK_FILTER_NEAREST;
	defaultSamplerInfo.minFilter = VK_FILTER_NEAREST;
	vkCreateSampler(device->logicalDevice, &defaultSamplerInfo, nullptr, &defaultSampler);

	materialReferences.resize(input.materials.size());
	materialDataBuffer = device->createBuffer(sizeof(MetallicMaterialConstants) * input.materials.size(), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	int data_index = 0;

	MetallicMaterialConstants* sceneMaterialConstants = (MetallicMaterialConstants*)materialDataBuffer.info.pMappedData;
	for (size_t i = 0; i < input.materials.size(); i++) {
		// We only read the most basic properties required for our sample
		tinygltf::Material material = input.materials[i];

		MetallicMaterialConstants constants;
		if (material.values.find("baseColorFactor") != material.values.end()) {
			constants.baseColorFactors = glm::make_vec4(material.values["baseColorFactor"].ColorFactor().data());
		}
		if (material.values.find("roughnessFactor") != material.values.end()) {
			constants.metallicRoughnessFactors.g = static_cast<float>(material.values["roughnessFactor"].Factor());
		}
		if (material.values.find("metallicFactor") != material.values.end()) {
			constants.metallicRoughnessFactors.r = static_cast<float>(material.values["metallicFactor"].Factor());
		}
		sceneMaterialConstants[data_index] = constants;

		MetallicMaterialResources materialResources;
		// default the material textures
		materialResources.baseColourImage = defaultImage;
		materialResources.baseColourSampler = defaultSampler;
		materialResources.metallicRoughnessImage = defaultImage;
		materialResources.metallicRoughnessSampler = defaultSampler;

		// set the uniform buffer for the material data
		materialResources.dataBuffer = materialDataBuffer.buffer;
		materialResources.dataBufferOffset = data_index * sizeof(MetallicMaterialConstants);
		// grab textures from gltf file
		if (material.values.find("baseColorTexture") != material.values.end()) {
			materialResources.baseColourImage = *textures[material.values["baseColorTexture"].TextureIndex()].image;
			materialResources.baseColourSampler = textures[material.values["baseColorTexture"].TextureIndex()].sampler;
		}
		if (material.values.find("metallicRoughnessTexture") != material.values.end()) {
			materialResources.metallicRoughnessImage = *textures[material.values["metallicRoughnessTexture"].TextureIndex()].image;
			materialResources.metallicRoughnessSampler = textures[material.values["metallicRoughnessTexture"].TextureIndex()].sampler;
		}
		MaterialPassType passType = MaterialPassType::MainColour;
		if (material.alphaMode == "BLEND") {
			passType = MaterialPassType::Transparent;
		}
			materialReferences[i] = writer.writeMaterialInstance(device, passType, materialResources, descriptorPool);

			data_index++;
	}
}

void VulkanGLTFModel::loadNode(const tinygltf::Node& inputNode, const tinygltf::Model& input, std::shared_ptr<Node> parent, std::vector<uint32_t>& indices, std::vector<Vertex>& vertices)
{
	std::shared_ptr<Node> node;
	if (inputNode.mesh > -1) { 
		node = std::make_shared<MeshNode>();
	}
	else {
		node = std::make_shared<Node>();
	}
	node.get()->worldTransform = glm::mat4(1.0f);
	node.get()->parent = parent;

	// Get the local node matrix
	// It's either made up from translation, rotation, scale or a 4x4 matrix
	if (inputNode.translation.size() == 3) {
		node.get()->localTransform = glm::translate(node.get()->localTransform, glm::vec3(glm::make_vec3(inputNode.translation.data())));
	}
	if (inputNode.rotation.size() == 4) {
		node.get()->localTransform *= glm::mat4(glm::quat(glm::make_quat(inputNode.rotation.data())));
	}
	if (inputNode.scale.size() == 3) {
		node.get()->localTransform = glm::scale(node.get()->localTransform, glm::vec3(glm::make_vec3(inputNode.scale.data())));
	}
	if (inputNode.matrix.size() == 16) {
		node.get()->localTransform = glm::make_mat4x4(inputNode.matrix.data());
	};

	// Load node's children
	if (inputNode.children.size() > 0) {
		for (size_t i = 0; i < inputNode.children.size(); i++) {
			loadNode(input.nodes[inputNode.children[i]], input, node, indices, vertices);
		}
	}

	// If the node contains mesh data, we load vertices and indices from the buffers
	// In glTF this is done via accessors and buffer views
	if (inputNode.mesh > -1) {
		const tinygltf::Mesh mesh = input.meshes[inputNode.mesh];
		// Iterate through all primitives of this node's mesh
		for (size_t i = 0; i < mesh.primitives.size(); i++) {
			const tinygltf::Primitive& glTFPrimitive = mesh.primitives[i];
			uint32_t firstIndex = static_cast<uint32_t>(indices.size());
			uint32_t vertexStart = static_cast<uint32_t>(vertices.size());
			uint32_t indexCount = 0;
			// Vertices
			{
				const float* positionBuffer = nullptr;
				const float* normalsBuffer = nullptr;
				const float* texCoordsBuffer = nullptr;
				size_t vertexCount = 0;

				// Get buffer data for vertex positions
				if (glTFPrimitive.attributes.find("POSITION") != glTFPrimitive.attributes.end()) {
					const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("POSITION")->second];
					const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
					positionBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
					vertexCount = accessor.count;
				}
				// Get buffer data for vertex normals
				if (glTFPrimitive.attributes.find("NORMAL") != glTFPrimitive.attributes.end()) {
					const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("NORMAL")->second];
					const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
					normalsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}
				// Get buffer data for vertex texture coordinates
				// glTF supports multiple sets, we only load the first one
				if (glTFPrimitive.attributes.find("TEXCOORD_0") != glTFPrimitive.attributes.end()) {
					const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("TEXCOORD_0")->second];
					const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
					texCoordsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}

				// Append data to model's vertex buffer
				for (size_t v = 0; v < vertexCount; v++) {
					glm::vec2 uv = texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * 2]) : glm::vec3(0.0f);
					Vertex vert{};
					vert.position = glm::vec4(glm::make_vec3(&positionBuffer[v * 3]), 1.0f);
					vert.normal = glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f)));
					vert.uv_x = uv[0];
					vert.uv_y = uv[1];
					vert.color = glm::vec4(1.0f);
					vertices.push_back(vert);
				}
			}
			// Indices
			{
				const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.indices];
				const tinygltf::BufferView& bufferView = input.bufferViews[accessor.bufferView];
				const tinygltf::Buffer& buffer = input.buffers[bufferView.buffer];

				indexCount += static_cast<uint32_t>(accessor.count);

				// glTF supports different component types of indices
				switch (accessor.componentType) {
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
					const uint32_t* buf = reinterpret_cast<const uint32_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					for (size_t index = 0; index < accessor.count; index++) {
						indices.push_back(buf[index] + vertexStart);
					}
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
					const uint16_t* buf = reinterpret_cast<const uint16_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					for (size_t index = 0; index < accessor.count; index++) {
						indices.push_back(buf[index] + vertexStart);
					}
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
					const uint8_t* buf = reinterpret_cast<const uint8_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					for (size_t index = 0; index < accessor.count; index++) {
						indices.push_back(buf[index] + vertexStart);
					}
					break;
				}
				default:
					std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
					return;
				}
			}
			Primitive primitive{};
			primitive.firstIndex = firstIndex;
			primitive.indexCount = indexCount;
			primitive.materialIndex = glTFPrimitive.material;

			static_cast<MeshNode*>(node.get())->mesh.primitives.push_back(primitive);
		}
	}

	if (parent.get()) {
		parent->children.push_back(node);
	}
	else {
		topNodes.push_back(node);
	}
}


void VulkanGLTFModel::addNodeDraws(glm::mat4& topMatrix, DrawObjectCollection& collection)
{
	for (std::shared_ptr<Node> node : topNodes) {
		node.get()->appendDraw(this, topMatrix, collection);
	}
}

void VulkanGLTFModel::destroy(VulkanDevice* device) {
	meshBuffers.destroy(device);
	for (AllocatedImage image : images) {
		device->destroyImage(image);
	}
	for (Texture texture : textures) {
		vkDestroySampler(device->logicalDevice, texture.sampler, nullptr);
	}
	device->destroyImage(defaultImage);
	vkDestroySampler(device->logicalDevice, defaultSampler, nullptr);
	device->destroyBuffer(materialDataBuffer);
	descriptorPool.destroyPools(device->logicalDevice);
}


void MeshNode::appendDraw(VulkanGLTFModel* model, const glm::mat4& topMatrix, DrawObjectCollection& collection) {
	glm::mat4 nodeMatrix = topMatrix * worldTransform;

	for (auto& s : mesh.primitives) {
		DrawObjectInfo def;
		def.indexCount = s.indexCount;
		def.firstIndex = s.firstIndex;
		def.indexBuffer = model->meshBuffers.indexBuffer.buffer;
		def.material = &model->materialReferences[s.materialIndex];

		def.transform = nodeMatrix;
		def.vertexBufferAddress = model->meshBuffers.vertexBufferAddress;

		collection.opaqueObjects.push_back(def);
	}

	// recurse down
	Node::appendDraw(model, topMatrix, collection);
}