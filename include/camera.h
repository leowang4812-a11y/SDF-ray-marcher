#pragma once

#include <glm/glm.hpp>

const glm::vec3 START_VIEW_DIR = { 0.0, 0.0, 1.0 };

glm::mat3 yaw_mat(float angle) {
    angle = glm::radians(angle);
    float s = sin(angle);
    float c = cos(angle);
    return glm::mat3(
        c, 0, -s,   
        0, 1, 0,  
        s, 0, c
    );
}

glm::mat3 pitch_mat(float angle) {
    angle = glm::radians(angle);
    float s = sin(angle);
    float c = cos(angle);
    return glm::mat3(
        1, 0, 0,   
        0, c, -s,  
        0, s, c
    );
}

struct Camera {
    glm::vec3 pos;
    float pitch;
    float yaw;

    glm::mat3 get_rot_mat() {
        return yaw_mat(yaw) * pitch_mat(pitch);
    }

    glm::vec3 get_view_dir() {
        return get_rot_mat() * START_VIEW_DIR;
    }
};