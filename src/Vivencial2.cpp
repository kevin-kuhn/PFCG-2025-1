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

struct Vertex
{
    vec3 position;
    vec2 texCoord;
    vec3 normal;
};

vector<Vertex> vertices;
GLuint shaderProgram, VAO, VBO, textureID;
vec3 ka(0.1f), kd(1.0f), ks(0.5f);
float shininess = 32.0f;

// Trajetória
vector<vec3> trajectoryPoints;
int currentPointIndex = 0;
float moveSpeed = 1.0f;
vec3 objectPos(0.0f);

// Luzes
vec3 lightPositions[3];
bool lightEnabled[3] = {true, true, true};
vec3 lightColors[3] = {
    vec3(1.0f, 0.95f, 0.8f), // Key light
    vec3(0.6f, 0.6f, 0.8f),  // Fill light
    vec3(0.4f, 0.4f, 0.4f)   // Back light
};

// Câmera
class Camera
{
public:
    vec3 position, front, up, right, worldUp;
    float yaw, pitch, speed, sensitivity;

    Camera(vec3 pos) : position(pos), worldUp(0.0f, 1.0f, 0.0f),
                       yaw(-90.0f), pitch(0.0f), speed(2.5f), sensitivity(0.1f)
    {
        updateCameraVectors();
    }

    mat4 GetViewMatrix() { return lookAt(position, position + front, up); }

    void Move(string dir, float dt)
    {
        float v = speed * dt;
        if (dir == "FORWARD")
            position += front * v;
        if (dir == "BACKWARD")
            position -= front * v;
        if (dir == "LEFT")
            position -= right * v;
        if (dir == "RIGHT")
            position += right * v;
    }

    void Rotate(float x, float y)
    {
        x *= sensitivity;
        y *= sensitivity;
        yaw += x;
        pitch += y;
        pitch = clamp(pitch, -89.0f, 89.0f);
        updateCameraVectors();
    }

private:
    void updateCameraVectors()
    {
        vec3 f;
        f.x = cos(radians(yaw)) * cos(radians(pitch));
        f.y = sin(radians(pitch));
        f.z = sin(radians(yaw)) * cos(radians(pitch));
        front = normalize(f);
        right = normalize(cross(front, worldUp));
        up = normalize(cross(right, front));
    }
};

Camera camera(vec3(0, 0, 5));
float deltaTime = 0.0f, lastFrame = 0.0f;
float lastX = 400.0f, lastY = 300.0f;
bool firstMouse = true;

string to_string(const vec3 &v)
{
    return "(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) + ")";
}

void updateLightPositions()
{
    lightPositions[0] = objectPos + vec3(2.0f, 2.0f, 2.0f);  // Key
    lightPositions[1] = objectPos + vec3(-2.0f, 1.0f, 1.5f); // Fill
    lightPositions[2] = objectPos + vec3(0.0f, 2.0f, -2.5f); // Back
}

void updateTrajectory(float dt)
{
    if (trajectoryPoints.empty())
        return;
    vec3 target = trajectoryPoints[currentPointIndex];
    vec3 dir = normalize(target - objectPos);
    float dist = length(target - objectPos);
    float step = moveSpeed * dt;

    if (step >= dist)
    {
        objectPos = target;
        currentPointIndex = (currentPointIndex + 1) % trajectoryPoints.size();
    }
    else
    {
        objectPos += dir * step;
    }
}

// Vertex Shader (igual ao seu atual)
const char *vertexShaderSource = R"(
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

// Fragment Shader com 3 luzes e atenuação
const char *fragmentShaderSource = R"(
    #version 450 core
    
    in vec3 FragPos;
    in vec3 Normal;
    in vec2 TexCoord;
    
    out vec4 FragColor;
    
    uniform sampler2D texBuff;
    uniform vec3 ka, kd, ks;
    uniform float shininess;
    uniform vec3 viewPos;
    
    uniform vec3 lightPos[3];
    uniform vec3 lightColor[3];
    uniform bool lightOn[3];
    
    void main() {
        vec3 ambient = vec3(0.0);
        vec3 diffuse = vec3(0.0);
        vec3 specular = vec3(0.0);
    
        vec3 norm = normalize(Normal);
        vec3 viewDir = normalize(viewPos - FragPos);
        vec3 texColor = texture(texBuff, TexCoord).rgb;
    
        for (int i = 0; i < 3; i++) {
            if (!lightOn[i]) continue;
    
            vec3 lightDir = normalize(lightPos[i] - FragPos);
            float distance = length(lightPos[i] - FragPos);
            float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);
    
            vec3 ambientComponent = ka * lightColor[i] * texColor * attenuation;
            float diff = max(dot(norm, lightDir), 0.0);
            vec3 diffuseComponent = kd * diff * lightColor[i] * texColor * attenuation;
    
            vec3 reflectDir = reflect(-lightDir, norm);
            float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
            vec3 specularComponent = ks * spec * lightColor[i] * attenuation;
    
            ambient += ambientComponent;
            diffuse += diffuseComponent;
            specular += specularComponent;
        }
    
        vec3 result = ambient + diffuse + specular;
        FragColor = vec4(result, 1.0);
    })";

GLuint compileShader()
{
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexShaderSource, nullptr);
    glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fs);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    camera.Rotate(xoffset, yoffset);
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

bool loadOBJWithMTL(const string &objPath, const string &mtlDir)
{
    ifstream objFile(objPath);
    if (!objFile.is_open())
        return false;
    vector<vec3> positions, normals;
    vector<vec2> texCoords;
    map<string, string> materials;
    string activeTexture;
    string line;

    while (getline(objFile, line))
    {
        istringstream iss(line);
        string prefix;
        iss >> prefix;
        if (prefix == "v")
        {
            float x, y, z;
            iss >> x >> y >> z;
            positions.emplace_back(x, y, z);
        }
        else if (prefix == "vt")
        {
            float u, v;
            iss >> u >> v;
            texCoords.emplace_back(u, v);
        }
        else if (prefix == "vn")
        {
            float nx, ny, nz;
            iss >> nx >> ny >> nz;
            normals.emplace_back(nx, ny, nz);
        }
        else if (prefix == "f")
        {
            string v1, v2, v3;
            iss >> v1 >> v2 >> v3;
            string verts[] = {v1, v2, v3};
            for (string &v : verts)
            {
                size_t s1 = v.find('/'), s2 = v.find('/', s1 + 1);
                int i = stoi(v.substr(0, s1)) - 1;
                int j = stoi(v.substr(s1 + 1, s2 - s1 - 1)) - 1;
                int k = stoi(v.substr(s2 + 1)) - 1;
                vertices.push_back({positions[i], texCoords[j], normals[k]});
            }
        }
        else if (prefix == "mtllib")
        {
            string mtlFile;
            iss >> mtlFile;
            ifstream mtl(mtlDir + "/" + mtlFile);
            if (mtl.is_open())
            {
                string mline, matName;
                while (getline(mtl, mline))
                {
                    istringstream mss(mline);
                    string tag;
                    mss >> tag;
                    if (tag == "newmtl")
                        mss >> matName;
                    else if (tag == "map_Kd")
                        mss >> materials[matName];
                    else if (tag == "Ka")
                        mss >> ka.r >> ka.g >> ka.b;
                    else if (tag == "Kd")
                        mss >> kd.r >> kd.g >> kd.b;
                    else if (tag == "Ks")
                        mss >> ks.r >> ks.g >> ks.b;
                    else if (tag == "Ns")
                        mss >> shininess;
                }
            }
        }
        else if (prefix == "usemtl")
        {
            string name;
            iss >> name;
            activeTexture = materials[name];
        }
    }

    if (!activeTexture.empty())
        textureID = loadTexture(mtlDir + "/" + activeTexture);
    return !vertices.empty();
}

void setupBuffers()
{
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, position));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, texCoord));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.Move("FORWARD", deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.Move("BACKWARD", deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.Move("LEFT", deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.Move("RIGHT", deltaTime);

    static double lastToggleTime = 0.0;
    double currentTime = glfwGetTime();
    double debounceTime = 0.2; // 200ms debounce

    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS && currentTime - lastToggleTime > debounceTime) {
        lightEnabled[0] = !lightEnabled[0];
        cout << "Key Light: " << (lightEnabled[0] ? "ON" : "OFF") << endl;
        lastToggleTime = currentTime;
    }
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS && currentTime - lastToggleTime > debounceTime) {
        lightEnabled[1] = !lightEnabled[1];
        cout << "Fill Light: " << (lightEnabled[1] ? "ON" : "OFF") << endl;
        lastToggleTime = currentTime;
    }
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS && currentTime - lastToggleTime > debounceTime) {
        lightEnabled[2] = !lightEnabled[2];
        cout << "Back Light: " << (lightEnabled[2] ? "ON" : "OFF") << endl;
        lastToggleTime = currentTime;
    }

    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS && currentTime - lastToggleTime > debounceTime) {
        vec3 point = camera.position + camera.front * 3.0f;
        trajectoryPoints.push_back(point);
        cout << "Ponto adicionado: " << to_string(point) << endl;
        lastToggleTime = currentTime;
    }
}


int main()
{
    glfwInit();
    GLFWwindow *window = glfwCreateWindow(800, 600, "Carolina Prates, Kevin Kuhn e Vítor Mello", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouse_callback);
    glEnable(GL_DEPTH_TEST);

    shaderProgram = compileShader();
    loadOBJWithMTL("../assets/Modelos3D/Cube.obj", "../assets/Modelos3D");
    setupBuffers();
    glUseProgram(shaderProgram);
    glUniform1i(glGetUniformLocation(shaderProgram, "texBuff"), 0);

    mat4 projection = perspective(radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);
        updateTrajectory(deltaTime);
        updateLightPositions();
        glfwPollEvents();
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        mat4 view = camera.GetViewMatrix();
        mat4 model = translate(mat4(1.0f), objectPos);
        // model = rotate(model, currentFrame, vec3(1, 1, 0));

        glUseProgram(shaderProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glBindVertexArray(VAO);

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, value_ptr(projection));
        glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, value_ptr(camera.position));
        glUniform3fv(glGetUniformLocation(shaderProgram, "ka"), 1, value_ptr(ka));
        glUniform3fv(glGetUniformLocation(shaderProgram, "kd"), 1, value_ptr(kd));
        glUniform3fv(glGetUniformLocation(shaderProgram, "ks"), 1, value_ptr(ks));
        glUniform1f(glGetUniformLocation(shaderProgram, "shininess"), shininess);

        for (int i = 0; i < 3; i++)
        {
            string idx = to_string(i);
            glUniform3fv(glGetUniformLocation(shaderProgram, ("lightPos[" + idx + "]").c_str()), 1, value_ptr(lightPositions[i]));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("lightColor[" + idx + "]").c_str()), 1, value_ptr(lightColors[i]));
            glUniform1i(glGetUniformLocation(shaderProgram, ("lightOn[" + idx + "]").c_str()), lightEnabled[i]);
        }

        glDrawArrays(GL_TRIANGLES, 0, vertices.size());

        for (const vec3 &point : trajectoryPoints)
        {
            mat4 markerModel = translate(mat4(1.0f), point);
            markerModel = scale(markerModel, vec3(0.1f));
            glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, value_ptr(markerModel));
            glDrawArrays(GL_TRIANGLES, 0, vertices.size());
        }

        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}
