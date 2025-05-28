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
    vec3 normal;
};

vector<Vertex> vertices;
GLuint shaderProgram, VAO, VBO, textureID;
vec3 ka(0.1f), kd(1.0f), ks(0.5f);
float shininess = 32.0f;

class Camera {
public:
    vec3 position, front, up, right, worldUp;
    float yaw, pitch, speed, sensitivity;

    Camera(vec3 pos) : position(pos), worldUp(0.0f, 1.0f, 0.0f), yaw(-90.0f), pitch(0.0f), speed(2.5f), sensitivity(0.1f) {
        updateCameraVectors();
    }

    mat4 GetViewMatrix() {
        return lookAt(position, position + front, up);
    }

    void Move(string dir, float deltaTime) {
        float velocity = speed * deltaTime;
        if (dir == "FORWARD")  position += front * velocity;
        if (dir == "BACKWARD") position -= front * velocity;
        if (dir == "LEFT")     position -= right * velocity;
        if (dir == "RIGHT")    position += right * velocity;
    }

    void Rotate(float xoffset, float yoffset) {
        xoffset *= sensitivity;
        yoffset *= sensitivity;
        yaw += xoffset;
        pitch += yoffset;
        pitch = clamp(pitch, -89.0f, 89.0f);
        updateCameraVectors();
    }

private:
    void updateCameraVectors() {
        vec3 f;
        f.x = cos(radians(yaw)) * cos(radians(pitch));
        f.y = sin(radians(pitch));
        f.z = sin(radians(yaw)) * cos(radians(pitch));
        front = normalize(f);
        right = normalize(cross(front, worldUp));
        up = normalize(cross(right, front));
    }
};

Camera camera(vec3(0.0f, 0.0f, 5.0f));
float deltaTime = 0.0f, lastFrame = 0.0f;
float lastX = 400.0f, lastY = 300.0f;
bool firstMouse = true;

const char* vertexShaderSource = R"(
#version 450 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec3 normal;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    FragPos = vec3(model * vec4(position, 1.0));
    Normal = mat3(transpose(inverse(model))) * normal;
    TexCoord = texCoord;
    gl_Position = projection * view * vec4(FragPos, 1.0);
})";

const char* fragmentShaderSource = R"(
#version 450 core
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;

uniform sampler2D texBuff;
uniform vec3 ka, kd, ks;
uniform float shininess;
uniform vec3 lightPos;
uniform vec3 viewPos;

void main() {
    vec3 ambient = ka * texture(texBuff, TexCoord).rgb;
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = kd * diff * texture(texBuff, TexCoord).rgb;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specular = ks * spec;
    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, 1.0);
})";

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
    if (!data) return 0;
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
    vector<vec3> positions, normals;
    vector<vec2> texCoords;
    map<string, string> materials;
    string activeTexture;
    string line;

    while (getline(objFile, line)) {
        istringstream iss(line);
        string prefix;
        iss >> prefix;
        if (prefix == "v") {
            float x, y, z; iss >> x >> y >> z;
            positions.emplace_back(x, y, z);
        } else if (prefix == "vt") {
            float u, v; iss >> u >> v;
            texCoords.emplace_back(u, v);
        } else if (prefix == "vn") {
            float nx, ny, nz; iss >> nx >> ny >> nz;
            normals.emplace_back(nx, ny, nz);
        } else if (prefix == "f") {
            string v1, v2, v3; iss >> v1 >> v2 >> v3;
            string verts[] = { v1, v2, v3 };
            for (string& v : verts) {
                size_t s1 = v.find('/'), s2 = v.find('/', s1 + 1);
                int i = stoi(v.substr(0, s1)) - 1;
                int j = stoi(v.substr(s1 + 1, s2 - s1 - 1)) - 1;
                int k = stoi(v.substr(s2 + 1)) - 1;
                vertices.push_back({ positions[i], texCoords[j], normals[k] });
            }
        } else if (prefix == "mtllib") {
            string mtlFile; iss >> mtlFile;
            ifstream mtl(mtlDir + "/" + mtlFile);
            if (mtl.is_open()) {
                string mline, matName;
                while (getline(mtl, mline)) {
                    istringstream mss(mline);
                    string tag; mss >> tag;
                    if (tag == "newmtl") mss >> matName;
                    else if (tag == "map_Kd") mss >> materials[matName];
                    else if (tag == "Ka") mss >> ka.r >> ka.g >> ka.b;
                    else if (tag == "Kd") mss >> kd.r >> kd.g >> kd.b;
                    else if (tag == "Ks") mss >> ks.r >> ks.g >> ks.b;
                    else if (tag == "Ns") mss >> shininess;
                }
            }
        } else if (prefix == "usemtl") {
            string name; iss >> name;
            activeTexture = materials[name];
        }
    }

    if (!activeTexture.empty())
        textureID = loadTexture(mtlDir + "/" + activeTexture);
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
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
}

void mouse_callback(GLFWwindow*, double xpos, double ypos) {
    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;
    camera.Rotate(xoffset, yoffset);
}

void processInput(GLFWwindow* window) {
    float currentFrame = glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.Move("FORWARD", deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.Move("BACKWARD", deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.Move("LEFT", deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.Move("RIGHT", deltaTime);
}

int main() {
    glfwInit();
    GLFWwindow* window = glfwCreateWindow(800, 600, "Phong + Camera - Kevin", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouse_callback);
    glEnable(GL_DEPTH_TEST);

    shaderProgram = compileShader();
    if (!loadOBJWithMTL("../assets/Modelos3D/Cube.obj", "../assets/Modelos3D")) {
        cerr << "Erro ao carregar modelo." << endl;
        return -1;
    }

    setupBuffers();
    glUseProgram(shaderProgram);
    glUniform1i(glGetUniformLocation(shaderProgram, "texBuff"), 0);

    mat4 projection = perspective(radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);

    while (!glfwWindowShouldClose(window)) {
        processInput(window);
        glfwPollEvents();
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float angle = (float)glfwGetTime();
        mat4 model = rotate(mat4(1.0f), angle, vec3(1, 1, 0));
        mat4 view = camera.GetViewMatrix();

        glUseProgram(shaderProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glBindVertexArray(VAO);

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, value_ptr(projection));

        glUniform3fv(glGetUniformLocation(shaderProgram, "ka"), 1, value_ptr(ka));
        glUniform3fv(glGetUniformLocation(shaderProgram, "kd"), 1, value_ptr(kd));
        glUniform3fv(glGetUniformLocation(shaderProgram, "ks"), 1, value_ptr(ks));
        glUniform1f(glGetUniformLocation(shaderProgram, "shininess"), shininess);
        glUniform3f(glGetUniformLocation(shaderProgram, "lightPos"), 3.0f, 3.0f, 3.0f);
        glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, value_ptr(camera.position));

        glDrawArrays(GL_TRIANGLES, 0, vertices.size());
        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}