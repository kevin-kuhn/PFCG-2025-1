#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

using namespace std;
using namespace glm;

struct Vertex {
    vec3 position;
    vec2 texCoord;
};

vector<Vertex> vertices;
GLuint shaderProgram, VAO, VBO, textureID;

const char* vertexShaderSource = R"(
#version 450 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec2 TexCoord;

void main() {
    gl_Position = projection * view * model * vec4(position, 1.0);
    TexCoord = texCoord;
}
)";

const char* fragmentShaderSource = R"(
#version 450 core
in vec2 TexCoord;
uniform sampler2D texBuff;
out vec4 FragColor;

void main() {
    FragColor = texture(texBuff, TexCoord);
}
)";

void key_callback(GLFWwindow* window, int key, int, int action, int) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

GLuint compileShader() {
    GLuint vShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vShader);

    GLuint fShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fShader);

    GLuint program = glCreateProgram();
    glAttachShader(program, vShader);
    glAttachShader(program, fShader);
    glLinkProgram(program);

    glDeleteShader(vShader);
    glDeleteShader(fShader);
    return program;
}

GLuint loadTexture(const string& path) {
    int w, h, ch;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 0);
    if (!data) {
        cerr << "Erro ao carregar textura: " << path << endl;
        return 0;
    }

    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GLenum format = (ch == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);
    return texID;
}

bool loadOBJWithMTL(const string& objPath, const string& mtlDir) {
    ifstream objFile(objPath);
    if (!objFile.is_open()) return false;

    vector<vec3> positions;
    vector<vec2> texCoords;
    map<string, string> materials;
    string activeTexture;

    string line;
    while (getline(objFile, line)) {
        istringstream iss(line);
        string prefix;
        iss >> prefix;

        if (prefix == "v") {
            float x, y, z;
            iss >> x >> y >> z;
            positions.emplace_back(x, y, z);
        } else if (prefix == "vt") {
            float u, v;
            iss >> u >> v;
            texCoords.emplace_back(u, v);
        } else if (prefix == "f") {
            string v1, v2, v3;
            iss >> v1 >> v2 >> v3;
            string verts[] = { v1, v2, v3 };
            for (string& v : verts) {
                size_t slash1 = v.find('/');
                size_t slash2 = v.find('/', slash1 + 1);
                int posIdx = stoi(v.substr(0, slash1)) - 1;
                int texIdx = stoi(v.substr(slash1 + 1, slash2 - slash1 - 1)) - 1;
                vertices.push_back({ positions[posIdx], texCoords[texIdx] });
            }
        } else if (prefix == "mtllib") {
            string mtlFile;
            iss >> mtlFile;
            ifstream mtlStream(mtlDir + "/" + mtlFile);
            if (mtlStream.is_open()) {
                string line2, matName;
                while (getline(mtlStream, line2)) {
                    istringstream mss(line2);
                    string tag;
                    mss >> tag;
                    if (tag == "newmtl") {
                        mss >> matName;
                    } else if (tag == "map_Kd") {
                        string texFile;
                        mss >> texFile;
                        materials[matName] = texFile;
                    }
                }
            }
        } else if (prefix == "usemtl") {
            string matName;
            iss >> matName;
            activeTexture = materials[matName];
        }
    }

    if (!activeTexture.empty()) {
        textureID = loadTexture(mtlDir + "/" + activeTexture);
        if (textureID == 0) return false;
    } else {
        cerr << "Nenhuma textura definida no .mtl" << endl;
        return false;
    }

    return !vertices.empty();
}

void setupBuffers() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

int main() {
    glfwInit();
    GLFWwindow* window = glfwCreateWindow(800, 600, "Cube Texturizado - Kevin", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glEnable(GL_DEPTH_TEST);

    shaderProgram = compileShader();

    if (!loadOBJWithMTL("../assets/Modelos3D/Cube.obj", "../assets/Modelos3D")) {
        std::cerr << "Erro ao carregar modelo com textura." << std::endl;
        return -1;
    }

    setupBuffers();
    glUseProgram(shaderProgram);
    glUniform1i(glGetUniformLocation(shaderProgram, "texBuff"), 0);

    mat4 projection = perspective(radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);
    mat4 view = translate(mat4(1.0f), vec3(0, 0, -5.0f));

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);

        float angle = (float)glfwGetTime();
        mat4 model = rotate(mat4(1.0f), angle, vec3(1, 1, 0));

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, value_ptr(projection));

        glDrawArrays(GL_TRIANGLES, 0, vertices.size());
        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}