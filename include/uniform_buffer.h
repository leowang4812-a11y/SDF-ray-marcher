#pragma once

class UniformBuffer {
public:
	UniformBuffer(uint32_t size, uint32_t binding)
        : m_size{ size } {
        glGenBuffers(1, &m_UBO);
        glBindBuffer(GL_UNIFORM_BUFFER, m_UBO);
        glBufferData(GL_UNIFORM_BUFFER, size, NULL, GL_STATIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0); 

        glBindBufferBase(GL_UNIFORM_BUFFER, binding, m_UBO);
    }
	void setData(const void* data) const {
        glBindBuffer(GL_UNIFORM_BUFFER, m_UBO);
        glBufferData(GL_UNIFORM_BUFFER, m_size, data, GL_STATIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

private:
	GLuint   m_UBO;
	uint32_t m_size;
};

class CameraUBO {
public:
	struct alignas(16) Data {
		glm::mat4 view;
		glm::mat4 proj;
	};

public:
 // only max 1 camera per shader so binding can be shared over all instances.
	CameraUBO()
		: buffer{ sizeof(Data), BINDING } {}

	void setData(const Data& data) const { buffer.setData(&data); }

private:
	static constexpr uint32_t BINDING = 0;

	UniformBuffer buffer;
};

class LightsUBO {
public:
	static constexpr std::size_t MAX_NUMBER_DIR_LIGHTS   = 8;
	static constexpr std::size_t MAX_NUMBER_POINT_LIGHTS = 8;
	static constexpr std::size_t MAX_NUMBER_SPOTLIGHTS   = 8;

	//glm::vec4 ensures 16 byte alignment
	struct alignas(16) DirLightData {
		glm::vec4 direction; 

		glm::vec4 ambient;  
		glm::vec4 diffuse;  
		glm::vec4 specular; 
	};

	struct alignas(16) PointLightData {
		glm::vec3 pos; float _pad;

		glm::vec3 ambient; 	float constant;
		glm::vec3 diffuse;  float linear;
		glm::vec3 specular; float quadratic;
	};

	struct alignas(16) SpotLightData {
		glm::vec3 pos;       float inner_cutoff;
		glm::vec3 direction; float outer_cutoff;

		glm::vec3 ambient;  float constant;
		glm::vec3 diffuse;  float linear;
		glm::vec3 specular; float quadratic;
	};

	// num_point_lights and num_spotlights must be below the maximum of each. 
	struct alignas(16) Data {
		int num_dir_lights;
		int num_point_lights;
		int num_spotlights;
		int _pad;

		std::array<DirLightData, MAX_NUMBER_DIR_LIGHTS>     dir_lights;
		std::array<PointLightData, MAX_NUMBER_POINT_LIGHTS> point_lights;
		std::array<SpotLightData, MAX_NUMBER_SPOTLIGHTS>    spotlights;
	};

public:
	LightsUBO() 
		: buffer{ sizeof(Data), BINDING} {}

	void setData(const Data& data) const { buffer.setData(&data); }

private:
	static constexpr uint32_t BINDING = 1;

	UniformBuffer buffer;
};