#pragma once

#include <VkBootstrap.h>
#include "vk_types.h"
#include "vk_initializers.h"
#include <unordered_map>


struct SceneBuffers {
    AllocatedBuffer indexBuffer;
    AllocatedBuffer vertexBuffer;
    VkDeviceAddress vertexBufferAddress;

    void destroy(VulkanDevice* device) { device->destroyBuffer(indexBuffer); device->destroyBuffer(vertexBuffer); };
};

struct Texture {
    AllocatedImage* image;
    VkSampler* sampler;
};


// Contains everything required to render a glTF model in Vulkan
// This class is heavily simplified (compared to glTF's feature set) but retains the basic glTF structure
class VulkanGLTFModel {
public:
    // The class requires some Vulkan objects so it can create it's own resources
    VulkanDevice* vulkanDevice;
    VkQueue copyQueue;

    SceneBuffers SceneBuffers;
    int indexCount;
    std::vector<SpecularMaterialReference> materialReference;
    std::vector<AllocatedImage> images;
    std::vector<std::shared_ptr<Node>> topNodes;

    std::vector<VkSampler> samplers;

    DescriptorAllocatorGrowable descriptorPool;

    AllocatedBuffer materialDataBuffer;
    


    void init(VulkanDevice* device);
    void destroy(VulkanDevice* device);
    void addNodeDraws();

private:
    void loadImages(VulkanDevice* device, tinygltf::Model& input);
    void loadTextures(VulkanDevice* device, tinygltf::Model& input);
    void loadMaterials(VulkanDevice* device, tinygltf::Model& input);
    void loadNode(VulkanDevice* device, const tinygltf::Node& inputNode, const tinygltf::Model& input, std::shared_ptr<Node> parent, std::vector<uint32_t>& indices, std::vector<Vertex>& vertices);
};

// Contains data for a single draw call
struct Primitive {
    uint32_t firstIndex;
    uint32_t indexCount;
    int32_t materialIndex;
};

struct Mesh {
    std::vector<Primitive> primitives;
};



struct GeometrySurface {
    uint32_t startIndex;
    uint32_t count;
    std::shared_ptr<MaterialReference> material;
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

// A node represents an object in the glTF scene graph
struct Node: public IRenderable {
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

    virtual void appendDraw(const glm::mat4& topMatrix, DrawObjectCollection& ctx)
    {
        // draw children
        for (auto& c : children) {
            c->appendDraw(topMatrix, ctx);
        }
    }
};

struct MeshNode : public Node {
    std::shared_ptr<MeshAsset> mesh;

    virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx) override;

};