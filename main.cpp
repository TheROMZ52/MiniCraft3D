#define GLFW_INCLUDE_NONE
#define NOMINMAX
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

constexpr int WORLD_X = 64;
constexpr int WORLD_Y = 32;
constexpr int WORLD_Z = 64;
constexpr int HOTBAR_SIZE = 8;
constexpr float PI = 3.1415926535f;

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
int selectedBlock = 1;
int screenWidth = 1280;
int screenHeight = 720;
float worldTime = 0.0f;

GLuint worldVAO = 0;
GLuint worldVBO = 0;
GLuint cubeVAO = 0;
GLuint cubeVBO = 0;
GLuint lineVAO = 0;
GLuint lineVBO = 0;
GLuint shaderProgram = 0;
GLuint atlasTexture = 0;
GLsizei worldVertexCount = 0;

struct Vertex {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
};

const char* vertexShaderSource = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
out vec3 vNormal;
out vec3 vWorldPos;
out vec2 vUV;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uModel;
void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;
    vUV = aUV;
    gl_Position = uProjection * uView * worldPos;
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
in vec3 vNormal;
in vec3 vWorldPos;
in vec2 vUV;
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
    float ambient = 0.34 + 0.08 * max(n.y, 0.0);
    float daylight = 0.72 + 0.28 * sin(uTime * 0.035);
    vec3 lit = texel.rgb * (ambient + diffuse * 0.72) * daylight;
    float distanceToCamera = length(vWorldPos - uCameraPos);
    float fog = 1.0 - exp(-pow(distanceToCamera * uFogDensity, 1.45));
    fog = clamp(fog, 0.0, 0.88);
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

const glm::vec3 faceNormals[6] = {
    {0,0,1},{0,0,-1},{0,1,0},{0,-1,0},{1,0,0},{-1,0,0}
};

uint32_t hashNoise(int x, int y, int seed) {
    uint32_t n = static_cast<uint32_t>(x * 374761393 + y * 668265263 + seed * 1442695041u);
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
        case 6: return {0.55f,0.20f,0.16f};
        default: return {0.76f,0.55f,0.18f};
    }
}

void createAtlas() {
    constexpr int tileSize = 16;
    constexpr int columns = 9;
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
                if (tile == 6 && (y == 5 || y == 11)) variation *= 0.68f;
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
}

int terrainHeight(int x, int z) {
    float broad = std::sin(x * 0.105f) * 2.8f + std::cos(z * 0.12f) * 2.4f;
    float detail = std::sin((x + z) * 0.23f) * 1.15f + std::cos((x - z) * 0.19f) * 0.8f;
    return std::clamp(7 + static_cast<int>(std::round(broad + detail)), 3, 15);
}

bool insideWorld(int x, int y, int z) {
    return x >= 0 && x < WORLD_X && y >= 0 && y < WORLD_Y && z >= 0 && z < WORLD_Z;
}

void generateWorld() {
    for (int x = 0; x < WORLD_X; ++x) {
        for (int z = 0; z < WORLD_Z; ++z) {
            int top = terrainHeight(x, z);
            for (int y = 0; y <= top; ++y) {
                if (y == top) world[x][y][z] = 1;
                else if (y >= top - 3) world[x][y][z] = 2;
                else world[x][y][z] = 3;
            }
            if (top <= 5) world[x][top][z] = 6;
        }
    }

    for (int x = 5; x < WORLD_X - 5; x += 9) {
        for (int z = 5; z < WORLD_Z - 5; z += 11) {
            if (hashNoise(x, z, 77) % 4 == 0) continue;
            int ground = terrainHeight(x, z);
            if (world[x][ground][z] == 6) continue;
            for (int y = 1; y <= 4; ++y)
                if (insideWorld(x, ground + y, z)) world[x][ground + y][z] = 4;
            for (int dx = -2; dx <= 2; ++dx) {
                for (int dz = -2; dz <= 2; ++dz) {
                    for (int dy = 2; dy <= 5; ++dy) {
                        int xx = x + dx, yy = ground + dy, zz = z + dz;
                        if (insideWorld(xx, yy, zz) &&
                            std::abs(dx) + std::abs(dz) + std::abs(dy - 3) <= 5 &&
                            world[xx][yy][zz] == 0) world[xx][yy][zz] = 5;
                    }
                }
            }
        }
    }

    for (int x = 0; x < WORLD_X; ++x) {
        for (int z = 0; z < WORLD_Z; ++z) {
            int top = terrainHeight(x, z);
            if (hashNoise(x, z, 123) % 29 == 0 && world[x][top][z] == 1 && top + 1 < WORLD_Y)
                world[x][top + 1][z] = 7;
            if (hashNoise(x, z, 991) % 97 == 0 && world[x][top][z] == 1 && top + 1 < WORLD_Y)
                world[x][top + 1][z] = 8;
        }
    }

    player = glm::vec3(32.0f, terrainHeight(32, 32) + 2.2f, 32.0f);
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
    if (firstMouse) {
        lastX = static_cast<float>(xpos);
        lastY = static_cast<float>(ypos);
        firstMouse = false;
    }
    float dx = static_cast<float>(xpos) - lastX;
    float dy = lastY - static_cast<float>(ypos);
    lastX = static_cast<float>(xpos);
    lastY = static_cast<float>(ypos);
    yaw += dx * 0.105f;
    pitch = std::clamp(pitch + dy * 0.105f, -89.0f, 89.0f);
}

void scrollCallback(GLFWwindow*, double, double yoffset) {
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
    if (id == 4) return (face == 2 || face == 3) ? 3 : 3;
    if (id == 5) return 4;
    if (id == 6) return 5;
    if (id == 7) return 6;
    if (id == 8) return 7;
    return 2;
}

void addFace(std::vector<Vertex>& vertices, int x, int y, int z, int id, int face) {
    int tile = tileForBlock(id, face);
    constexpr float atlasWidth = 144.0f;
    constexpr float tileSize = 16.0f;
    float u0 = (tile * tileSize + 0.5f) / atlasWidth;
    float u1 = ((tile + 1) * tileSize - 0.5f) / atlasWidth;
    float v0 = 0.5f / tileSize;
    float v1 = 15.5f / tileSize;
    const float uv[4][2] = {{u0,v0},{u1,v0},{u1,v1},{u0,v1}};
    const int order[6] = {0,1,2,0,2,3};
    for (int i : order) {
        const float* p = faceCorners[face][i];
        const glm::vec3 n = faceNormals[face];
        vertices.push_back({
            x + p[0], y + p[1], z + p[2],
            n.x, n.y, n.z,
            uv[i][0], uv[i][1]
        });
    }
}

void rebuildWorldMesh() {
    std::vector<Vertex> vertices;
    vertices.reserve(180000);
    const int neighbor[6][3] = {{0,0,1},{0,0,-1},{0,1,0},{0,-1,0},{1,0,0},{-1,0,0}};
    for (int x = 0; x < WORLD_X; ++x) {
        for (int y = 0; y < WORLD_Y; ++y) {
            for (int z = 0; z < WORLD_Z; ++z) {
                int id = world[x][y][z];
                if (!id) continue;
                for (int face = 0; face < 6; ++face) {
                    int nx = x + neighbor[face][0];
                    int ny = y + neighbor[face][1];
                    int nz = z + neighbor[face][2];
                    if (!solidAt(nx, ny, nz)) addFace(vertices, x, y, z, id, face);
                }
            }
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, worldVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)), vertices.data(), GL_STATIC_DRAW);
    worldVertexCount = static_cast<GLsizei>(vertices.size());
}

void setupVertexLayout(GLuint vao, GLuint vbo, const void* data, GLsizeiptr size, GLenum usage) {
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, size, data, usage);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, nx)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, u)));
}

void createGeometry() {
    glGenVertexArrays(1, &worldVAO);
    glGenBuffers(1, &worldVBO);
    setupVertexLayout(worldVAO, worldVBO, nullptr, 0, GL_DYNAMIC_DRAW);

    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

    glGenVertexArrays(1, &lineVAO);
    glGenBuffers(1, &lineVBO);
    glBindVertexArray(lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
    glBufferData(GL_ARRAY_BUFFER, 24 * 3 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
}

void processInput(GLFWwindow* window, float dt) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        glfwSetWindowShouldClose(window, true);
    }
    for (int i = 0; i < HOTBAR_SIZE; ++i)
        if (glfwGetKey(window, GLFW_KEY_1 + i) == GLFW_PRESS) selectedBlock = i + 1;

    glm::vec3 front = cameraFront();
    glm::vec3 forward = glm::normalize(glm::vec3(front.x, 0.0f, front.z));
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
    float speed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 8.5f : 5.2f;
    glm::vec3 move(0.0f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) move += forward;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) move -= forward;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) move += right;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) move -= right;
    if (glm::length(move) > 0.0f) move = glm::normalize(move) * speed * dt;

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
        if (velocity.y < 0.0f) grounded = true;
        velocity.y = 0.0f;
    }
    if (player.y < -12.0f) {
        player = glm::vec3(32.0f, terrainHeight(32, 32) + 2.2f, 32.0f);
        velocity = glm::vec3(0.0f);
    }
}

void processBlocks(GLFWwindow* window) {
    glm::ivec3 hit, previous;
    bool found = raycast(hit, previous);
    bool left = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool right = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    bool changed = false;

    if (left && !leftWasDown && found && !(hit.x == 32 && hit.z == 32 && hit.y <= 0)) {
        world[hit.x][hit.y][hit.z] = 0;
        changed = true;
    }
    if (right && !rightWasDown && found && insideWorld(previous.x, previous.y, previous.z) &&
        world[previous.x][previous.y][previous.z] == 0) {
        world[previous.x][previous.y][previous.z] = selectedBlock;
        if (collides(player)) world[previous.x][previous.y][previous.z] = 0;
        else changed = true;
    }
    leftWasDown = left;
    rightWasDown = right;
    if (changed) rebuildWorldMesh();
}

void setCommonUniforms(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camera, const glm::vec3& fog) {
    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uView"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uProjection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3fv(glGetUniformLocation(shaderProgram, "uCameraPos"), 1, glm::value_ptr(camera));
    glUniform3fv(glGetUniformLocation(shaderProgram, "uFogColor"), 1, glm::value_ptr(fog));
    glm::vec3 sun(-0.45f, 0.75f + 0.2f * std::sin(worldTime * 0.035f), -0.35f);
    glUniform3fv(glGetUniformLocation(shaderProgram, "uSunDirection"), 1, glm::value_ptr(sun));
    glUniform1f(glGetUniformLocation(shaderProgram, "uFogDensity"), 0.012f);
    glUniform1f(glGetUniformLocation(shaderProgram, "uTime"), worldTime);
    glUniform1i(glGetUniformLocation(shaderProgram, "uAtlas"), 0);
}

void renderWorld(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camera, const glm::vec3& fog) {
    setCommonUniforms(view, projection, camera, fog);
    glUniform1i(glGetUniformLocation(shaderProgram, "uTextured"), 1);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlasTexture);
    glBindVertexArray(worldVAO);
    glDrawArrays(GL_TRIANGLES, 0, worldVertexCount);
}

void renderTarget(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camera, const glm::vec3& fog) {
    glm::ivec3 hit, previous;
    if (!raycast(hit, previous)) return;
    setCommonUniforms(view, projection, camera, fog);
    glUniform1i(glGetUniformLocation(shaderProgram, "uTextured"), 0);
    glUniform3f(glGetUniformLocation(shaderProgram, "uSolidColor"), 1.0f, 0.92f, 0.24f);
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(hit));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(model));
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
        case 7: return {0.55f,0.20f,0.16f};
        default: return {0.76f,0.55f,0.18f};
    }
}

void drawUI() {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_SCISSOR_TEST);
    int cx = screenWidth / 2;
    int cy = screenHeight / 2;
    glScissor(cx - 1, cy - 9, 2, 18);
    glClearColor(1.0f, 1.0f, 1.0f, 0.95f);
    glClear(GL_COLOR_BUFFER_BIT);
    glScissor(cx - 9, cy - 1, 18, 2);
    glClear(GL_COLOR_BUFFER_BIT);

    const int box = 42;
    const int gap = 5;
    const int total = HOTBAR_SIZE * box + (HOTBAR_SIZE - 1) * gap;
    const int start = (screenWidth - total) / 2;
    for (int i = 0; i < HOTBAR_SIZE; ++i) {
        int x = start + i * (box + gap);
        glScissor(x, 18, box, box);
        glClearColor(0.035f, 0.045f, 0.06f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glScissor(x + 5, 23, box - 10, box - 10);
        glm::vec3 color = uiColor(i + 1);
        glClearColor(color.r, color.g, color.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        if (i + 1 == selectedBlock) {
            glScissor(x - 2, 16, box + 4, 2);
            glClearColor(1.0f, 0.83f, 0.2f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            glScissor(x - 2, 60, box + 4, 2);
            glClear(GL_COLOR_BUFFER_BIT);
        }
    }
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
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

    GLFWwindow* window = glfwCreateWindow(screenWidth, screenHeight, "MiniCraft 3D | 27.1", nullptr, nullptr);
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
    createGeometry();
    createAtlas();
    generateWorld();
    rebuildWorldMesh();

    double lastFrame = glfwGetTime();
    double titleTimer = 0.0;
    int frames = 0;
    float fps = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = static_cast<float>(std::min(now - lastFrame, 0.05));
        lastFrame = now;
        worldTime += dt;

        processInput(window, dt);
        processBlocks(window);

        float daylight = 0.5f + 0.5f * std::sin(worldTime * 0.035f);
        glm::vec3 sky = glm::mix(glm::vec3(0.035f,0.045f,0.10f), glm::vec3(0.48f,0.72f,0.94f), daylight);
        glClearColor(sky.r, sky.g, sky.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(glm::radians(76.0f),
            static_cast<float>(screenWidth) / static_cast<float>(screenHeight), 0.1f, 150.0f);
        glm::vec3 front = cameraFront();
        glm::mat4 view = glm::lookAt(player, player + front, glm::vec3(0,1,0));

        renderWorld(view, projection, player, sky);
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
            std::string title = "MiniCraft 3D 27.1 | " + std::to_string(static_cast<int>(fps)) +
                " FPS | " + std::to_string(static_cast<int>(player.x)) + ", " +
                std::to_string(static_cast<int>(player.y)) + ", " +
                std::to_string(static_cast<int>(player.z));
            glfwSetWindowTitle(window, title.c_str());
        }
    }

    glDeleteTextures(1, &atlasTexture);
    glDeleteVertexArrays(1, &worldVAO);
    glDeleteBuffers(1, &worldVBO);
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteVertexArrays(1, &lineVAO);
    glDeleteBuffers(1, &lineVBO);
    glDeleteProgram(shaderProgram);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
