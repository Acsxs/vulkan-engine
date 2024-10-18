#pragma once

#include <VkBootstrap.h>
#include "vk_types.h"
#include "vk_initializers.h"


struct SceneData {
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 viewProjection;

    glm::vec4 viewPos;
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection; // w for sun power
    glm::vec4 sunlightColor;
};

struct Node {
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