#define GLFW_INCLUDE_NONE
#define NOMINMAX
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "src/gameplay/gameplay.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <utility>

constexpr int WORLD_X = 64;
constexpr int WORLD_Y = 32;
constexpr int WORLD_Z = 64;
constexpr int CHUNK_SIZE = 16;
constexpr int CHUNKS_X = WORLD_X / CHUNK_SIZE;
constexpr int CHUNKS_Z = WORLD_Z / CHUNK_SIZE;
constexpr int CHUNK_COUNT = CHUNKS_X * CHUNKS_Z;
constexpr int HOTBAR_SIZE = 8;

int world[WORLD_X][WORLD_Y][WORLD_Z]{};
glm::vec3 player(32.0f, 12.0f, 32.0f);
glm::vec3 velocity(0.0f);
float yaw = -90.0f;
float pitch = 0.0f;
float lastX = 640.0f;
float lastY = 360.0f;
bool firstMouse = true;
bool grounded = false;
bool leftWasDown = false;
bool rightWasDown = false;
bool saveWasDown = false;
bool loadWasDown = false;
int selectedBlock = 1;
mc::Inventory inventory;
mc::PlayerStats playerStats;
std::vector<mc::ItemDrop> itemDrops;
bool playerMoving = false;
bool playerSprinting = false;
glm::ivec3 miningTarget(-999);
float miningProgress = 0.0f;
bool inventoryOpen = false;
bool inventoryWasDown = false;
bool craftWasDown = false;
bool inventoryClickWasDown = false;
bool uiClickWasDown = false;
bool pauseWasDown = false;
enum class UiScreen { Gameplay, Pause, Settings };
UiScreen uiScreen = UiScreen::Gameplay;
float mouseSensitivity = 0.105f;
float fieldOfView = 76.0f;
bool showDebugInfo = true;
double mouseX = 0.0;
double mouseY = 0.0;
std::string lastCraftMessage;
int screenWidth = 1280;
int screenHeight = 720;
float worldTime = 0.0f;

struct Vertex {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
    float ao;
};

struct ChunkMesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLsizei vertexCount = 0;
    bool dirty = true;
};

struct Plane {
    glm::vec3 normal;
    float distance;
};

std::array<ChunkMesh, CHUNK_COUNT> chunks;
GLuint cubeVAO = 0;
GLuint cubeVBO = 0;
GLuint shaderProgram = 0;
GLuint atlasTexture = 0;
GLint locView = -1;
GLint locProjection = -1;
GLint locModel = -1;
GLint locCamera = -1;
GLint locSun = -1;
GLint locFog = -1;
GLint locFogDensity = -1;
GLint locTime = -1;
GLint locTextured = -1;
GLint locSolidColor = -1;
GLint locAtlas = -1;

const char* vertexShaderSource = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
layout(location=3) in float aAO;
out vec3 vNormal;
out vec3 vWorldPos;
out vec2 vUV;
out float vAO;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uModel;
void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;
    vUV = aUV;
    vAO = aAO;
    gl_Position = uProjection * uView * worldPos;
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
in vec3 vNormal;
in vec3 vWorldPos;
in vec2 vUV;
in float vAO;
out vec4 FragColor;
uniform sampler2D uAtlas;
uniform vec3 uCameraPos;
uniform vec3 uSunDirection;
uniform vec3 uFogColor;
uniform float uFogDensity;
uniform float uTime;
uniform int uTextured;
uniform vec3 uSolidColor;
void main() {
    if (uTextured == 0) {
        FragColor = vec4(uSolidColor, 1.0);
        return;
    }
    vec4 texel = texture(uAtlas, vUV);
    vec3 n = normalize(vNormal);
    float diffuse = max(dot(n, normalize(uSunDirection)), 0.0);
    float ambient = 0.36 + 0.08 * max(n.y, 0.0);
    float daylight = 0.76 + 0.24 * sin(uTime * 0.04);
    float occlusion = mix(0.58, 1.0, clamp(vAO, 0.0, 1.0));
    vec3 lit = texel.rgb * (ambient + diffuse * 0.68) * daylight * occlusion;
    float distanceToCamera = length(vWorldPos - uCameraPos);
    float fog = 1.0 - exp(-pow(distanceToCamera * uFogDensity, 1.45));
    fog = clamp(fog, 0.0, 0.9);
    FragColor = vec4(mix(lit, uFogColor, fog), 1.0);
}
)";

const float cubeVertices[] = {
    -0.5f,-0.5f, 0.5f,  0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f,
     0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f,-0.5f, 0.5f,
     0.5f,-0.5f,-0.5f, -0.5f,-0.5f,-0.5f, -0.5f, 0.5f,-0.5f,
    -0.5f, 0.5f,-0.5f,  0.5f, 0.5f,-0.5f,  0.5f,-0.5f,-0.5f,
    -0.5f, 0.5f, 0.5f,  0.5f, 0.5f, 0.5f,  0.5f, 0.5f,-0.5f,
     0.5f, 0.5f,-0.5f, -0.5f, 0.5f,-0.5f, -0.5f, 0.5f, 0.5f,
    -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f,-0.5f, 0.5f,
     0.5f,-0.5f, 0.5f, -0.5f,-0.5f, 0.5f, -0.5f,-0.5f,-0.5f,
     0.5f,-0.5f, 0.5f,  0.5f,-0.5f,-0.5f,  0.5f, 0.5f,-0.5f,
     0.5f, 0.5f,-0.5f,  0.5f, 0.5f, 0.5f,  0.5f,-0.5f, 0.5f,
    -0.5f,-0.5f,-0.5f, -0.5f, 0.5f,-0.5f, -0.5f, 0.5f, 0.5f,
    -0.5f, 0.5f, 0.5f, -0.5f,-0.5f, 0.5f, -0.5f,-0.5f,-0.5f
};

const float faceCorners[6][4][3] = {
    {{-0.5f,-0.5f,0.5f},{0.5f,-0.5f,0.5f},{0.5f,0.5f,0.5f},{-0.5f,0.5f,0.5f}},
    {{0.5f,-0.5f,-0.5f},{-0.5f,-0.5f,-0.5f},{-0.5f,0.5f,-0.5f},{0.5f,0.5f,-0.5f}},
    {{-0.5f,0.5f,0.5f},{0.5f,0.5f,0.5f},{0.5f,0.5f,-0.5f},{-0.5f,0.5f,-0.5f}},
    {{-0.5f,-0.5f,-0.5f},{0.5f,-0.5f,-0.5f},{0.5f,-0.5f,0.5f},{-0.5f,-0.5f,0.5f}},
    {{0.5f,-0.5f,0.5f},{0.5f,-0.5f,-0.5f},{0.5f,0.5f,-0.5f},{0.5f,0.5f,0.5f}},
    {{-0.5f,-0.5f,-0.5f},{-0.5f,-0.5f,0.5f},{-0.5f,0.5f,0.5f},{-0.5f,0.5f,-0.5f}}
};

const glm::ivec3 faceNormals[6] = {
    {0,0,1},{0,0,-1},{0,1,0},{0,-1,0},{1,0,0},{-1,0,0}
};

uint32_t hashNoise(int x, int y, int seed) {
    uint32_t n = static_cast<uint32_t>(x) * 374761393u +
                 static_cast<uint32_t>(y) * 668265263u +
                 static_cast<uint32_t>(seed) * 1442695041u;
    n = (n ^ (n >> 13)) * 1274126177u;
    return n ^ (n >> 16);
}

glm::vec3 baseColor(int tile) {
    switch (tile) {
        case 0: return {0.28f,0.68f,0.17f};
        case 1: return {0.39f,0.25f,0.12f};
        case 2: return {0.48f,0.49f,0.52f};
        case 3: return {0.48f,0.27f,0.11f};
        case 4: return {0.13f,0.42f,0.12f};
        case 5: return {0.78f,0.69f,0.43f};
        case 6: return {0.88f,0.91f,0.96f};
        case 7: return {0.72f,0.48f,0.16f};
        case 8: return {0.39f,0.25f,0.12f};
        case 9: return {0.68f,0.43f,0.22f};
        default: return {0.28f,0.68f,0.17f};
    }
}

void createAtlas() {
    constexpr int tileSize = 16;
    constexpr int columns = 10;
    std::vector<unsigned char> pixels(tileSize * columns * tileSize * 4);
    for (int tile = 0; tile < columns; ++tile) {
        glm::vec3 base = baseColor(tile);
        for (int y = 0; y < tileSize; ++y) {
            for (int x = 0; x < tileSize; ++x) {
                uint32_t noise = hashNoise(x, y, tile);
                float variation = 0.82f + static_cast<float>(noise % 38) / 100.0f;
                if (tile == 4 && ((x + y * 3) % 7 == 0)) variation *= 0.48f;
                if (tile == 7 && ((x * 5 + y * 3) % 11 < 2)) variation *= 0.42f;
                if (tile == 3 && (x == 3 || x == 11 || y == 7)) variation *= 0.62f;
                if (tile == 6 && (y == 5 || y == 11)) variation *= 0.9f;
                if (tile == 8 && y < 4) base = {0.28f,0.68f,0.17f};
                int px = tile * tileSize + x;
                int index = (y * tileSize * columns + px) * 4;
                pixels[index + 0] = static_cast<unsigned char>(std::clamp(base.r * variation, 0.0f, 1.0f) * 255);
                pixels[index + 1] = static_cast<unsigned char>(std::clamp(base.g * variation, 0.0f, 1.0f) * 255);
                pixels[index + 2] = static_cast<unsigned char>(std::clamp(base.b * variation, 0.0f, 1.0f) * 255);
                pixels[index + 3] = 255;
            }
        }
    }
    glGenTextures(1, &atlasTexture);
    glBindTexture(GL_TEXTURE_2D, atlasTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tileSize * columns, tileSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048]{};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "Shader compile error: " << log << '\n';
    }
    return shader;
}

void createShader() {
    GLuint vertex = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertex);
    glAttachShader(shaderProgram, fragment);
    glLinkProgram(shaderProgram);
    GLint ok = GL_FALSE;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048]{};
        glGetProgramInfoLog(shaderProgram, sizeof(log), nullptr, log);
        std::cerr << "Shader link error: " << log << '\n';
    }
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    locView = glGetUniformLocation(shaderProgram, "uView");
    locProjection = glGetUniformLocation(shaderProgram, "uProjection");
    locModel = glGetUniformLocation(shaderProgram, "uModel");
    locCamera = glGetUniformLocation(shaderProgram, "uCameraPos");
    locSun = glGetUniformLocation(shaderProgram, "uSunDirection");
    locFog = glGetUniformLocation(shaderProgram, "uFogColor");
    locFogDensity = glGetUniformLocation(shaderProgram, "uFogDensity");
    locTime = glGetUniformLocation(shaderProgram, "uTime");
    locTextured = glGetUniformLocation(shaderProgram, "uTextured");
    locSolidColor = glGetUniformLocation(shaderProgram, "uSolidColor");
    locAtlas = glGetUniformLocation(shaderProgram, "uAtlas");
}

int biomeAt(int x, int z) {
    float temperature = std::sin(x * 0.052f) + std::cos(z * 0.047f);
    float moisture = std::sin((x + z) * 0.061f) + std::cos((x - z) * 0.039f);
    if (temperature > 0.62f && moisture < -0.05f) return 1;
    if (temperature < -0.55f) return 2;
    if (moisture > 0.35f) return 3;
    return 0;
}

int terrainHeight(int x, int z) {
    float broad = std::sin(x * 0.105f) * 2.8f + std::cos(z * 0.12f) * 2.4f;
    float detail = std::sin((x + z) * 0.23f) * 1.15f + std::cos((x - z) * 0.19f) * 0.8f;
    return std::clamp(7 + static_cast<int>(std::round(broad + detail)), 3, 17);
}

bool insideWorld(int x, int y, int z) {
    return x >= 0 && x < WORLD_X && y >= 0 && y < WORLD_Y && z >= 0 && z < WORLD_Z;
}

void generateWorld() {
    for (int x = 0; x < WORLD_X; ++x) {
        for (int z = 0; z < WORLD_Z; ++z) {
            int top = terrainHeight(x, z);
            int biome = biomeAt(x, z);
            for (int y = 0; y <= top; ++y) {
                if (y == top) {
                    if (biome == 1) world[x][y][z] = 6;
                    else if (biome == 2) world[x][y][z] = 7;
                    else world[x][y][z] = 1;
                } else if (y >= top - 3) {
                    world[x][y][z] = biome == 1 ? 6 : 2;
                } else {
                    float caveA = std::sin(x * 0.23f + y * 0.37f) * std::cos(z * 0.19f - y * 0.28f);
                    float caveB = std::sin((x + z) * 0.12f + y * 0.31f);
                    bool cave = y > 3 && y < top - 3 && caveA + caveB > 0.98f;
                    if (cave) world[x][y][z] = 0;
                    else if (y < top - 5 && hashNoise(x * 3 + z, y * 7, 413) % 181 == 0) world[x][y][z] = 8;
                    else world[x][y][z] = 3;
                }
            }
        }
    }

    for (int x = 5; x < WORLD_X - 5; x += 5) {
        for (int z = 5; z < WORLD_Z - 5; z += 6) {
            int biome = biomeAt(x, z);
            uint32_t roll = hashNoise(x, z, 77) % 100;
            if (biome == 1 || roll > (biome == 3 ? 62u : 30u)) continue;
            int ground = terrainHeight(x, z);
            if (world[x][ground][z] != 1 || ground + 6 >= WORLD_Y) continue;
            int trunk = biome == 3 ? 4 : 3;
            for (int y = 1; y <= trunk; ++y) world[x][ground + y][z] = 4;
            for (int dx = -2; dx <= 2; ++dx) {
                for (int dz = -2; dz <= 2; ++dz) {
                    for (int dy = trunk - 1; dy <= trunk + 2; ++dy) {
                        int xx = x + dx, yy = ground + dy, zz = z + dz;
                        if (!insideWorld(xx, yy, zz)) continue;
                        if (std::abs(dx) + std::abs(dz) + std::abs(dy - (trunk + 1)) <= 4 &&
                            world[xx][yy][zz] == 0) world[xx][yy][zz] = 5;
                    }
                }
            }
        }
    }

    player = glm::vec3(32.0f, terrainHeight(32, 32) + 2.2f, 32.0f);
    velocity = glm::vec3(0.0f);
}

bool meshSolidAt(int x, int y, int z) {
    return insideWorld(x, y, z) && world[x][y][z] != 0;
}

bool solidAt(int x, int y, int z) {
    if (x < 0 || x >= WORLD_X || z < 0 || z >= WORLD_Z || y < 0) return true;
    if (y >= WORLD_Y) return false;
    return world[x][y][z] != 0;
}

bool collides(glm::vec3 p) {
    const float radius = 0.29f;
    const float minX = p.x - radius, maxX = p.x + radius;
    const float minY = p.y - 1.65f, maxY = p.y + 0.12f;
    const float minZ = p.z - radius, maxZ = p.z + radius;
    for (int x = static_cast<int>(std::floor(minX + 0.5f)); x <= static_cast<int>(std::floor(maxX + 0.5f)); ++x)
        for (int y = static_cast<int>(std::floor(minY + 0.5f)); y <= static_cast<int>(std::floor(maxY + 0.5f)); ++y)
            for (int z = static_cast<int>(std::floor(minZ + 0.5f)); z <= static_cast<int>(std::floor(maxZ + 0.5f)); ++z)
                if (solidAt(x, y, z)) return true;
    return false;
}

glm::vec3 cameraFront() {
    glm::vec3 front;
    front.x = std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch));
    front.y = std::sin(glm::radians(pitch));
    front.z = std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch));
    return glm::normalize(front);
}

void mouseCallback(GLFWwindow*, double xpos, double ypos) {
    mouseX = xpos;
    mouseY = ypos;
    if (inventoryOpen || uiScreen != UiScreen::Gameplay) {
        lastX = static_cast<float>(xpos);
        lastY = static_cast<float>(ypos);
        firstMouse = true;
        return;
    }
    if (firstMouse) {
        lastX = static_cast<float>(xpos);
        lastY = static_cast<float>(ypos);
        firstMouse = false;
    }
    float dx = static_cast<float>(xpos) - lastX;
    float dy = lastY - static_cast<float>(ypos);
    lastX = static_cast<float>(xpos);
    lastY = static_cast<float>(ypos);
    yaw += dx * mouseSensitivity;
    pitch = std::clamp(pitch + dy * mouseSensitivity, -89.0f, 89.0f);
}

void scrollCallback(GLFWwindow*, double, double yoffset) {
    if (uiScreen != UiScreen::Gameplay || inventoryOpen) return;
    selectedBlock += yoffset > 0 ? 1 : -1;
    if (selectedBlock < 1) selectedBlock = HOTBAR_SIZE;
    if (selectedBlock > HOTBAR_SIZE) selectedBlock = 1;
}

bool raycast(glm::ivec3& hit, glm::ivec3& previous) {
    glm::vec3 direction = cameraFront();
    glm::ivec3 lastCell(-999);
    for (float distance = 0.08f; distance <= 7.0f; distance += 0.025f) {
        glm::vec3 point = player + direction * distance;
        glm::ivec3 cell(
            static_cast<int>(std::floor(point.x + 0.5f)),
            static_cast<int>(std::floor(point.y + 0.5f)),
            static_cast<int>(std::floor(point.z + 0.5f))
        );
        if (cell == lastCell) continue;
        lastCell = cell;
        if (insideWorld(cell.x, cell.y, cell.z) && world[cell.x][cell.y][cell.z] != 0) {
            hit = cell;
            glm::vec3 before = player + direction * std::max(0.0f, distance - 0.03f);
            previous = glm::ivec3(
                static_cast<int>(std::floor(before.x + 0.5f)),
                static_cast<int>(std::floor(before.y + 0.5f)),
                static_cast<int>(std::floor(before.z + 0.5f))
            );
            return true;
        }
    }
    return false;
}

int tileForBlock(int id, int face) {
    if (id == 1) return face == 2 ? 0 : (face == 3 ? 1 : 8);
    if (id == 2) return 1;
    if (id == 3) return 2;
    if (id == 4) return 3;
    if (id == 5) return 4;
    if (id == 6) return 5;
    if (id == 7) return 6;
    if (id == 8) return 7;
    if (id == 9) return 9;
    return 2;
}

float vertexAO(int x, int y, int z, int face, int corner) {
    glm::ivec3 normal = faceNormals[face];
    glm::ivec3 axisU, axisV;
    if (face <= 1) {
        axisU = {1,0,0};
        axisV = {0,1,0};
    } else if (face <= 3) {
        axisU = {1,0,0};
        axisV = {0,0,1};
    } else {
        axisU = {0,0,1};
        axisV = {0,1,0};
    }
    const float* p = faceCorners[face][corner];
    int signU = p[axisU.x ? 0 : (axisU.y ? 1 : 2)] < 0.0f ? -1 : 1;
    int signV = p[axisV.y ? 1 : 2] < 0.0f ? -1 : 1;
    glm::ivec3 side1 = normal + axisU * signU;
    glm::ivec3 side2 = normal + axisV * signV;
    glm::ivec3 diagonal = normal + axisU * signU + axisV * signV;
    bool a = meshSolidAt(x + side1.x, y + side1.y, z + side1.z);
    bool b = meshSolidAt(x + side2.x, y + side2.y, z + side2.z);
    bool c = meshSolidAt(x + diagonal.x, y + diagonal.y, z + diagonal.z);
    int level = (a && b) ? 0 : 3 - (static_cast<int>(a) + static_cast<int>(b) + static_cast<int>(c));
    return 0.48f + 0.173333f * static_cast<float>(level);
}

void addFace(std::vector<Vertex>& vertices, int localX, int y, int localZ,
             int worldX, int worldZ, int id, int face) {
    constexpr float atlasWidth = 160.0f;
    constexpr float tileSize = 16.0f;
    int tile = tileForBlock(id, face);
    float u0 = (tile * tileSize + 0.5f) / atlasWidth;
    float u1 = ((tile + 1) * tileSize - 0.5f) / atlasWidth;
    float v0 = 0.5f / tileSize;
    float v1 = 15.5f / tileSize;
    const float uv[4][2] = {{u0,v0},{u1,v0},{u1,v1},{u0,v1}};
    const int order[6] = {0,1,2,0,2,3};
    for (int i : order) {
        const float* p = faceCorners[face][i];
        const glm::vec3 n(faceNormals[face]);
        float ao = vertexAO(worldX, y, worldZ, face, i);
        vertices.push_back({
            localX + p[0], y + p[1], localZ + p[2],
            n.x, n.y, n.z,
            uv[i][0], uv[i][1], ao
        });
    }
}

int chunkIndex(int cx, int cz) {
    return cz * CHUNKS_X + cx;
}

void markChunkDirty(int x, int z) {
    int cx = x / CHUNK_SIZE;
    int cz = z / CHUNK_SIZE;
    chunks[chunkIndex(cx, cz)].dirty = true;
    if (x % CHUNK_SIZE == 0 && cx > 0) chunks[chunkIndex(cx - 1, cz)].dirty = true;
    if (x % CHUNK_SIZE == CHUNK_SIZE - 1 && cx + 1 < CHUNKS_X) chunks[chunkIndex(cx + 1, cz)].dirty = true;
    if (z % CHUNK_SIZE == 0 && cz > 0) chunks[chunkIndex(cx, cz - 1)].dirty = true;
    if (z % CHUNK_SIZE == CHUNK_SIZE - 1 && cz + 1 < CHUNKS_Z) chunks[chunkIndex(cx, cz + 1)].dirty = true;
}

void rebuildChunk(int cx, int cz) {
    ChunkMesh& chunk = chunks[chunkIndex(cx, cz)];
    std::vector<Vertex> vertices;
    vertices.reserve(24000);
    int originX = cx * CHUNK_SIZE;
    int originZ = cz * CHUNK_SIZE;
    const int neighbor[6][3] = {{0,0,1},{0,0,-1},{0,1,0},{0,-1,0},{1,0,0},{-1,0,0}};

    for (int x = originX; x < originX + CHUNK_SIZE; ++x) {
        for (int y = 0; y < WORLD_Y; ++y) {
            for (int z = originZ; z < originZ + CHUNK_SIZE; ++z) {
                int id = world[x][y][z];
                if (!id) continue;
                for (int face = 0; face < 6; ++face) {
                    int nx = x + neighbor[face][0];
                    int ny = y + neighbor[face][1];
                    int nz = z + neighbor[face][2];
                    if (!meshSolidAt(nx, ny, nz))
                        addFace(vertices, x - originX, y, z - originZ, x, z, id, face);
                }
            }
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, chunk.vbo);
    glBufferData(GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
        vertices.empty() ? nullptr : vertices.data(), GL_DYNAMIC_DRAW);
    chunk.vertexCount = static_cast<GLsizei>(vertices.size());
    chunk.dirty = false;
}

void rebuildDirtyChunks() {
    for (int cz = 0; cz < CHUNKS_Z; ++cz)
        for (int cx = 0; cx < CHUNKS_X; ++cx)
            if (chunks[chunkIndex(cx, cz)].dirty) rebuildChunk(cx, cz);
}

void setupGeometry() {
    for (ChunkMesh& chunk : chunks) {
        glGenVertexArrays(1, &chunk.vao);
        glGenBuffers(1, &chunk.vbo);
        glBindVertexArray(chunk.vao);
        glBindBuffer(GL_ARRAY_BUFFER, chunk.vbo);
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, x)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, nx)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, u)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, ao)));
    }

    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
}

void saveWorld() {
    struct Header {
        char magic[8];
        uint32_t width;
        uint32_t height;
        uint32_t depth;
    };
    Header header{{'M','C','3','D','2','7','3','\0'}, WORLD_X, WORLD_Y, WORLD_Z};
    float playerData[5] = {player.x, player.y, player.z, yaw, pitch};
    std::ofstream file("MiniCraft3D.world", std::ios::binary | std::ios::trunc);
    if (!file) {
        std::cerr << "Could not save world.\n";
        return;
    }
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file.write(reinterpret_cast<const char*>(world), sizeof(world));
    file.write(reinterpret_cast<const char*>(playerData), sizeof(playerData));
    file.write(reinterpret_cast<const char*>(inventory.slots().data()),
               static_cast<std::streamsize>(sizeof(mc::ItemStack) * mc::InventorySize));
    file.write(reinterpret_cast<const char*>(inventory.durabilitySlots().data()),
               static_cast<std::streamsize>(sizeof(int) * mc::InventorySize));
    float statsData[2] = {playerStats.health(), playerStats.hunger()};
    file.write(reinterpret_cast<const char*>(statsData), sizeof(statsData));
    uint32_t dropCount = static_cast<uint32_t>(itemDrops.size());
    file.write(reinterpret_cast<const char*>(&dropCount), sizeof(dropCount));
    for (const mc::ItemDrop& drop : itemDrops) {
        int32_t itemData[2] = {drop.itemId, drop.count};
        float state[7] = {drop.x, drop.y, drop.z, drop.vx, drop.vy, drop.vz, drop.age};
        file.write(reinterpret_cast<const char*>(itemData), sizeof(itemData));
        file.write(reinterpret_cast<const char*>(state), sizeof(state));
    }
    if (file) std::cout << "World saved: MiniCraft3D.world\\n";
    else std::cerr << "World save failed.\n";
}

bool loadWorld() {
    struct Header {
        char magic[8];
        uint32_t width;
        uint32_t height;
        uint32_t depth;
    };
    std::ifstream file("MiniCraft3D.world", std::ios::binary);
    if (!file) return false;
    Header header{};
    int loaded[WORLD_X][WORLD_Y][WORLD_Z]{};
    float playerData[5]{};
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    bool oldFormat = std::memcmp(header.magic, "MC3D271", 7) == 0;
    bool currentFormat = std::memcmp(header.magic, "MC3D272", 7) == 0;
    bool survivalFormat = std::memcmp(header.magic, "MC3D273", 7) == 0;
    if (!file || (!oldFormat && !currentFormat && !survivalFormat) ||
        header.width != WORLD_X || header.height != WORLD_Y || header.depth != WORLD_Z)
        return false;
    file.read(reinterpret_cast<char*>(loaded), sizeof(loaded));
    file.read(reinterpret_cast<char*>(playerData), sizeof(playerData));
    if (!file || !std::isfinite(playerData[0]) || !std::isfinite(playerData[1]) ||
        !std::isfinite(playerData[2]) || !std::isfinite(playerData[3]) ||
        !std::isfinite(playerData[4])) return false;

    std::array<mc::ItemStack, mc::InventorySize> loadedInventory{};
    std::array<int, mc::InventorySize> loadedDurability{};
    float loadedStats[2] = {mc::PlayerStats::MaxHealth, mc::PlayerStats::MaxHunger};
    std::vector<mc::ItemDrop> loadedDrops;
    if (currentFormat || survivalFormat) {
        file.read(reinterpret_cast<char*>(loadedInventory.data()),
                  static_cast<std::streamsize>(sizeof(mc::ItemStack) * mc::InventorySize));
        if (survivalFormat) {
            file.read(reinterpret_cast<char*>(loadedDurability.data()),
                      static_cast<std::streamsize>(sizeof(int) * mc::InventorySize));
            file.read(reinterpret_cast<char*>(loadedStats), sizeof(loadedStats));
            if (!file || !std::isfinite(loadedStats[0]) || !std::isfinite(loadedStats[1])) return false;
        }
        uint32_t dropCount = 0;
        file.read(reinterpret_cast<char*>(&dropCount), sizeof(dropCount));
        if (!file || dropCount > 4096) return false;
        loadedDrops.reserve(dropCount);
        for (uint32_t i = 0; i < dropCount; ++i) {
            int32_t itemData[2]{};
            float state[7]{};
            file.read(reinterpret_cast<char*>(itemData), sizeof(itemData));
            file.read(reinterpret_cast<char*>(state), sizeof(state));
            if (!file) return false;
            if (itemData[0] <= 0 || itemData[1] <= 0) continue;
            mc::ItemDrop drop;
            drop.itemId = itemData[0];
            drop.count = itemData[1];
            drop.x = state[0];
            drop.y = state[1];
            drop.z = state[2];
            drop.vx = state[3];
            drop.vy = state[4];
            drop.vz = state[5];
            drop.age = state[6];
            loadedDrops.push_back(drop);
        }
    }

    std::memcpy(world, loaded, sizeof(world));
    player = glm::vec3(playerData[0], playerData[1], playerData[2]);
    yaw = playerData[3];
    pitch = std::clamp(playerData[4], -89.0f, 89.0f);
    velocity = glm::vec3(0.0f);
    if (survivalFormat) inventory.replace(loadedInventory, loadedDurability);
    else inventory.replace(loadedInventory);
    playerStats.set(loadedStats[0], loadedStats[1]);
    itemDrops = std::move(loadedDrops);
    return true;
}

void processInput(GLFWwindow* window, float dt) {
    bool escapeDown = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    if (escapeDown && !pauseWasDown) {
        if (inventoryOpen) {
            inventoryOpen = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        } else if (uiScreen == UiScreen::Gameplay) {
            uiScreen = UiScreen::Pause;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else if (uiScreen == UiScreen::Settings) {
            uiScreen = UiScreen::Pause;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            uiScreen = UiScreen::Gameplay;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        firstMouse = true;
    }
    pauseWasDown = escapeDown;
    if (uiScreen != UiScreen::Gameplay) {
        playerMoving = false;
        playerSprinting = false;
        return;
    }
    for (int i = 0; i < HOTBAR_SIZE; ++i)
        if (glfwGetKey(window, GLFW_KEY_1 + i) == GLFW_PRESS) selectedBlock = i + 1;

    bool inventoryDown = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
    bool craftDown = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;
    if (inventoryDown && !inventoryWasDown) {
        inventoryOpen = !inventoryOpen;
        glfwSetInputMode(window, GLFW_CURSOR, inventoryOpen ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
        firstMouse = true;
    }
    if (craftDown && !craftWasDown) {
        int crafted = mc::Crafting::craftFirstAvailable(inventory);
        lastCraftMessage = crafted >= 0
            ? std::string("Crafted ") + mc::Crafting::recipe(static_cast<std::size_t>(crafted)).name
            : "No available recipe";
    }
    inventoryWasDown = inventoryDown;
    craftWasDown = craftDown;

    bool saveDown = glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS;
    bool loadDown = glfwGetKey(window, GLFW_KEY_F9) == GLFW_PRESS;
    if (saveDown && !saveWasDown) saveWorld();
    if (loadDown && !loadWasDown && loadWorld()) {
        for (ChunkMesh& chunk : chunks) chunk.dirty = true;
        std::cout << "World loaded.\n";
    }
    saveWasDown = saveDown;
    loadWasDown = loadDown;
    if (inventoryOpen) {
        playerMoving = false;
        playerSprinting = false;
        return;
    }

    glm::vec3 front = cameraFront();
    glm::vec3 forward = glm::normalize(glm::vec3(front.x, 0.0f, front.z));
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
    playerSprinting = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
    float speed = playerSprinting ? 8.5f : 5.2f;
    glm::vec3 move(0.0f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) move += forward;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) move -= forward;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) move += right;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) move -= right;
    playerMoving = glm::length(move) > 0.0f;
    if (playerMoving) move = glm::normalize(move) * speed * dt;

    glm::vec3 next = player;
    next.x += move.x;
    if (!collides(next)) player.x = next.x;
    next = player;
    next.z += move.z;
    if (!collides(next)) player.z = next.z;

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && grounded) {
        velocity.y = 7.4f;
        grounded = false;
    }
    velocity.y = std::max(velocity.y - 20.0f * dt, -28.0f);
    next = player;
    next.y += velocity.y * dt;
    if (!collides(next)) {
        player.y = next.y;
        grounded = false;
    } else {
        if (velocity.y < 0.0f) {
            grounded = true;
            float impactSpeed = -velocity.y;
            if (impactSpeed > 12.0f) playerStats.damage((impactSpeed - 12.0f) * 1.5f);
        }
        velocity.y = 0.0f;
    }
    if (player.y < -12.0f) {
        playerStats.damage(mc::PlayerStats::MaxHealth);
        player = glm::vec3(32.0f, terrainHeight(32, 32) + 2.2f, 32.0f);
        velocity = glm::vec3(0.0f);
    }
}

glm::vec3 uiColor(int id);
void setFrameUniforms(const glm::mat4& view, const glm::mat4& projection,
                      const glm::vec3& camera, const glm::vec3& fog);

void processUiMouse(GLFWwindow* window) {
    bool down = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    if (uiScreen == UiScreen::Gameplay || !down || uiClickWasDown) {
        uiClickWasDown = down;
        return;
    }
    const int x = static_cast<int>(mouseX);
    const int y = screenHeight - static_cast<int>(mouseY);
    const int centerX = screenWidth / 2;
    const int centerY = screenHeight / 2;
    auto inside = [&](int left, int bottom, int width, int height) {
        return x >= left && x < left + width && y >= bottom && y < bottom + height;
    };
    if (uiScreen == UiScreen::Pause) {
        if (inside(centerX - 116, centerY + 48, 232, 38)) {
            uiScreen = UiScreen::Gameplay;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true;
        } else if (inside(centerX - 116, centerY + 1, 232, 38)) {
            saveWorld();
            lastCraftMessage = "World saved";
        } else if (inside(centerX - 116, centerY - 46, 232, 38)) {
            uiScreen = UiScreen::Settings;
        } else if (inside(centerX - 116, centerY - 93, 232, 38)) {
            glfwSetWindowShouldClose(window, true);
        }
    } else if (uiScreen == UiScreen::Settings) {
        const int left = centerX - 215;
        if (inside(left + 274, centerY + 35, 42, 34))
            fieldOfView = std::clamp(fieldOfView - 2.0f, 60.0f, 100.0f);
        else if (inside(left + 370, centerY + 35, 42, 34))
            fieldOfView = std::clamp(fieldOfView + 2.0f, 60.0f, 100.0f);
        else if (inside(left + 274, centerY - 21, 42, 34))
            mouseSensitivity = std::clamp(mouseSensitivity - 0.01f, 0.035f, 0.30f);
        else if (inside(left + 370, centerY - 21, 42, 34))
            mouseSensitivity = std::clamp(mouseSensitivity + 0.01f, 0.035f, 0.30f);
        else if (inside(left + 274, centerY - 77, 138, 34))
            showDebugInfo = !showDebugInfo;
        else if (inside(centerX - 116, centerY - 153, 232, 38))
            uiScreen = UiScreen::Pause;
    }
    uiClickWasDown = down;
}

void processInventoryMouse(GLFWwindow* window) {
    bool down = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    if (!inventoryOpen) {
        inventoryClickWasDown = down;
        return;
    }
    if (down && !inventoryClickWasDown) {
        const int box = 42;
        const int gap = 5;
        const int total = HOTBAR_SIZE * box + (HOTBAR_SIZE - 1) * gap;
        const int start = (screenWidth - total) / 2;
        int x = static_cast<int>(mouseX);
        int y = screenHeight - static_cast<int>(mouseY);
        int column = (x - start) / (box + gap);
        if (column >= 0 && column < HOTBAR_SIZE) {
            int slotX = start + column * (box + gap);
            if (x >= slotX && x < slotX + box && y >= 18 && y < 18 + box) {
                selectedBlock = column + 1;
            } else {
                for (int row = 0; row < 3; ++row) {
                    int slotY = 18 + (3 - row) * (box + gap) + 12;
                    if (x < slotX || x >= slotX + box || y < slotY || y >= slotY + box) continue;
                    std::size_t backpackIndex = static_cast<std::size_t>(8 + row * 8 + column);
                    std::size_t hotbarIndex = static_cast<std::size_t>(selectedBlock - 1);
                    inventory.swapSlots(backpackIndex, hotbarIndex);
                    break;
                }
            }
        }
    }
    inventoryClickWasDown = down;
}

void processBlocks(GLFWwindow* window, float dt) {
    if (uiScreen != UiScreen::Gameplay) {
        leftWasDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        rightWasDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        return;
    }
    if (inventoryOpen) {
        leftWasDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        rightWasDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        return;
    }
    glm::ivec3 hit, previous;
    bool found = raycast(hit, previous);
    bool left = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool right = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    bool changed = false;

    if (left && found) {
        if (hit != miningTarget) {
            miningTarget = hit;
            miningProgress = 0.0f;
        }
        int brokenBlock = world[hit.x][hit.y][hit.z];
        int heldItem = inventory.slot(static_cast<std::size_t>(selectedBlock - 1)).itemId;
        float hardness = 1.0f;
        if (brokenBlock == 3) hardness = 2.0f;
        else if (brokenBlock == 8) hardness = 3.0f;
        else if (brokenBlock == 4) hardness = 1.5f;
        else if (brokenBlock == 5) hardness = 0.25f;
        else if (brokenBlock == 1 || brokenBlock == 2 || brokenBlock == 6 || brokenBlock == 7)
            hardness = 0.45f;
        float efficiency = mc::Items::miningEfficiency(heldItem, brokenBlock);
        if (!mc::Items::isTool(heldItem)) efficiency = 0.75f;
        miningProgress += dt * efficiency / hardness;
        if (miningProgress >= 1.0f) {
            bool requiresPickaxe = brokenBlock == 3 || brokenBlock == 8;
            bool harvestable = !requiresPickaxe || mc::Items::canHarvest(heldItem, brokenBlock);
            world[hit.x][hit.y][hit.z] = 0;
            if (harvestable) {
                mc::ItemDrop drop;
                drop.itemId = brokenBlock;
                drop.count = 1;
                drop.x = static_cast<float>(hit.x);
                drop.y = static_cast<float>(hit.y);
                drop.z = static_cast<float>(hit.z);
                drop.vy = 1.8f;
                itemDrops.push_back(drop);
            }
            if (mc::Items::isTool(heldItem) &&
                mc::Items::toolType(heldItem) != mc::ToolType::Sword)
                inventory.damageItem(static_cast<std::size_t>(selectedBlock - 1));
            markChunkDirty(hit.x, hit.z);
            changed = true;
            miningTarget = glm::ivec3(-999);
            miningProgress = 0.0f;
        }
    } else {
        miningTarget = glm::ivec3(-999);
        miningProgress = 0.0f;
    }

    const mc::ItemStack held = inventory.slot(static_cast<std::size_t>(selectedBlock - 1));
    if (right && !rightWasDown && found && held.itemId >= 1 && held.itemId <= 9 &&
        held.count > 0 && insideWorld(previous.x, previous.y, previous.z) &&
        world[previous.x][previous.y][previous.z] == 0) {
        world[previous.x][previous.y][previous.z] = held.itemId;
        if (collides(player)) world[previous.x][previous.y][previous.z] = 0;
        else if (inventory.removeItem(held.itemId, 1)) {
            markChunkDirty(previous.x, previous.z);
            changed = true;
        } else {
            world[previous.x][previous.y][previous.z] = 0;
        }
    }

    leftWasDown = left;
    rightWasDown = right;
    if (changed) rebuildDirtyChunks();
}

void updateDrops(float dt) {
    for (std::size_t i = 0; i < itemDrops.size();) {
        mc::ItemDrop& drop = itemDrops[i];
        drop.age += dt;
        drop.vy = std::max(drop.vy - 12.0f * dt, -12.0f);
        drop.x += drop.vx * dt;
        drop.z += drop.vz * dt;
        float nextY = drop.y + drop.vy * dt;
        int bx = static_cast<int>(std::floor(drop.x + 0.5f));
        int by = static_cast<int>(std::floor(nextY - 0.12f + 0.5f));
        int bz = static_cast<int>(std::floor(drop.z + 0.5f));
        if (solidAt(bx, by, bz)) {
            drop.vy = 0.0f;
            drop.y = static_cast<float>(by) + 0.62f;
        } else {
            drop.y = nextY;
        }
        drop.vx *= std::max(0.0f, 1.0f - 3.0f * dt);
        drop.vz *= std::max(0.0f, 1.0f - 3.0f * dt);

        glm::vec3 delta(drop.x - player.x, drop.y - (player.y - 0.8f), drop.z - player.z);
        if (glm::dot(delta, delta) < 2.25f) {
            int remaining = inventory.addItem(drop.itemId, drop.count);
            if (remaining == 0) {
                itemDrops.erase(itemDrops.begin() + static_cast<std::ptrdiff_t>(i));
                continue;
            }
            drop.count = remaining;
        }
        ++i;
    }
}

void renderDrops(const glm::mat4& view, const glm::mat4& projection,
                 const glm::vec3& camera, const glm::vec3& fog) {
    if (itemDrops.empty()) return;
    setFrameUniforms(view, projection, camera, fog);
    glUniform1i(locTextured, 0);
    glBindVertexArray(cubeVAO);
    for (const mc::ItemDrop& drop : itemDrops) {
        glm::vec3 color = uiColor(drop.itemId);
        glUniform3fv(locSolidColor, 1, glm::value_ptr(color));
        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(drop.x, drop.y, drop.z));
        model = glm::scale(model, glm::vec3(0.22f));
        glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(model));
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
}

glm::vec4 matrixRow(const glm::mat4& m, int row) {
    return glm::vec4(m[0][row], m[1][row], m[2][row], m[3][row]);
}

std::array<Plane, 6> extractFrustum(const glm::mat4& matrix) {
    glm::vec4 r0 = matrixRow(matrix, 0);
    glm::vec4 r1 = matrixRow(matrix, 1);
    glm::vec4 r2 = matrixRow(matrix, 2);
    glm::vec4 r3 = matrixRow(matrix, 3);
    glm::vec4 raw[6] = {r3 + r0, r3 - r0, r3 + r1, r3 - r1, r3 + r2, r3 - r2};
    std::array<Plane, 6> planes{};
    for (int i = 0; i < 6; ++i) {
        float magnitude = glm::length(glm::vec3(raw[i]));
        planes[i].normal = glm::vec3(raw[i]) / magnitude;
        planes[i].distance = raw[i].w / magnitude;
    }
    return planes;
}

bool chunkVisible(const std::array<Plane, 6>& planes, int cx, int cz) {
    glm::vec3 minimum(cx * CHUNK_SIZE - 0.5f, -0.5f, cz * CHUNK_SIZE - 0.5f);
    glm::vec3 maximum((cx + 1) * CHUNK_SIZE - 0.5f, WORLD_Y - 0.5f,
                      (cz + 1) * CHUNK_SIZE - 0.5f);
    for (const Plane& plane : planes) {
        glm::vec3 positive(
            plane.normal.x >= 0.0f ? maximum.x : minimum.x,
            plane.normal.y >= 0.0f ? maximum.y : minimum.y,
            plane.normal.z >= 0.0f ? maximum.z : minimum.z
        );
        if (glm::dot(plane.normal, positive) + plane.distance < 0.0f) return false;
    }
    return true;
}

void setFrameUniforms(const glm::mat4& view, const glm::mat4& projection,
                      const glm::vec3& camera, const glm::vec3& fog) {
    glUseProgram(shaderProgram);
    glUniformMatrix4fv(locView, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(locProjection, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3fv(locCamera, 1, glm::value_ptr(camera));
    glUniform3fv(locFog, 1, glm::value_ptr(fog));
    float sunAngle = worldTime * 0.04f;
    glm::vec3 sun(std::cos(sunAngle) * 0.55f, std::sin(sunAngle), -0.35f);
    glUniform3fv(locSun, 1, glm::value_ptr(sun));
    glUniform1f(locFogDensity, 0.009f);
    glUniform1f(locTime, worldTime);
    glUniform1i(locAtlas, 0);
}

int renderWorld(const glm::mat4& view, const glm::mat4& projection,
                const glm::vec3& camera, const glm::vec3& fog) {
    setFrameUniforms(view, projection, camera, fog);
    glUniform1i(locTextured, 1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlasTexture);
    auto planes = extractFrustum(projection * view);
    int visible = 0;

    for (int cz = 0; cz < CHUNKS_Z; ++cz) {
        for (int cx = 0; cx < CHUNKS_X; ++cx) {
            ChunkMesh& chunk = chunks[chunkIndex(cx, cz)];
            if (!chunk.vertexCount || !chunkVisible(planes, cx, cz)) continue;
            glm::mat4 model = glm::translate(glm::mat4(1.0f),
                glm::vec3(cx * CHUNK_SIZE, 0.0f, cz * CHUNK_SIZE));
            glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(model));
            glBindVertexArray(chunk.vao);
            glDrawArrays(GL_TRIANGLES, 0, chunk.vertexCount);
            ++visible;
        }
    }
    return visible;
}

void renderTarget(const glm::mat4& view, const glm::mat4& projection,
                  const glm::vec3& camera, const glm::vec3& fog) {
    glm::ivec3 hit, previous;
    if (!raycast(hit, previous)) return;
    setFrameUniforms(view, projection, camera, fog);
    glUniform1i(locTextured, 0);
    glUniform3f(locSolidColor, 1.0f, 0.91f, 0.22f);
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(hit));
    glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(model));
    glBindVertexArray(cubeVAO);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(2.0f);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

glm::vec3 uiColor(int id) {
    switch (id) {
        case 1: return {0.28f,0.68f,0.17f};
        case 2: return {0.39f,0.25f,0.12f};
        case 3: return {0.48f,0.49f,0.52f};
        case 4: return {0.48f,0.27f,0.11f};
        case 5: return {0.13f,0.42f,0.12f};
        case 6: return {0.78f,0.69f,0.43f};
        case 7: return {0.88f,0.91f,0.96f};
        case 8: return {0.72f,0.48f,0.16f};
        case 9: return {0.68f,0.43f,0.22f};
        case 10: return {0.78f,0.78f,0.82f};
        case 11: return {0.58f,0.35f,0.18f};
        case 20: return {0.68f,0.48f,0.27f};
        case 21: return {0.55f,0.58f,0.62f};
        case 22: return {0.66f,0.43f,0.22f};
        case 23: return {0.72f,0.61f,0.39f};
        case 24: return {0.76f,0.76f,0.79f};
        default: return {0.2f,0.2f,0.2f};
    }
}

void drawUiRect(int x, int y, int width, int height, const glm::vec3& color) {
    if (width <= 0 || height <= 0) return;
    glScissor(x, y, width, height);
    glClearColor(color.r, color.g, color.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void drawUiDigit(int digit, int x, int y, int scale, const glm::vec3& color) {
    static const unsigned char glyphs[10][5] = {
        {7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},
        {7,4,7,1,7},{7,4,7,5,7},{7,1,1,1,1},{7,5,7,5,7},{7,5,7,1,7}
    };
    if (digit < 0 || digit > 9) return;
    for (int row = 0; row < 5; ++row)
        for (int column = 0; column < 3; ++column)
            if (glyphs[digit][row] & (1u << (2 - column)))
                drawUiRect(x + column * scale, y + (4 - row) * scale, scale, scale, color);
}

void drawUiNumber(int value, int x, int y, int scale, const glm::vec3& color) {
    std::string digits = std::to_string(std::max(0, value));
    if (digits.size() > 3) digits = digits.substr(digits.size() - 3);
    int width = static_cast<int>(digits.size()) * (4 * scale) - scale;
    int cursor = x - width;
    for (char digit : digits) {
        drawUiDigit(digit - '0', cursor, y, scale, color);
        cursor += 4 * scale;
    }
}

void drawSurvivalIcon(int x, int y, float amount, bool food) {
    static const char* heart[6] = {
        "##..##", "######", "######", ".####.", "..##..", "...#.."
    };
    static const char* drumstick[6] = {
        "..###.", ".#####", "..####", ".#####", "#####.", ".###.."
    };
    const char** shape = food ? drumstick : heart;
    const glm::vec3 shadow = food ? glm::vec3(0.20f,0.12f,0.07f) : glm::vec3(0.22f,0.035f,0.045f);
    const glm::vec3 fill = food ? glm::vec3(0.91f,0.56f,0.16f) : glm::vec3(0.91f,0.16f,0.22f);
    const glm::vec3 muted = food ? glm::vec3(0.35f,0.25f,0.16f) : glm::vec3(0.34f,0.13f,0.15f);
    for (int row = 0; row < 6; ++row) {
        for (int column = 0; column < 6; ++column) {
            if (shape[row][column] != '#') continue;
            int px = x + column * 2;
            int py = y + (5 - row) * 2;
            drawUiRect(px, py, 2, 2, shadow);
            float point = static_cast<float>(column) / 6.0f;
            drawUiRect(px, py + 1, 1, 1, point < amount ? fill : muted);
        }
    }
}

void drawUiItemIcon(int x, int y, int itemId) {
    glm::vec3 color = uiColor(itemId);
    drawUiRect(x + 2, y + 1, 22, 22, glm::vec3(0.025f,0.03f,0.04f));
    drawUiRect(x + 4, y + 3, 18, 18, color);
    drawUiRect(x + 4, y + 18, 18, 3, color * 0.68f);
    drawUiRect(x + 5, y + 19, 6, 2, color * 1.18f);
}

void drawUI() {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_SCISSOR_TEST);

    const int centerX = screenWidth / 2;
    const int centerY = screenHeight / 2;
    const glm::vec3 panel = {0.035f,0.045f,0.065f};
    const glm::vec3 panelEdge = {0.20f,0.25f,0.31f};
    const glm::vec3 slotColor = {0.075f,0.09f,0.12f};
    const glm::vec3 slotInner = {0.12f,0.14f,0.17f};
    const glm::vec3 white = {0.91f,0.94f,0.96f};
    const glm::vec3 muted = {0.47f,0.54f,0.62f};

    auto glyph = [](char raw) -> std::array<unsigned char, 7> {
        char c = raw >= 'a' && raw <= 'z' ? static_cast<char>(raw - 'a' + 'A') : raw;
        static const std::array<std::array<unsigned char, 7>, 36> font = {{
            {{14,17,17,31,17,17,17}},{{30,17,17,30,17,17,30}},{{14,17,16,16,16,17,14}},
            {{30,17,17,17,17,17,30}},{{31,16,16,30,16,16,31}},{{31,16,16,30,16,16,16}},
            {{14,17,16,23,17,17,15}},{{17,17,17,31,17,17,17}},{{14,4,4,4,4,4,14}},
            {{7,2,2,2,18,18,12}},{{17,18,20,24,20,18,17}},{{16,16,16,16,16,16,31}},
            {{17,27,21,21,17,17,17}},{{17,25,21,19,17,17,17}},{{14,17,17,17,17,17,14}},
            {{30,17,17,30,16,16,16}},{{14,17,17,17,21,18,13}},{{30,17,17,30,20,18,17}},
            {{15,16,16,14,1,1,30}},{{31,4,4,4,4,4,4}},{{17,17,17,17,17,17,14}},
            {{17,17,17,17,17,10,4}},{{17,17,17,21,21,21,10}},{{17,17,10,4,10,17,17}},
            {{17,17,10,4,4,4,4}},{{31,1,2,4,8,16,31}},
            {{14,17,19,21,25,17,14}},{{4,12,4,4,4,4,14}},{{14,17,1,2,4,8,31}},
            {{30,1,1,14,1,1,30}},{{2,6,10,18,31,2,2}},{{31,16,16,30,1,1,30}},
            {{14,16,16,30,17,17,14}},{{31,1,2,4,8,8,8}},{{14,17,17,14,17,17,14}},
            {{14,17,17,15,1,1,14}}
        }};
        if (c >= 'A' && c <= 'Z') return font[static_cast<std::size_t>(c - 'A')];
        if (c >= '0' && c <= '9') return font[static_cast<std::size_t>(26 + c - '0')];
        if (c == '-') return {{0,0,0,31,0,0,0}};
        if (c == '.') return {{0,0,0,0,0,12,12}};
        if (c == ':') return {{0,12,12,0,12,12,0}};
        if (c == '/') return {{1,2,2,4,8,8,16}};
        if (c == '+') return {{0,4,4,31,4,4,0}};
        return {{0,0,0,0,0,0,0}};
    };

    auto drawText = [&](const std::string& value, int x, int y, int scale,
                        const glm::vec3& color, bool centered = false) {
        int width = value.empty() ? 0 : static_cast<int>(value.size()) * 6 * scale - scale;
        int cursor = centered ? x - width / 2 : x;
        for (char c : value) {
            auto rows = glyph(c);
            for (int row = 0; row < 7; ++row)
                for (int column = 0; column < 5; ++column)
                    if (rows[static_cast<std::size_t>(row)] & (1u << (4 - column)))
                        drawUiRect(cursor + column * scale, y + (6 - row) * scale,
                                   scale, scale, color);
            cursor += 6 * scale;
        }
    };

    auto drawButton = [&](int x, int y, int width, int height, const std::string& label,
                          bool highlighted = false) {
        drawUiRect(x, y, width, height, highlighted ? glm::vec3(0.78f,0.59f,0.18f) : panelEdge);
        drawUiRect(x + 2, y + 2, width - 4, height - 4,
                   highlighted ? glm::vec3(0.18f,0.15f,0.09f) : slotColor);
        drawText(label, x + width / 2, y + (height - 7 * 2) / 2 + 1, 2,
                 highlighted ? glm::vec3(1.0f,0.88f,0.54f) : white, true);
    };

    if (uiScreen == UiScreen::Pause || uiScreen == UiScreen::Settings) {
        const int panelWidth = uiScreen == UiScreen::Pause ? 360 : 430;
        const int panelHeight = 340;
        const int left = centerX - panelWidth / 2;
        const int bottom = centerY - panelHeight / 2;
        drawUiRect(left - 4, bottom - 4, panelWidth + 8, panelHeight + 8, {0.015f,0.02f,0.03f});
        drawUiRect(left, bottom, panelWidth, panelHeight, panelEdge);
        drawUiRect(left + 3, bottom + 3, panelWidth - 6, panelHeight - 6, panel);
        drawUiRect(left + 16, bottom + panelHeight - 64, panelWidth - 32, 2, {0.34f,0.42f,0.50f});
        drawText(uiScreen == UiScreen::Pause ? "GAME PAUSED" : "SETTINGS",
                 centerX, bottom + panelHeight - 48, 3, white, true);
        if (uiScreen == UiScreen::Pause) {
            drawButton(centerX - 116, centerY + 48, 232, 38, "RESUME", true);
            drawButton(centerX - 116, centerY + 1, 232, 38, "SAVE WORLD");
            drawButton(centerX - 116, centerY - 46, 232, 38, "SETTINGS");
            drawButton(centerX - 116, centerY - 93, 232, 38, "QUIT GAME");
            drawText("ESC  RESUME", centerX, bottom + 13, 1, muted, true);
        } else {
            drawText("FIELD OF VIEW", left + 28, centerY + 48, 2, white);
            drawButton(left + 274, centerY + 35, 42, 34, "-");
            drawText(std::to_string(static_cast<int>(fieldOfView)), left + 336, centerY + 48, 2, white, true);
            drawButton(left + 370, centerY + 35, 42, 34, "+");
            drawText("MOUSE SENSITIVITY", left + 28, centerY - 8, 2, white);
            drawButton(left + 274, centerY - 21, 42, 34, "-");
            drawText(std::to_string(static_cast<int>(mouseSensitivity * 1000.0f)),
                     left + 336, centerY - 8, 2, white, true);
            drawButton(left + 370, centerY - 21, 42, 34, "+");
            drawText("DEBUG INFO", left + 28, centerY - 64, 2, white);
            drawButton(left + 274, centerY - 77, 138, 34, showDebugInfo ? "ON" : "OFF", showDebugInfo);
            drawButton(centerX - 116, bottom + 17, 232, 38, "BACK", true);
        }
    }

    if (uiScreen != UiScreen::Gameplay) {
        glDisable(GL_SCISSOR_TEST);
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        return;
    }

    if (!inventoryOpen) {
        drawUiRect(centerX - 1, centerY - 10, 3, 21, {0.015f,0.02f,0.025f});
        drawUiRect(centerX - 10, centerY - 1, 21, 3, {0.015f,0.02f,0.025f});
        drawUiRect(centerX, centerY - 8, 1, 17, white);
        drawUiRect(centerX - 8, centerY, 17, 1, white);
        if (miningProgress > 0.0f) {
            drawUiRect(centerX - 42, centerY - 24, 84, 5, {0.025f,0.03f,0.04f});
            drawUiRect(centerX - 41, centerY - 23,
                       static_cast<int>(82 * std::clamp(miningProgress, 0.0f, 1.0f)), 3,
                       {0.94f,0.72f,0.20f});
        }
    }

    const int hotbarBox = 48;
    const int hotbarGap = 4;
    const int hotbarTotal = HOTBAR_SIZE * hotbarBox + (HOTBAR_SIZE - 1) * hotbarGap;
    const int hotbarStart = (screenWidth - hotbarTotal) / 2;
    const int hotbarY = 18;
    drawUiRect(hotbarStart - 7, hotbarY - 7, hotbarTotal + 14, hotbarBox + 14, panelEdge);
    drawUiRect(hotbarStart - 5, hotbarY - 5, hotbarTotal + 10, hotbarBox + 10, panel);

    auto drawSlot = [&](int x, int y, int box, const mc::ItemStack& stack,
                        bool selected, std::size_t slotIndex, int hotbarNumber) {
        drawUiRect(x, y, box, box, selected ? glm::vec3(0.96f,0.76f,0.25f) : panelEdge);
        drawUiRect(x + 2, y + 2, box - 4, box - 4, selected ? glm::vec3(0.25f,0.20f,0.10f) : slotColor);
        drawUiRect(x + 4, y + 4, box - 8, box - 8, slotInner);
        if (hotbarNumber > 0)
            drawUiDigit(hotbarNumber, x + 5, y + box - 11, 1, muted);
        if (stack.itemId > 0 && stack.count > 0) {
            int iconSize = box >= 46 ? 26 : 22;
            drawUiItemIcon(x + (box - iconSize) / 2, y + (box - iconSize) / 2 + 1, stack.itemId);
            drawUiNumber(stack.count, x + box - 4, y + 4, 1, white);
            int maximum = mc::Items::maxDurability(stack.itemId);
            if (maximum > 0) {
                float ratio = static_cast<float>(inventory.durability(slotIndex)) / maximum;
                int barWidth = static_cast<int>((box - 10) * std::clamp(ratio, 0.0f, 1.0f));
                drawUiRect(x + 5, y + 3, box - 10, 3, {0.025f,0.025f,0.03f});
                glm::vec3 durabilityColor = ratio > 0.5f ? glm::vec3(0.25f,0.82f,0.34f) :
                    (ratio > 0.2f ? glm::vec3(0.91f,0.68f,0.18f) : glm::vec3(0.90f,0.22f,0.18f));
                drawUiRect(x + 5, y + 3, barWidth, 3, durabilityColor);
            }
        }
    };

    if (inventoryOpen) {
        const int box = 42;
        const int gap = 5;
        const int total = HOTBAR_SIZE * box + (HOTBAR_SIZE - 1) * gap;
        const int start = (screenWidth - total) / 2;
        drawUiRect(start - 11, 68, total + 22, 160, panelEdge);
        drawUiRect(start - 9, 70, total + 18, 156, panel);
        drawUiRect(start - 2, 218, total + 4, 2, {0.30f,0.38f,0.46f});
        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 8; ++column) {
                int index = 8 + row * 8 + column;
                int x = start + column * (box + gap);
                int y = 18 + (3 - row) * (box + gap) + 12;
                drawSlot(x, y, box, inventory.slot(static_cast<std::size_t>(index)),
                         false, static_cast<std::size_t>(index), 0);
            }
        }
    }

    for (int i = 0; i < HOTBAR_SIZE; ++i) {
        int x = hotbarStart + i * (hotbarBox + hotbarGap);
        drawSlot(x, hotbarY, hotbarBox, inventory.slot(static_cast<std::size_t>(i)),
                 i + 1 == selectedBlock, static_cast<std::size_t>(i), i + 1);
    }

    if (!inventoryOpen) {
        const int iconY = hotbarY + hotbarBox + 13;
        const int iconStart = centerX - 190;
        for (int i = 0; i < 10; ++i) {
            float healthPart = std::clamp((playerStats.health() - i * 2.0f) / 2.0f, 0.0f, 1.0f);
            float hungerPart = std::clamp((playerStats.hunger() - i * 2.0f) / 2.0f, 0.0f, 1.0f);
            drawSurvivalIcon(iconStart + i * 18, iconY, healthPart, false);
            drawSurvivalIcon(centerX + 10 + i * 18, iconY, hungerPart, true);
        }
    }

    if (inventoryOpen) {
        drawUiRect(centerX - 54, screenHeight - 48, 108, 3, {0.85f,0.67f,0.22f});
        drawUiRect(centerX - 53, screenHeight - 47, 106, 1, {0.28f,0.34f,0.40f});
    }

    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}
std::string inventorySummary() {
    std::string summary;
    for (std::size_t i = 0; i < mc::InventorySize; ++i) {
        const mc::ItemStack& stack = inventory.slot(i);
        if (stack.itemId <= 0 || stack.count <= 0) continue;
        summary += " " + std::to_string(stack.itemId) + "x" + std::to_string(stack.count);
    }
    return summary;
}

void framebufferCallback(GLFWwindow*, int w, int h) {
    screenWidth = std::max(w, 1);
    screenHeight = std::max(h, 1);
    glViewport(0, 0, screenWidth, screenHeight);
}

int main() {
    if (!glfwInit()) {
        std::cerr << "GLFW initialization failed\n";
        return 1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow* window = glfwCreateWindow(screenWidth, screenHeight, "MiniCraft3D | 27.4", nullptr, nullptr);
    if (!window) {
        std::cerr << "OpenGL window creation failed\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferCallback);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSwapInterval(1);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "GLEW initialization failed\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    glGetError();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_MULTISAMPLE);

    createShader();
    setupGeometry();
    createAtlas();
    generateWorld();
    if (loadWorld()) std::cout << "Saved world loaded.\n";
    else std::cout << "Generated a new world.\n";
    for (ChunkMesh& chunk : chunks) chunk.dirty = true;
    rebuildDirtyChunks();

    double lastFrame = glfwGetTime();
    double titleTimer = 0.0;
    int frames = 0;
    float fps = 0.0f;
    int visibleChunks = 0;

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = static_cast<float>(std::min(now - lastFrame, 0.05));
        lastFrame = now;
        if (uiScreen == UiScreen::Gameplay) worldTime += dt;

        processInput(window, dt);
        processUiMouse(window);
        processInventoryMouse(window);
        processBlocks(window, dt);
        if (uiScreen == UiScreen::Gameplay) playerStats.tick(dt, playerMoving, playerSprinting);
        if (!playerStats.alive()) {
            player = glm::vec3(32.0f, terrainHeight(32, 32) + 2.2f, 32.0f);
            velocity = glm::vec3(0.0f);
            playerStats.set(mc::PlayerStats::MaxHealth, 12.0f);
            lastCraftMessage = "Respawned";
        }
        if (uiScreen == UiScreen::Gameplay) updateDrops(dt);

        float daylight = 0.5f + 0.5f * std::sin(worldTime * 0.04f);
        glm::vec3 nightSky(0.025f,0.035f,0.09f);
        glm::vec3 daySky(0.48f,0.72f,0.94f);
        glm::vec3 sky = glm::mix(nightSky, daySky, daylight);
        glClearColor(sky.r, sky.g, sky.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(glm::radians(fieldOfView),
            static_cast<float>(screenWidth) / static_cast<float>(screenHeight), 0.1f, 120.0f);
        glm::vec3 front = cameraFront();
        glm::mat4 view = glm::lookAt(player, player + front, glm::vec3(0,1,0));

        visibleChunks = renderWorld(view, projection, player, sky);
        renderDrops(view, projection, player, sky);
        renderTarget(view, projection, player, sky);
        drawUI();

        glfwSwapBuffers(window);
        glfwPollEvents();

        ++frames;
        titleTimer += dt;
        if (titleTimer >= 0.5) {
            fps = frames / static_cast<float>(titleTimer);
            frames = 0;
            titleTimer = 0.0;
            std::string title = "MiniCraft3D 27.4";
            if (showDebugInfo) {
                title += " | " + std::to_string(static_cast<int>(fps)) + " FPS | Chunks " +
                    std::to_string(visibleChunks) + "/" + std::to_string(CHUNK_COUNT) +
                    " | XYZ " + std::to_string(static_cast<int>(player.x)) + "," +
                    std::to_string(static_cast<int>(player.y)) + "," +
                    std::to_string(static_cast<int>(player.z));
            }
            title += (inventoryOpen ? " | Inventory" : "") +
                std::string(" | HP:") + std::to_string(static_cast<int>(playerStats.health())) +
                " Hunger:" + std::to_string(static_cast<int>(playerStats.hunger())) +
                " | Items:" + inventorySummary() +
                (lastCraftMessage.empty() ? "" : " | " + lastCraftMessage);
            glfwSetWindowTitle(window, title.c_str());
        }
    }

    saveWorld();
    glDeleteTextures(1, &atlasTexture);
    for (ChunkMesh& chunk : chunks) {
        glDeleteVertexArrays(1, &chunk.vao);
        glDeleteBuffers(1, &chunk.vbo);
    }
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteProgram(shaderProgram);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
