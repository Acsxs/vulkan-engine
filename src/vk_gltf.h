#pragma once

#include <VkBootstrap.h>
#include "vk_types.h"
#include "vk_initializers.h"
#include <unordered_map>

struct Scene {
    std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
    std::vector<std::shared_ptr<Node>> topNodes;
    std::unordered_map<std::string, AllocatedImage> images;
    std::unordered_map<std::string, std::shared_ptr<MaterialReference>> materials;
};

struct GeometrySurface {
    uint32_t startIndex;
    uint32_t count;
    std::shared_ptr<MaterialReference> material;
};

struct MeshBuffers {
    AllocatedBuffer indexBuffer;
    AllocatedBuffer vertexBuffer;
    VkDeviceAddress vertexBufferAddress;
};

struct Mesh {
    std::string name;

    std::vector<GeoSurface> surfaces;
    MeshBuffers meshBuffers;
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