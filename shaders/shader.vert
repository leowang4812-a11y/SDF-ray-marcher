#version 410 core

layout (location = 0) in vec2 aPos;

out vec2 uv;
out vec3 ray;

uniform mat3 cam_rotation;
uniform float focal_length;
uniform vec2 resolution;

void main() {
    float aspect = resolution.x / resolution.y;

    uv = aPos;
    // We aren't sampling textures, so we can make uv.x chonky for the ray casting.
    uv.x *= aspect;

    ray = cam_rotation * vec3(uv, focal_length);

    gl_Position = vec4(aPos, 0.0, 1.0);
}