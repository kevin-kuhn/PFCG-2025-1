
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

#include <string>
#include <algorithm>
#include <unordered_map>
#include <cctype>

map<string, string> config;

struct Vertex {
    vec3 position;
    vec2 texCoord;
    vec3 normal;
};

struct Material {
    vec3 ka = vec3(0.1f);
    vec3 kd = vec3(1.0f);
    vec3 ks = vec3(0.5f);
    float shininess = 32.0f;
};

struct Submesh {
    vector<Vertex> vertices;
    GLuint VAO, VBO, textureID;
    int vertexCount;
    Material material;
};

struct Modelo {
    GLuint VAO = 0, VBO = 0, textura = 0;
    int vertexCount = 0;
    Material material;
    std::vector<Submesh> partes;
};

vec3 ka(0.1f), kd(1.0f), ks(0.5f);
float shininess = 32.0f;
vec3 lightColor;
vec3 lightPos;
GLuint shaderProgram;
Modelo ovni, vaca, casa, chao;
GLuint skyboxTexture, quadVAO;
GLuint skyboxShader;

// ============== CONFIGURATION LOADER ==============
void loadConfig(const string& filename) {
    ifstream file(filename);
    string line;
    string section;
    while (getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        if (line[0] == '[') {
            section = line.substr(1, line.find(']') - 1);
            continue;
        }

        size_t eq = line.find('=');
        if (eq == string::npos) continue;

        string key = line.substr(0, eq);
        string value = line.substr(eq + 1);

        string fullKey = section.empty() ? key : section + "." + key;
        config[fullKey] = value;
    }
}

float getFloat(const string& key, float def) {
    return config.count(key) ? stof(config[key]) : def;
}

vec3 getVec3(const string& key, vec3 def) {
    if (!config.count(key)) return def;
    stringstream ss(config[key]);
    float x, y, z;
    char sep; // ignora vírgulas
    ss >> x >> sep >> y >> sep >> z;
    return vec3(x, y, z);
}

string getString(const string& key, const string& def) {
    return config.count(key) ? config[key] : def;
}

bool getBool(const std::string& key, bool def) {
    if (config.count(key) == 0) return def;

    std::string val = config[key];
    std::transform(val.begin(), val.end(), val.begin(), ::tolower);

    return (val == "true");
}

bool casaLuz = getFloat("estado_inicial.casa_luz", true);

// ============== CAMERA ==============
class Camera
{
public:
    vec3 position, front, up, right, worldUp;
    float yaw, pitch, speed, sensitivity;

    Camera(vec3 pos) : position(pos), worldUp(0.0f, 1.0f, 0.0f), yaw(-90.0f), pitch(0.0f), speed(2.5f), sensitivity(0.1f)
    {
        updateCameraVectors();
    }

    mat4 GetViewMatrix()
    {
        return lookAt(position, position + front, up);
    }

    void Move(string dir, float deltaTime)
    {
        float velocity = speed * deltaTime;
        if (dir == "FORWARD")
            position += front * velocity;
        if (dir == "BACKWARD")
            position -= front * velocity;
        if (dir == "LEFT")
            position -= right * velocity;
        if (dir == "RIGHT")
            position += right * velocity;
    }

    void Rotate(float xoffset, float yoffset)
    {
        xoffset *= sensitivity;
        yoffset *= sensitivity;
        yaw += xoffset;
        pitch += yoffset;
        pitch = glm::clamp(pitch, -89.0f, 89.0f);
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

Camera camera(vec3(0.0f, 1.5f, 10.0f));
float deltaTime = 0.0f, lastFrame = 0.0f;
float lastX = 400.0f, lastY = 300.0f;

// ============== SHADERS ==============
const char *vertexShaderSource = R"(
    #version 450 core
    layout(location = 0) in vec3 position;
    layout(location = 1) in vec2 texCoord;
    layout(location = 2) in vec3 normal;

    out vec3 FragPos;
    out vec3 Normal;
    out vec2 TexCoord;

    uniform mat4 model; // transformações do objeto
    uniform mat4 view; // câmera
    uniform mat4 projection; // perspectiva

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
    uniform vec3 viewPos;

    // luz do ovni
    uniform vec3 lightPos;
    uniform vec3 lightColor;
    uniform vec3 lightDir;

    void main() {
    vec3 baseColor = texture(texBuff, TexCoord).rgb;
    vec3 ambient = ka * baseColor;
    vec3 norm = normalize(Normal);
    vec3 lightDirection = normalize(lightPos - FragPos);

    // Spot cutoff
    float theta = dot(lightDirection, normalize(-lightDir)); // ângulo do cone spotlight
    float cutoff = 0.85; // ângulo central
    float outerCutoff = 0.70; // ângulo externo
    float intensity = clamp((theta - outerCutoff) / (cutoff - outerCutoff), 0.0, 1.0);

    // Componente difusa (baseada no ângulo entre luz e normal)
    float diff = max(dot(norm, lightDirection), 0.0);
    vec3 diffuse = kd * diff * baseColor * lightColor * intensity;

    // Vetor da câmera até o fragmento
    vec3 viewDir = normalize(viewPos - FragPos);

    // Direção do reflexo da luz em relação à normal 
    vec3 reflectDir = reflect(-lightDirection, norm);

    // Componente especular (brilho)
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specColor = lightColor * 0.4 + vec3(0.2);
    vec3 specular = ks * spec * specColor * intensity;

    // Atenuação da luz com base na distância do ponto à fonte de luz
    float distance = length(lightPos - FragPos);
    float attenuation = 1.0 / (1.0 + 0.05 * distance + 0.01 * distance * distance);

    vec3 result = (ambient + diffuse + specular) * attenuation * 2.0;
    FragColor = vec4(result, 1.0);
})";

const char *skyboxVertex = R"(
    #version 450 core
    out vec2 TexCoord;
    void main() {
        vec2 pos[6] = vec2[](
            vec2(-1, 1), vec2(-1, -1), vec2(1, -1),
            vec2(-1, 1), vec2(1, -1), vec2(1, 1)
        );
        vec2 tex[6] = vec2[](
            vec2(0, 1), vec2(0, 0), vec2(1, 0),
            vec2(0, 1), vec2(1, 0), vec2(1, 1)
        );
        gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);
        TexCoord = tex[gl_VertexID];
})";

const char *skyboxFragment = R"(
    #version 450 core
    in vec2 TexCoord;
    out vec4 FragColor;
    uniform sampler2D skyTexture;
    void main() {
        FragColor = texture(skyTexture, TexCoord);
})";

GLuint compileSkyboxShader()
{
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &skyboxVertex, NULL);
    glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &skyboxFragment, NULL);
    glCompileShader(fs);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

GLuint compileShader()
{
    GLuint v = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(v, 1, &vertexShaderSource, NULL);
    glCompileShader(v);
    GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(f, 1, &fragmentShaderSource, NULL);
    glCompileShader(f);
    GLuint program = glCreateProgram();
    glAttachShader(program, v);
    glAttachShader(program, f);
    glLinkProgram(program);
    glDeleteShader(v);
    glDeleteShader(f);
    return program;
}

GLuint loadTexture(const string &path) {
    GLuint textureID;
    glGenTextures(1, &textureID);
    int width, height, nrChannels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);

    if (data) {
        GLenum format;
        if (nrChannels == 1)
            format = GL_RED;
        else if (nrChannels == 3)
            format = GL_RGB;
        else if (nrChannels == 4)
            format = GL_RGBA;
        else {
            std::cerr << "Unsupported channel count: " << nrChannels << " in texture " << path << std::endl;
            stbi_image_free(data);
            return 0;
        }

        glBindTexture(GL_TEXTURE_2D, textureID);

        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    } else {
        std::cerr << "Failed to load texture: " << path << std::endl;
        return 0;
    }

    return textureID;
}

void initSkybox()
{
    glGenVertexArrays(1, &quadVAO);
    skyboxTexture = loadTexture(getString("texturas.textura_ceu", "../assets/Modelos3D/final/ceu.png"));
    skyboxShader = compileSkyboxShader();
}

bool loadOBJWithMTL(const string& objPath, const string& mtlDir, vector<Submesh>& submeshes)
{
    ifstream file(objPath);
    if (!file.is_open())
        return false;

    vector<vec3> positions, normals;
    vector<vec2> texCoords;
    map<string, string> texturesPorMaterial;
    map<string, Material> materiais;

    string activeMaterial;
    Submesh submeshAtual;
    string line;

    while (getline(file, line))
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
            texCoords.emplace_back(u, 1.0f - v);
        }
        else if (prefix == "vn")
        {
            float x, y, z;
            iss >> x >> y >> z;
            normals.emplace_back(x, y, z);
        }
        else if (prefix == "f")
        {
            string v1, v2, v3;
            iss >> v1 >> v2 >> v3;
            string vs[] = {v1, v2, v3};
            for (auto& v : vs)
            {
                int p = 0, t = 0, n = 0;
                sscanf(v.c_str(), "%d/%d/%d", &p, &t, &n);
                submeshAtual.vertices.push_back({positions[p - 1], texCoords[t - 1], normals[n - 1]});
            }
        }
        else if (prefix == "mtllib")
        {
            string mtlFile;
            iss >> mtlFile;
            ifstream mtl(mtlDir + "/" + mtlFile);
            string mline, matName;

            while (getline(mtl, mline))
            {
                istringstream mss(mline);
                string tag;
                mss >> tag;

                if (tag == "newmtl") {
                    mss >> matName;
                    materiais[matName] = Material(); // inicia material
                }
                else if (tag == "Ka")
                    mss >> materiais[matName].ka.r >> materiais[matName].ka.g >> materiais[matName].ka.b;
                else if (tag == "Kd")
                    mss >> materiais[matName].kd.r >> materiais[matName].kd.g >> materiais[matName].kd.b;
                else if (tag == "Ks")
                    mss >> materiais[matName].ks.r >> materiais[matName].ks.g >> materiais[matName].ks.b;
                else if (tag == "Ns")
                    mss >> materiais[matName].shininess;
                else if (tag == "map_Kd")
                    mss >> texturesPorMaterial[matName];
            }
        }
        else if (prefix == "usemtl")
        {
            if (!submeshAtual.vertices.empty()) {
                // Finaliza o submesh atual
                submeshAtual.vertexCount = submeshAtual.vertices.size();
                submeshes.push_back(submeshAtual);
                submeshAtual = Submesh();
            }

            string mtlName;
            iss >> mtlName;
            activeMaterial = mtlName;
            submeshAtual.material = materiais[mtlName];

            if (texturesPorMaterial.count(mtlName))
                submeshAtual.textureID = loadTexture(mtlDir + "/" + texturesPorMaterial[mtlName]);
            else
                submeshAtual.textureID = 0;
        }
    }

    // Adiciona último submesh
    if (!submeshAtual.vertices.empty()) {
        submeshAtual.vertexCount = submeshAtual.vertices.size();
        submeshes.push_back(submeshAtual);
    }

    // Cria os VAOs e VBOs para cada submesh
    for (Submesh& s : submeshes) {
        glGenVertexArrays(1, &s.VAO);
        glGenBuffers(1, &s.VBO);
        glBindVertexArray(s.VAO);
        glBindBuffer(GL_ARRAY_BUFFER, s.VBO);
        glBufferData(GL_ARRAY_BUFFER, s.vertices.size() * sizeof(Vertex), s.vertices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
        glEnableVertexAttribArray(2);
    }

    return !submeshes.empty();
}

bool loadModel(const string& path, Modelo& modelo) {
    modelo.partes.clear(); // limpa se já existia algo

    if (!loadOBJWithMTL(path, "../assets/Modelos3D/final", modelo.partes)) {
        cerr << "Erro ao carregar modelo: " << path << endl;
        return false;
    }

    modelo.vertexCount = 0;
    for (Submesh& sub : modelo.partes) {
        modelo.vertexCount += sub.vertexCount;
    }

    return true;
}

void mouse_callback(GLFWwindow *, double xpos, double ypos)
{
    static bool first = true;
    if (first)
    {
        lastX = xpos;
        lastY = ypos;
        first = false;
    }
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;
    camera.Rotate(xoffset, yoffset);
}

void processInput(GLFWwindow *window)
{
    float current = glfwGetTime();
    deltaTime = current - lastFrame;
    lastFrame = current;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.Move("FORWARD", deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.Move("BACKWARD", deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.Move("LEFT", deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.Move("RIGHT", deltaTime);
    if (glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS)
    {
        casaLuz = !casaLuz;
        glfwWaitEventsTimeout(0.1);
    }
}

void carregarJanela(GLFWwindow*& w) {
    int width = (int)getFloat("window.width", 800);
    int height = (int)getFloat("window.height", 600);
    string title = getString("window.title", "OVNI vs Vaca");

    w = glfwCreateWindow(width, height, title.c_str(), NULL, NULL);
}

void drawChao(const Modelo& chao, const mat4& model) {
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, value_ptr(model));
    glUniform3fv(glGetUniformLocation(shaderProgram, "ka"), 1, value_ptr(chao.material.ka));
    glUniform3fv(glGetUniformLocation(shaderProgram, "kd"), 1, value_ptr(chao.material.kd));
    glUniform3fv(glGetUniformLocation(shaderProgram, "ks"), 1, value_ptr(chao.material.ks));
    glUniform1f(glGetUniformLocation(shaderProgram, "shininess"), chao.material.shininess);

    glBindVertexArray(chao.VAO);
    glActiveTexture(GL_TEXTURE0); // ATIVA UNIDADE 0
    glBindTexture(GL_TEXTURE_2D, chao.textura);
    glUniform1i(glGetUniformLocation(shaderProgram, "texBuff"), 0);
    glDrawArrays(GL_TRIANGLES, 0, chao.vertexCount);
}

int main() {
    glfwInit();
    loadConfig("config.ini");
    GLFWwindow* w;
    carregarJanela(w);
    glfwMakeContextCurrent(w);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(w, mouse_callback);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f, 2.0f);

    shaderProgram = compileShader();

    float alturaAbducao = getFloat("alturas.abducao", 5.0f);
    float alturaFuga = getFloat("alturas.fuga", 15.0f);
    float curvaAmplitude = getFloat("curvas.amplitude", 1.0f); // raio da curva no plano XZ

    initSkybox();

    loadModel(getString("modelo_paths.ovni", "../assets/Modelos3D/final/Nave.obj"), ovni);
    loadModel(getString("modelo_paths.vaca", "../assets/Modelos3D/final/vaca.obj"), vaca);
    loadModel(getString("modelo_paths.casa", "../assets/Modelos3D/final/casa.obj"), casa);

    // ==== CHÃO ====
    vector<Vertex> chaoVerts = {
        {{-50.0f, 0.0f, -50.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{ 50.0f, 0.0f, -50.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{ 50.0f, 0.0f,  50.0f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
        {{-50.0f, 0.0f, -50.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{ 50.0f, 0.0f,  50.0f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
        {{-50.0f, 0.0f,  50.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
    };
    chao.vertexCount = chaoVerts.size();
    chao.textura = loadTexture(getString("texturas.textura_chao", "../assets/Modelos3D/final/grama.png"));
    chao.material.ka = getVec3("chao_ka", vec3(0.2f));
    chao.material.kd = getVec3("chao_kd", vec3(0.8f));
    chao.material.ks = getVec3("chao_ks", vec3(0.1f));
    chao.material.shininess = getFloat("chao_shininess", 8.0f);
    glGenVertexArrays(1, &chao.VAO);
    glGenBuffers(1, &chao.VBO);
    glBindVertexArray(chao.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, chao.VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * chaoVerts.size(), chaoVerts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);

    // ==== ESTADOS INICIAIS ====
    const float ovniTopo = getFloat("estado_inicial.ovni_topo", (alturaFuga + 5.0f));
    const float ovniBaixo = getFloat("estado_inicial.ovni_baixo", (alturaAbducao + 1.5f));
    float ovniY = getFloat("estado_inicial.ovniY", ovniTopo);
    float vacaY = getFloat("estado_inicial.vacaY", 0.0f);
    float vacaX = getFloat("estado_inicial.vacaX", 0.0f);
    float vacaRot = getFloat("estado_inicial.vaca_rot", 0.0f);

    while (!glfwWindowShouldClose(w)) {
        processInput(w);
        glfwPollEvents();
        float t = glfwGetTime();

        float baseSpeed = 1.5f;
        float vacaDist = alturaAbducao - vacaY;
        float ovniDist = ovniY - ovniBaixo;
        float ovniSpeed = (vacaDist > 0.01f && ovniDist > 0.01f) ? baseSpeed * (ovniDist / vacaDist) : baseSpeed;

        if (casaLuz) {
            if (ovniY < ovniTopo)
                ovniY += ovniSpeed * deltaTime;
            else
                ovniY = ovniTopo;
            if (vacaY > 0.0f) {
                vacaY -= baseSpeed * deltaTime;

                // Aplica rotação decrescente na vaca durante a queda
                if (vacaY < alturaAbducao) {
                    vacaRot += 5.0f * deltaTime;  // controla a velocidade da rotação
                    if (vacaRot > glm::radians(720.0f)) // no máximo 2 voltas
                        vacaRot = glm::radians(720.0f);
                }
            } else {
                vacaY = 0.0f;
                vacaRot = 0.0f; // reseta rotação quando toca o chão
            }            
        } else {
            if (ovniY > ovniBaixo)
                ovniY -= ovniSpeed * deltaTime;
            else
                ovniY = ovniBaixo;

            if (vacaY < alturaAbducao) {
                vacaY += baseSpeed * deltaTime;
                vacaX = sin(t * 2.0f) * 0.5f; // curva suave em X
            } else {
                vacaY = alturaAbducao;
                vacaX = 0.0f;
            }
        }

        // ==== DESENHO DO FUNDO ====
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
        glUseProgram(skyboxShader);
        glBindVertexArray(quadVAO);
        glBindTexture(GL_TEXTURE_2D, skyboxTexture);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glEnable(GL_DEPTH_TEST);

        // ==== SHADER PRINCIPAL ====
        glUseProgram(shaderProgram);
        mat4 proj = perspective(radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);
        mat4 view = camera.GetViewMatrix();
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, value_ptr(proj));

        // === AJUSTE DE MATERIAIS E LUZ ===
        vec3 vacaPos = vec3(0, vacaY, 0);

        if (casaLuz) {
            ka = getVec3("luz_casa.ka", vec3(0.2f));
            kd = getVec3("luz_casa.kd", vec3(1.5f));
            ks = getVec3("luz_casa.ks", vec3(0.3f));
            lightColor = vec3(1.0f);
            lightPos = vec3(5.0f, 1.5f, -6.5f); // dentro da casa
            vec3 dir = normalize(vacaPos - lightPos);
            glUniform3fv(glGetUniformLocation(shaderProgram, "lightDir"), 1, value_ptr(dir));
        } else {
            ka = getVec3("luz_ovni.ka", vec3(0.05f, 0.2f, 0.05f));
            kd = getVec3("luz_ovni.kd", vec3(0.2f, 1.0f, 0.2f));
            ks = getVec3("luz_ovni.ks", vec3(0.1f, 0.8f, 0.1f));
            lightColor = vec3(0.0f, 1.0f, 0.0f);
            lightPos = vec3(0, ovniY - 1.0f, 0);
            vec3 dir = normalize(vec3(0, -1, 0));
            glUniform3fv(glGetUniformLocation(shaderProgram, "lightDir"), 1, value_ptr(dir));
        }

        glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, value_ptr(camera.position));
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos"), 1, value_ptr(lightPos));
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, value_ptr(lightColor));

        // ==== DESENHO ====
        auto draw = [&](Modelo& m, mat4 model) {
            glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, value_ptr(model));
            
            for (const Submesh& sub : m.partes) {
                glUniform3fv(glGetUniformLocation(shaderProgram, "ka"), 1, value_ptr(sub.material.ka));
                glUniform3fv(glGetUniformLocation(shaderProgram, "kd"), 1, value_ptr(sub.material.kd));
                glUniform3fv(glGetUniformLocation(shaderProgram, "ks"), 1, value_ptr(sub.material.ks));
                glUniform1f(glGetUniformLocation(shaderProgram, "shininess"), sub.material.shininess);

                glBindVertexArray(sub.VAO);
                glBindTexture(GL_TEXTURE_2D, sub.textureID);
                glDrawArrays(GL_TRIANGLES, 0, sub.vertexCount);
            }
        };

        drawChao(chao, mat4(1.0f));
        draw(ovni, translate(mat4(1.0f), vec3(0, ovniY, 0)) * rotate(mat4(1.0f), t, vec3(0, 1, 0)));
        draw(casa, translate(mat4(1.0f), vec3(5, 0, -5)));

        vec3 posVaca;
        if (!casaLuz && vacaY >= alturaAbducao) {
            float curvaT = t * 2.0f; // velocidade da curva
            float vacaX = curvaAmplitude * sin(curvaT);
            float vacaZ = curvaAmplitude * sin(curvaT) * cos(curvaT);
            posVaca = vec3(vacaX, vacaY, vacaZ);
        } else {
            posVaca = vec3(vacaX, vacaY, 0);
        }
        
        mat4 modelVaca = translate(mat4(1.0f), posVaca);
        // Aplica rotação na vaca apenas quando estiver caindo (casaLuz == true)
        if (casaLuz && vacaY < alturaAbducao && vacaY > 0.0f)
            modelVaca = modelVaca * rotate(mat4(1.0f), vacaRot, vec3(1, 0, 0));

        draw(vaca, modelVaca);

        glfwSwapBuffers(w);
    }

    glfwTerminate();
    return 0;
}