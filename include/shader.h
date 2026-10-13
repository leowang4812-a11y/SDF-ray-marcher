#pragma once

#include <iostream>
#include <string>
#include <string_view>
#include <fstream>
#include <sstream>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

// won't be using geometry shader.
class Shader {
public:
	Shader() = default;	
	Shader(std::string_view vertex_path, std::string_view fragment_path, std::string_view geometry_path="") {
		ID = glCreateProgram();
		auto attachShader = [&](std::string_view path) {
			if (path == "") return;

			std::cout << path << '\n';

			GLuint component_ID = initShader(path);
			glAttachShader(ID, component_ID);
			glDeleteShader(component_ID);
		};

		attachShader(vertex_path);
		attachShader(fragment_path);
		attachShader(geometry_path);

		glLinkProgram(ID);
		checkShaderError(ID, "PROGRAM");
	}

	void use() const {	glUseProgram(ID);}

	void setBool (std::string_view name, bool value) const {	glUniform1i(glGetUniformLocation(ID, name.data()), (int)value); }
	void setInt  (std::string_view name, int value) const {	glUniform1i(glGetUniformLocation(ID, name.data()), value); }
	void setFloat(std::string_view name, float value) const {	glUniform1f(glGetUniformLocation(ID, name.data()), value); }
	void setVec2 (std::string_view name, const glm::vec2& vec2) const {	glUniform2f(glGetUniformLocation(ID, name.data()), vec2.x, vec2.y); }
	void setVec3 (std::string_view name, float x, float y, float z) const {	glUniform3f(glGetUniformLocation(ID, name.data()), x, y, z); }
	void setVec3 (std::string_view name, const glm::vec3& vec3) const {	glUniform3f(glGetUniformLocation(ID, name.data()), vec3.x, vec3.y, vec3.z); }
	void setMat4 (std::string_view name, glm::mat4 mat4) const {	glUniformMatrix4fv(glGetUniformLocation(ID, name.data()), 1, GL_FALSE, glm::value_ptr(mat4)); }
	void setMat3 (std::string_view name, glm::mat3 mat3) const {	glUniformMatrix3fv(glGetUniformLocation(ID, name.data()), 1, GL_FALSE, glm::value_ptr(mat3)); }

public:
	unsigned int ID; // shader_program ID

private:
	int initShader(std::string_view shader_path) {
		std::string shader_suffix = static_cast<std::string>(shader_path.substr(shader_path.find('.')));
		GLenum shader_type;
		if (shader_suffix == ".vert") shader_type = GL_VERTEX_SHADER;
		else if (shader_suffix == ".frag") shader_type = GL_FRAGMENT_SHADER;
		else if (shader_suffix == ".geom") shader_type = GL_GEOMETRY_SHADER;
		else {
			std::cerr << "ERROR::SHADER::SUFFIX_INVALID: " << shader_path << " SUFFIX: " << shader_suffix << '\n';
			return -1;
		}

		std::string shader_code;
		std::ifstream shader_file;
		// ensures that it doesn't fail silently
		shader_file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
		try {
			shader_file.open(std::string{ shader_path });
			std::stringstream shader_stream;

			shader_stream << shader_file.rdbuf();
			shader_file.close();
			shader_code = shader_stream.str();
		}
		catch (const std::ifstream::failure& e) {
			std::cout << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ" << '\n';
		}

		const char* code = shader_code.c_str();

		GLuint ID;
		ID = glCreateShader(shader_type);
		glShaderSource(ID, 1, &code, NULL);
		glCompileShader(ID);
		checkShaderError(ID, shader_suffix);

		return ID;
	}

	void checkShaderError(unsigned int shader, std::string type) {
		int success;
		char info_log[1024];

		if (type == "PROGRAM") {
			glGetProgramiv(shader, GL_LINK_STATUS, &success);
			if (!success) {
				glGetProgramInfoLog(shader, 1024, NULL, info_log);
				std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << info_log << '\n';
			}
		}
		else {
			glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
			if (!success) {
				glGetShaderInfoLog(shader, 1024, NULL, info_log);
				std::cout << "ERROR::SHADER::COMPILATION_FAILED of type: " << type << '\n' << info_log << '\n';
			}
		}
	}
};
