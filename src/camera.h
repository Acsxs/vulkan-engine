#pragma once

#include "vk_types.h"
#include <SDL_events.h>

class Camera {
public:
    float mouseSense=0.5;
    float moveSpeed=0.25;

    bool relMouse = true;

    glm::vec3 velocity;
    glm::vec3 position;
    // vertical rotation
    float pitch{ 0.f };
    // horizontal rotation
    float yaw{ 0.f };

    glm::mat4 getViewMatrix();
    glm::mat4 getRotationMatrix();

    void processSDLEvent(SDL_Event& e);

    void update();
};
