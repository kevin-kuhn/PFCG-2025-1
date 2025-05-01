#include <iostream>
#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace std;

const GLuint WIDTH = 1000, HEIGHT = 1000;

// Estado global
bool rotateX = false, rotateY = false, rotateZ = false;
glm::vec3 translation(0.0f);
float scaleFactor = 1.0f;

const char* vertexShaderSource = R"(
#version 450 core
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 color;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec4 finalColor;

void main() {
    gl_Position = projection * view * model * vec4(position, 1.0);
    finalColor = vec4(color, 1.0);
}
)";

const char* fragmentShaderSource = R"(
#version 450 core
in vec4 finalColor;
out vec4 color;

void main() {
    color = finalColor;
}
)";

// Prototipação
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
GLuint setupShader();
GLuint setupGeometry();

int main() {
    glfwInit();
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Cubo 3D - Kevin", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    glViewport(0, 0, WIDTH, HEIGHT);
    glEnable(GL_DEPTH_TEST);

    GLuint shaderProgram = setupShader();
    GLuint VAO = setupGeometry();
    GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
    GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
    GLint projLoc = glGetUniformLocation(shaderProgram, "projection");

    glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, -8.0f));
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)WIDTH / HEIGHT, 0.1f, 100.0f);

    std::vector<glm::vec3> cubePositions = {
        {0, 0, 0},
        {2, 0, 0},
        {-2, 0, 0},
        {0, 2, 0},
        {0, -2, 0}
    };

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        glClearColor(1, 1, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);

        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

        float angle = glfwGetTime();

        for (auto pos : cubePositions) {
            glm::mat4 model = glm::mat4(1.0f);
            glm::vec3 modelPos = pos;

            if (pos == glm::vec3(0, 0, 0)) {
                modelPos += translation;
                model = glm::translate(model, modelPos);
                model = glm::scale(model, glm::vec3(scaleFactor));
                if (rotateX) model = glm::rotate(model, angle, glm::vec3(1, 0, 0));
                if (rotateY) model = glm::rotate(model, angle, glm::vec3(0, 1, 0));
                if (rotateZ) model = glm::rotate(model, angle, glm::vec3(0, 0, 1));
            } else {
                model = glm::translate(model, modelPos);
            }

            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
            glBindVertexArray(VAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        glfwSwapBuffers(window);
    }

    glDeleteVertexArrays(1, &VAO);
    glfwTerminate();
    return 0;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS) return;

    switch (key) {
        case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(window, true); break;
        case GLFW_KEY_X: rotateX = true; rotateY = rotateZ = false; break;
        case GLFW_KEY_Y: rotateY = true; rotateX = rotateZ = false; break;
        case GLFW_KEY_Z: rotateZ = true; rotateX = rotateY = false; break;
        case GLFW_KEY_W: translation.z -= 0.1f; break;
        case GLFW_KEY_S: translation.z += 0.1f; break;
        case GLFW_KEY_A: translation.x -= 0.1f; break;
        case GLFW_KEY_D: translation.x += 0.1f; break;
        case GLFW_KEY_I: translation.y += 0.1f; break;
        case GLFW_KEY_J: translation.y -= 0.1f; break;
        case GLFW_KEY_LEFT_BRACKET: scaleFactor = std::max(0.1f, scaleFactor - 0.1f); break;
        case GLFW_KEY_RIGHT_BRACKET: scaleFactor += 0.1f; break;
    }
}

GLuint setupShader() {
    GLuint vShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vShader);

    GLuint fShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fShader);

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vShader);
    glAttachShader(shaderProgram, fShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vShader);
    glDeleteShader(fShader);
    return shaderProgram;
}

GLuint setupGeometry() {
    GLfloat vertices[] = {
        // Frente - Vermelho
        -0.5,-0.5,0.5, 1,0,0,  0.5,-0.5,0.5, 1,0,0,  0.5,0.5,0.5, 1,0,0,
        -0.5,-0.5,0.5, 1,0,0,  0.5,0.5,0.5, 1,0,0, -0.5,0.5,0.5, 1,0,0,
        // Trás - Verde
        -0.5,-0.5,-0.5, 0,1,0,  0.5,-0.5,-0.5, 0,1,0,  0.5,0.5,-0.5, 0,1,0,
        -0.5,-0.5,-0.5, 0,1,0,  0.5,0.5,-0.5, 0,1,0, -0.5,0.5,-0.5, 0,1,0,
        // Direita - Azul
        0.5,-0.5,-0.5, 0,0,1,  0.5,-0.5,0.5, 0,0,1,  0.5,0.5,0.5, 0,0,1,
        0.5,-0.5,-0.5, 0,0,1,  0.5,0.5,0.5, 0,0,1,  0.5,0.5,-0.5, 0,0,1,
        // Esquerda - Amarelo
        -0.5,-0.5,-0.5, 1,1,0, -0.5,-0.5,0.5, 1,1,0, -0.5,0.5,0.5, 1,1,0,
        -0.5,-0.5,-0.5, 1,1,0, -0.5,0.5,0.5, 1,1,0, -0.5,0.5,-0.5, 1,1,0,
        // Topo - Ciano
        -0.5,0.5,-0.5, 0,1,1,  0.5,0.5,-0.5, 0,1,1,  0.5,0.5,0.5, 0,1,1,
        -0.5,0.5,-0.5, 0,1,1,  0.5,0.5,0.5, 0,1,1, -0.5,0.5,0.5, 0,1,1,
        // Base - Magenta
        -0.5,-0.5,-0.5, 1,0,1, 0.5,-0.5,-0.5, 1,0,1, 0.5,-0.5,0.5, 1,0,1,
        -0.5,-0.5,-0.5, 1,0,1, 0.5,-0.5,0.5, 1,0,1, -0.5,-0.5,0.5, 1,0,1
    };

    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glGenVertexArrays(1, &VAO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    return VAO;
}
