#pragma once

#include <VkBootstrap.h>
#include "vk_types.h"
#include "vk_material.h"
#include "vk_initializers.h"
#include <unordered_map>

#include "tiny_gltf.h"

struct MeshBuffers {
    AllocatedBuffer indexBuffer;
    AllocatedBuffer vertexBuffer;
    VkDeviceAddress vertexBufferAddress;

    void destroy(VulkanDevice* device) { indexBuffer.destroy(device); vertexBuffer.destroy(device); };
};

struct Texture {
    AllocatedImage* image;
    VkSampler sampler;
};

// Contains data for a single draw call
struct Primitive {
    uint32_t firstIndex;
    uint32_t indexCount;
    std::shared_ptr<MaterialInstance> materialInstance;
};

struct Mesh {
    std::vector<Primitive> primitives;
};


struct SceneData {
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 viewProjection;

    glm::vec4 viewPos;
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection; // w for sun power
    glm::vec4 sunlightColor;
};

//forward declaration for Node
class VulkanGLTFModel;

// Extendable class for recursive draw
struct IRenderable {
    virtual void appendDraw( const glm::mat4& topMatrix, DrawObjectCollection* collection) {};
};

// A node represents an object in the glTF scene graph
struct Node: IRenderable {
    std::weak_ptr<Node> parent;
    std::vector<std::shared_ptr<Node>> children;

    glm::mat4 localTransform;
    glm::mat4 worldTransform;

    void refreshTransform(const glm::mat4& parentMatrix)
    {
        worldTransform = parentMatrix * localTransform;
        for (auto c : children) {
            c->refreshTransform(worldTransform);
        }
    }

    virtual void appendDraw( const glm::mat4& topMatrix, DrawObjectCollection* collection)
    {
        // draw children
        for (auto& c : children) {
            c->appendDraw(topMatrix, collection);
        }
    }
};

struct MeshNode : public Node {
    Mesh mesh;
    MeshBuffers* meshBuffers;
    virtual void appendDraw(const glm::mat4& topMatrix, DrawObjectCollection* collection) override;
};

// Contains everything required to render a glTF model in Vulkan
// This class is heavily simplified (compared to glTF's feature set) but retains the basic glTF structure
class VulkanGLTFModel {
public:
    MeshBuffers meshBuffers = {};
    int indexCount = 0;

    std::vector<std::shared_ptr<MaterialInstance>> materialInstances;
    std::shared_ptr<MaterialInstance> defaultMaterialInstance;

    std::vector<AllocatedImage> images;
    std::vector<VkSamplerCreateInfo> samplerInfos;
    std::vector<Texture> textures;


    std::vector<std::shared_ptr<Node>> topNodes;

    DescriptorAllocatorGrowable descriptorPool;

    AllocatedBuffer materialDataBuffer;

    AllocatedImage defaultImage;
    VkSampler defaultSampler;
    


    void init(VulkanDevice* device, std::string filename, MetallicRoughnessMaterialWriter* writer);
    void destroy(VulkanDevice* device);
    void addNodeDraws(glm::mat4& topMatrix, DrawObjectCollection* col);

private:
    void loadImages(VulkanDevice* device, tinygltf::Model& input);
    void loadSamplers(VulkanDevice* device, tinygltf::Model& input);
    void loadTextures(VulkanDevice* device, tinygltf::Model& input);
    void loadMaterials(VulkanDevice* device, tinygltf::Model& input, MetallicRoughnessMaterialWriter* writer);
    void loadNode(const tinygltf::Node& inputNode, const tinygltf::Model& input, std::shared_ptr<Node> parent, std::vector<uint32_t>& indices, std::vector<Vertex>& vertices);
};
