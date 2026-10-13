#include <iostream>
#include <format>
#include <string>

#define GL_SILENCE_DEPRECATION
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL 
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp> 

#include "shader.h"
#include "camera.h"

int width, height;

void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
	static float mouse_xlast = width / 2.0f;
	static float mouse_ylast = height / 2.0f;
	static bool first_opened_mouse = false;

	if (first_opened_mouse) {
		mouse_xlast = xpos;
		mouse_ylast = ypos;
		first_opened_mouse = false;
	}
	float xoffset = xpos - mouse_xlast;
	float yoffset = mouse_ylast - ypos;

	mouse_xlast = xpos;
	mouse_ylast = ypos;

    constexpr float MOUSE_SENS = 0.1f;

	Camera* cam = static_cast<Camera*>(glfwGetWindowUserPointer(window));
	cam->yaw += xoffset * MOUSE_SENS;
    cam->pitch += yoffset * MOUSE_SENS;

    if (cam->pitch > 89.0f) cam->pitch = 89.0f;
    else if (cam->pitch < -89.0f) cam->pitch = -89.0f;
}

void move_camera(Camera& cam, GLFWwindow* window, float dt) {
	if (glfwGetKey(window, GLFW_KEY_BACKSPACE) == GLFW_PRESS) {
		glfwSetWindowShouldClose(window, true);
	}

    constexpr float MOVESPEED = 2.0f;

    glm::vec3 front = cam.get_view_dir();
    glm::vec3 side  = glm::normalize(glm::cross(front, glm::vec3(0.0, 1.0, 0.0)));

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        cam.pos += front * MOVESPEED * dt;
    }
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        cam.pos -= front * MOVESPEED * dt;
    }
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        cam.pos += side * MOVESPEED * dt;
    }
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        cam.pos -= side * MOVESPEED * dt;
    }
}

struct Material {
    glm::vec3 diffuse_col;
    float albedo;
    float fresnel_0;
};

struct Box {
    glm::vec3 centre;
    glm::vec3 dimensions;
};

struct Plane {
    glm::vec3 normal;
    float offset;
};

struct Sphere {
    glm::vec3 centre;
    float radius;

    void rotate(glm::vec3 pivot, glm::vec3 axis, float angle) {
        // translate 
        // rotate
        // translate back
        glm::vec3 trans_point = centre - pivot;
        glm::mat4 rot_mat = glm::rotate(glm::mat4(1.0f), angle, axis);

        centre = glm::vec3(rot_mat * glm::vec4(trans_point, 1.0)) + pivot;
    }
};

void set_sphere(Shader& shader, int idx, Sphere& sphere) {
    shader.setVec3(std::format("spheres[{}].centre", idx), sphere.centre);
    shader.setFloat(std::format("spheres[{}].radius", idx), sphere.radius);
}

void set_plane(Shader& shader, int idx, Plane& plane) {
    shader.setVec3(std::format("planes[{}].normal", idx), plane.normal);
    shader.setFloat(std::format("planes[{}].offset", idx), plane.offset);
}

void set_box(Shader& shader, int idx, Box& box) {
    shader.setVec3(std::format("boxes[{}].centre", idx), box.centre);
    shader.setVec3(std::format("boxes[{}].dimensions", idx), box.dimensions);
}

void set_material(Shader& shader, int idx, const Material& mat, const std::string& prefix) {
    shader.setVec3(prefix + std::format("[{}].mat.diffuse_col", idx), mat.diffuse_col);
    shader.setFloat(prefix + std::format("[{}].mat.albedo", idx), mat.albedo);
    shader.setFloat(prefix + std::format("[{}].mat.fresnel_0", idx), 0.0);
    if (idx == 3 || idx == 4 || idx == 5) {
        shader.setFloat(prefix + std::format("[{}].mat.albedo", idx), 0.05);
        shader.setFloat(prefix + std::format("[{}].mat.fresnel_0", idx), 0.8);
    } 
}

static std::array<bool, 256> prev_state = {GLFW_RELEASE};
static std::array<bool, 256> prev_mode = {false};
static std::array<bool, 256> move_mode = {false};
static std::array<bool, 256> transition = {false};

void update_key_state(GLFWwindow* window, int key) {
    bool cur_state = glfwGetKey(window, key);

    if (cur_state == GLFW_PRESS && prev_state[key] == GLFW_RELEASE) {
        move_mode[key] = !move_mode[key];
    }

    transition[key] = (prev_mode[key] == false && move_mode[key] == true);

    prev_mode[key] = move_mode[key];
    prev_state[key] = cur_state;
}

void place_sphere(GLFWwindow* window, std::vector<Sphere>& spheres) {
    if (transition[GLFW_KEY_SPACE]) {
        spheres.emplace_back(Sphere(glm::vec3{ 0.0, 5.0, 0.0 }, 1.0));
    }
}

constexpr float STARTING_DIST = 5.0;
void move_sphere(GLFWwindow* window, Sphere& active, Camera& cam, float& dist, float dt) {
    constexpr float FAST_MS = 3.0;
    constexpr float SLOW_MS = FAST_MS * 0.05;

    float MS;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)) {
        MS = SLOW_MS;
    }
    else {
        MS = FAST_MS;
    }

    active.centre = cam.pos + cam.get_view_dir() * dist;

    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        active.radius += 0.2 * MS * dt;
    }
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
        active.radius -= 0.2 * MS * dt;
    }
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
        dist += MS * dt;
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        dist -= MS * dt;
    }
}

// select object based on lowest dot product between view_dir and centre? that kinda works.
void select_move_sphere(GLFWwindow* window, Camera& cam, std::vector<Sphere>& spheres, float dt) {
    static Sphere* closest = nullptr;

    if (transition[GLFW_KEY_G]) {
        glm::vec3 view_dir = cam.get_view_dir();

        float max_dot = -1e9;
        for (Sphere& sphere : spheres) {
            glm::vec3 vec_to_centre = glm::normalize(sphere.centre - cam.pos);

            float dot_prod = dot(vec_to_centre, view_dir);
            if (dot_prod > max_dot) {
                max_dot = dot_prod;
                closest = &sphere;
            }
        }
    }

    if (closest) {
        // This checks if G is turned on
        static float dist = STARTING_DIST;
        if (move_mode[GLFW_KEY_G]) {
            move_sphere(window, *closest, cam, dist, dt);
        }
        else {
            dist = STARTING_DIST;
        }
    }
}

// so this all just draws a quad on the screen.
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "OpenGL", nullptr, nullptr);
    glfwMakeContextCurrent(window);



    // Initialize GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW\n";
        return -1;
    }
    
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);


    float vertices[] = {
        // positions
        -1.0f, -1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,

        -1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f,  1.0f
    };

    GLuint VAO, VBO;

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(
        0,
        2,
        GL_FLOAT,
        GL_FALSE,
        2 * sizeof(float),
        (void*)0
    );

    Camera cam{
        pos: glm::vec3(0.0),
        pitch: 0.0,
        yaw: 0.0
    };
    // This basically processes the function as the mouse moves.
    // Setting the window user pointer to have the camera allows the camera to move directly in the function.
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetWindowUserPointer(window, &cam);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    Shader shader{ "shaders/shader.vert", "shaders/shader.frag" };
    shader.use();

    glEnableVertexAttribArray(0);

    float dt;
	float last_time = 0.0f;
	int frame_index = 0;

    // Scene settings:
    std::vector<Sphere> spheres = {
        {{ 0, -50, 0 }, 50},
        {{ 1, 1, 1 }, 1.2},
        {{ 3, 2, 2 }, 1.2},
        {{ 5.5, 1, 1 }, 1.4},
        {{ 1, 1.5, 4 }, 0.8}
    };

    std::vector<Plane> planes;
    std::vector<Box> boxes = {
        {{8, 5, 10}, {0.2, 0.2, 3}},
        {{8, 2, 5}, {1, 1, 3}},
        {{6, 1, 15}, {4, 1, 1}},
        {{2, 5, 40}, {1, 20, 20}},
        {{10, 5, 40}, {1, 20, 20}},
    };

    while (!glfwWindowShouldClose(window)) {
		float cur_time = glfwGetTime();
		dt = cur_time - last_time;
		last_time = cur_time;

        // Set Uniforms 
        shader.setMat3("cam_rotation", cam.get_rot_mat());
        shader.setFloat("focal_length", 1.0);
        shader.setVec2("resolution", glm::vec2(width, height));

        shader.setFloat("time", cur_time);
        shader.setVec3("cam_pos", cam.pos);
        shader.setVec3("view_dir", cam.get_view_dir());

        shader.setInt("num_spheres", spheres.size());
        shader.setInt("num_planes", planes.size());
        shader.setInt("num_boxes", boxes.size());

        for (int i = 0; i < spheres.size(); ++i) {
            spheres[i].rotate(glm::vec3(0, 0, 0), glm::vec3(0, 1, 0), dt * (i+1) / 50.0);
        }

        static const Material default_mat = Material(glm::vec3(0.8, 0.6, 0.7), 1.0, 0.8);
        for (int i = 0; i < spheres.size(); ++i){ 
            set_sphere(shader, i, spheres[i]);
            set_material(shader, i, default_mat, "spheres");
        } 
        for (int i = 0; i < planes.size(); ++i) {
            set_plane(shader, i, planes[i]);
            set_material(shader, i, default_mat, "planes");
        }
        for (int i = 0; i < boxes.size(); ++i) {
            set_box(shader, i, boxes[i]);
            set_material(shader, i, default_mat, "boxes");
        }

        // Update all keys to check if any of them have been pressed
        for (int i = 0; i < 256; ++i) {
            update_key_state(window, i);
        }

        // Main scripts

        static float sphere_dist = STARTING_DIST;
        
        place_sphere(window, spheres);

        if (move_mode[GLFW_KEY_SPACE]) {
            move_sphere(window, spheres.back(), cam, sphere_dist, dt);
        }
        else {
            sphere_dist = STARTING_DIST;
        }
        move_camera(cam, window, dt);

        select_move_sphere(window, cam, spheres, dt);

        glClear(GL_COLOR_BUFFER_BIT);

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
}