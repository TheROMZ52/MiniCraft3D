#define GLFW_INCLUDE_NONE
#define NOMINMAX
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "src/gameplay/gameplay.hpp"
#include "src/world/infinite_world.hpp"
#include "src/render/atlas.hpp"
#include <unordered_map>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cctype>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <utility>

constexpr int WORLD_X = 128;
constexpr int WORLD_Y = 32;
constexpr int WORLD_Z = 128;
constexpr int CHUNK_SIZE = 16;
constexpr int CHUNKS_X = WORLD_X / CHUNK_SIZE;
constexpr int CHUNKS_Z = WORLD_Z / CHUNK_SIZE;
constexpr int CHUNK_COUNT = CHUNKS_X * CHUNKS_Z;
constexpr int HOTBAR_SIZE = 8;

int world[WORLD_X][WORLD_Y][WORLD_Z]{};
glm::vec3 player(64.0f, 12.0f, 64.0f);
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
bool craftingWasDown = false;
bool worldInitialized = false;
bool pauseWasDown = false;
enum class UiScreen { MainMenu, WorldSelect, Gameplay, Pause, Settings, Crafting };
UiScreen uiScreen = UiScreen::MainMenu;
UiScreen settingsReturnScreen = UiScreen::Pause;
std::string currentWorldPath = "MiniCraft3D.world";
std::vector<std::string> worldPaths;
std::vector<std::string> worldNames;
float mouseSensitivity = 0.105f;
float fieldOfView = 76.0f;
bool showDebugInfo = true;
int renderDistanceChunks = 8;
bool viewBobbing = true;
float walkAnimationTime = 0.0f;
float handSwingTime = 0.0f;
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
mc::world::InfiniteWorld infiniteWorld;
std::unordered_map<mc::world::ChunkPos, ChunkMesh, mc::world::ChunkPosHash> streamedMeshes;
GLuint cubeVAO = 0;
GLuint cubeVBO = 0;
GLuint shaderProgram = 0;
GLuint atlasTexture = 0;
GLuint skyProgram = 0;
GLuint uiProgram = 0;
GLuint edgeVAO = 0;
GLuint edgeVBO = 0;
GLuint emptyVAO = 0;
GLuint uiVAO = 0;
GLuint uiVBO = 0;
GLint locFogStart = -1;
GLint locFogEnd = -1;
GLint locDay = -1;
GLint locAlpha = -1;
GLint locSolidLight = -1;
GLint locSkyInverse = -1;
GLint locSkySun = -1;
GLint locSkyHorizon = -1;
GLint locSkyZenith = -1;
GLint locSkyCamera = -1;
GLint locSkyDay = -1;
GLint locSkyTime = -1;
GLint locSkyCloud = -1;
GLint locUiScreen = -1;
GLint locUiTint = -1;
GLint locUiMode = -1;
GLint locUiAtlas = -1;
GLint locView = -1;
GLint locProjection = -1;
GLint locModel = -1;
GLint locCamera = -1;
GLint locFog = -1;
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
    vNormal = mat3(uModel) * aNormal;
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
uniform vec3 uFogColor;
uniform float uFogStart;
uniform float uFogEnd;
uniform float uDay;
uniform int uTextured;
uniform vec3 uSolidColor;
uniform float uAlpha;
uniform float uSolidLight;
void main() {
    if (uTextured == 0) {
        FragColor = vec4(uSolidColor * uSolidLight, uAlpha);
        return;
    }
    vec4 texel = texture(uAtlas, vUV);
    if (texel.a < 0.5) discard;
    if (uTextured == 3) {
        FragColor = vec4(0.0, 0.0, 0.0, 0.62);
        return;
    }
    vec3 n = normalize(vNormal);
    float face = n.y > 0.5 ? 1.0 : (n.y < -0.5 ? 0.5 : (abs(n.z) > 0.5 ? 0.8 : 0.6));
    vec3 light = mix(vec3(0.24, 0.28, 0.46), vec3(1.0), uDay);
    vec3 lit = texel.rgb * face * light;
    if (uTextured == 1) {
        lit *= vAO;
        float distanceToCamera = length(vWorldPos - uCameraPos);
        float fog = clamp((distanceToCamera - uFogStart) / max(uFogEnd - uFogStart, 0.001), 0.0, 1.0);
        lit = mix(lit, uFogColor, fog * fog * (3.0 - 2.0 * fog));
    }
    FragColor = vec4(lit, 1.0);
}
)";

const char* skyVertexSource = R"(
#version 330 core
out vec3 vRay;
uniform mat4 uInvViewProj;
void main() {
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    vec2 ndc = p * 2.0 - 1.0;
    gl_Position = vec4(ndc, 1.0, 1.0);
    vec4 world = uInvViewProj * vec4(ndc, 1.0, 1.0);
    vRay = world.xyz / world.w;
}
)";

const char* skyFragmentSource = R"(
#version 330 core
in vec3 vRay;
out vec4 FragColor;
uniform vec3 uSunDir;
uniform vec3 uHorizon;
uniform vec3 uZenith;
uniform vec3 uCamera;
uniform float uDay;
uniform float uTime;
uniform float uCloudHeight;
float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}
float hash31(vec3 p) {
    p = fract(p * vec3(443.897, 441.423, 437.195));
    p += dot(p, p.yzx + 19.19);
    return fract((p.x + p.y) * p.z);
}
float vnoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}
void main() {
    vec3 d = normalize(vRay);
    float h = d.y;
    vec3 col = mix(uHorizon, uZenith, pow(clamp(h, 0.0, 1.0), 0.5));
    if (h < 0.0) col = mix(uHorizon, uHorizon * 0.8, clamp(-h * 5.0, 0.0, 1.0));
    float night = 1.0 - smoothstep(0.0, 0.45, uDay);
    if (night > 0.001 && h > -0.1) {
        vec3 q = d * 80.0;
        float s = hash31(floor(q));
        float twinkle = 0.65 + 0.35 * sin(uTime * 1.7 + s * 80.0);
        float star = step(0.9955, s) * (1.0 - smoothstep(0.18, 0.34, length(fract(q) - 0.5)));
        col += vec3(star * twinkle * night * smoothstep(-0.02, 0.12, h));
    }
    vec3 sunDir = normalize(uSunDir);
    vec3 axis = abs(sunDir.y) > 0.95 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
    vec3 sr = normalize(cross(axis, sunDir));
    vec3 su = cross(sunDir, sr);
    float sd = dot(d, sunDir);
    if (sd > 0.2) {
        vec2 sp = vec2(dot(d, sr), dot(d, su)) / sd;
        float box = max(abs(sp.x), abs(sp.y));
        float body = 1.0 - smoothstep(0.085, 0.092, box);
        float glow = exp(-length(sp) * 7.0) * 0.28;
        col = mix(col, vec3(1.0, 0.97, 0.80), body);
        col += vec3(1.0, 0.82, 0.5) * glow;
    }
    float md = -sd;
    if (md > 0.2 && sunDir.y < 0.12) {
        vec2 mp = vec2(dot(d, sr), -dot(d, su)) / md;
        float box = max(abs(mp.x), abs(mp.y));
        float body = 1.0 - smoothstep(0.058, 0.064, box);
        float shade = 0.84 + 0.16 * hash21(floor(mp * 9.0));
        col = mix(col, vec3(0.88, 0.91, 0.97) * shade, body);
    }
    if (uCamera.y < uCloudHeight && h > 0.012) {
        float t = (uCloudHeight - uCamera.y) / h;
        vec2 pos = uCamera.xz + d.xz * t + vec2(uTime * 1.2, 0.0);
        vec2 cell = floor(pos / 11.0);
        float n = vnoise(cell * 0.21) * 0.62 + vnoise(cell * 0.53 + 7.0) * 0.38;
        float cloud = step(0.56, n);
        float fade = 1.0 - smoothstep(180.0, 420.0, t);
        vec3 cloudColor = mix(vec3(0.16, 0.18, 0.28), vec3(0.95), uDay);
        cloudColor = mix(cloudColor, uHorizon * 1.15, 0.3 * uDay);
        col = mix(col, cloudColor, cloud * fade * 0.92);
    }
    FragColor = vec4(col, 1.0);
}
)";

const char* uiVertexSource = R"(
#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
layout(location=2) in float aShade;
out vec2 vUV;
out float vShade;
uniform vec2 uScreen;
void main() {
    vUV = aUV;
    vShade = aShade;
    gl_Position = vec4(aPos.x / uScreen.x * 2.0 - 1.0, aPos.y / uScreen.y * 2.0 - 1.0, 0.0, 1.0);
}
)";

const char* uiFragmentSource = R"(
#version 330 core
in vec2 vUV;
in float vShade;
out vec4 FragColor;
uniform sampler2D uAtlas;
uniform vec4 uTint;
uniform int uMode;
void main() {
    if (uMode == 1) {
        FragColor = uTint;
        return;
    }
    vec4 t = texture(uAtlas, vUV);
    if (t.a < 0.5) discard;
    FragColor = vec4(t.rgb * vShade * uTint.rgb, 1.0);
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

namespace atl = mc::atlas;

mc::atlas::Pixels atlasPixels;

bool isBlockItem(int id) {
    return (id >= 1 && id <= 9) || id == 11;
}

int itemSpriteTile(int id) {
    switch (id) {
        case 10: return atl::Stick;
        case 20: return atl::WoodPickaxe;
        case 21: return atl::StonePickaxe;
        case 22: return atl::WoodAxe;
        case 23: return atl::WoodShovel;
        case 24: return atl::WoodSword;
        case 25: return atl::StoneAxe;
        case 26: return atl::StoneShovel;
        case 27: return atl::StoneSword;
        default: return atl::Stick;
    }
}

int tileForBlock(int id, int face) {
    switch (id) {
        case 1: return face == 2 ? atl::GrassTop : (face == 3 ? atl::Dirt : atl::GrassSide);
        case 2: return atl::Dirt;
        case 3: return atl::Stone;
        case 4: return (face == 2 || face == 3) ? atl::LogTop : atl::LogSide;
        case 5: return atl::Leaves;
        case 6: return atl::Sand;
        case 7: return atl::Snow;
        case 8: return atl::Ore;
        case 9: return atl::Planks;
        case 11: return face == 2 ? atl::TableTop : (face == 3 ? atl::Planks : atl::TableSide);
        default: return atl::Stone;
    }
}

void faceUv(int face, const float* p, int tile, float& u, float& v) {
    float fu = 0.0f;
    float fv = 0.5f - p[1];
    switch (face) {
        case 0: fu = p[0] + 0.5f; break;
        case 1: fu = 0.5f - p[0]; break;
        case 4: fu = 0.5f - p[2]; break;
        case 5: fu = p[2] + 0.5f; break;
        default: fu = p[0] + 0.5f; fv = p[2] + 0.5f; break;
    }
    u = (static_cast<float>(tile * atl::TileSize) + 0.5f + fu * 15.0f) / static_cast<float>(atl::Width);
    v = (0.5f + fv * 15.0f) / static_cast<float>(atl::TileSize);
}

bool loadExternalAtlas(mc::atlas::Pixels& out) {
    std::ifstream file("textures/atlas.ppm", std::ios::binary);
    if (!file) return false;
    auto nextToken = [&]() {
        std::string token;
        while (file >> std::ws && file.peek() == '#') {
            std::string ignored;
            std::getline(file, ignored);
        }
        file >> token;
        return token;
    };
    std::string format = nextToken();
    int width = 0, height = 0, maximum = 0;
    try {
        width = std::stoi(nextToken());
        height = std::stoi(nextToken());
        maximum = std::stoi(nextToken());
    } catch (...) {
        return false;
    }
    if ((format != "P6" && format != "P3") || width != atl::Width || height != atl::Height || maximum != 255) {
        std::cerr << "Texture atlas must be a " << atl::Width << "x" << atl::Height
                  << " PPM image; using built-in atlas.\n";
        return false;
    }
    std::vector<unsigned char> rgb(static_cast<std::size_t>(width) * height * 3);
    if (format == "P6") {
        char separator = 0;
        file.get(separator);
        if (separator == '\r' && file.peek() == '\n') file.get();
        if (!std::isspace(static_cast<unsigned char>(separator))) return false;
        file.read(reinterpret_cast<char*>(rgb.data()), static_cast<std::streamsize>(rgb.size()));
        if (!file) return false;
    } else {
        for (unsigned char& value : rgb) {
            int channel = 0;
            if (!(file >> channel) || channel < 0 || channel > 255) return false;
            value = static_cast<unsigned char>(channel);
        }
    }
    out.assign(static_cast<std::size_t>(width) * height, {});
    for (std::size_t i = 0; i < out.size(); ++i) {
        unsigned char r = rgb[i * 3], g = rgb[i * 3 + 1], b = rgb[i * 3 + 2];
        if (r == 255 && g == 0 && b == 255) continue;
        out[i] = {r, g, b, 255};
    }
    return true;
}

void createAtlas() {
    atlasPixels = atl::generate();
    mc::atlas::Pixels external;
    if (loadExternalAtlas(external)) atlasPixels = std::move(external);

    std::vector<unsigned char> upload(atlasPixels.size() * 4);
    for (int tile = 0; tile < atl::Columns; ++tile) {
        const atl::Rgba average = atl::averageColor(atlasPixels, tile);
        for (int y = 0; y < atl::TileSize; ++y)
            for (int x = 0; x < atl::TileSize; ++x) {
                const std::size_t index = static_cast<std::size_t>(y * atl::Width + tile * atl::TileSize + x);
                const atl::Rgba& p = atlasPixels[index];
                upload[index * 4 + 0] = p.a ? p.r : average.r;
                upload[index * 4 + 1] = p.a ? p.g : average.g;
                upload[index * 4 + 2] = p.a ? p.b : average.b;
                upload[index * 4 + 3] = p.a;
            }
    }
    glGenTextures(1, &atlasTexture);
    glBindTexture(GL_TEXTURE_2D, atlasTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, atl::Width, atl::Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, upload.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 3);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
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

GLuint linkProgram(const char* vertexSource, const char* fragmentSource) {
    GLuint vertex = compileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048]{};
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::cerr << "Shader link error: " << log << '\n';
    }
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    return program;
}

void createShader() {
    shaderProgram = linkProgram(vertexShaderSource, fragmentShaderSource);
    skyProgram = linkProgram(skyVertexSource, skyFragmentSource);
    uiProgram = linkProgram(uiVertexSource, uiFragmentSource);

    locView = glGetUniformLocation(shaderProgram, "uView");
    locProjection = glGetUniformLocation(shaderProgram, "uProjection");
    locModel = glGetUniformLocation(shaderProgram, "uModel");
    locCamera = glGetUniformLocation(shaderProgram, "uCameraPos");
    locFog = glGetUniformLocation(shaderProgram, "uFogColor");
    locFogStart = glGetUniformLocation(shaderProgram, "uFogStart");
    locFogEnd = glGetUniformLocation(shaderProgram, "uFogEnd");
    locDay = glGetUniformLocation(shaderProgram, "uDay");
    locTextured = glGetUniformLocation(shaderProgram, "uTextured");
    locSolidColor = glGetUniformLocation(shaderProgram, "uSolidColor");
    locAlpha = glGetUniformLocation(shaderProgram, "uAlpha");
    locSolidLight = glGetUniformLocation(shaderProgram, "uSolidLight");
    locAtlas = glGetUniformLocation(shaderProgram, "uAtlas");

    locSkyInverse = glGetUniformLocation(skyProgram, "uInvViewProj");
    locSkySun = glGetUniformLocation(skyProgram, "uSunDir");
    locSkyHorizon = glGetUniformLocation(skyProgram, "uHorizon");
    locSkyZenith = glGetUniformLocation(skyProgram, "uZenith");
    locSkyCamera = glGetUniformLocation(skyProgram, "uCamera");
    locSkyDay = glGetUniformLocation(skyProgram, "uDay");
    locSkyTime = glGetUniformLocation(skyProgram, "uTime");
    locSkyCloud = glGetUniformLocation(skyProgram, "uCloudHeight");

    locUiScreen = glGetUniformLocation(uiProgram, "uScreen");
    locUiTint = glGetUniformLocation(uiProgram, "uTint");
    locUiMode = glGetUniformLocation(uiProgram, "uMode");
    locUiAtlas = glGetUniformLocation(uiProgram, "uAtlas");
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
    std::memset(world, 0, sizeof(world));
    streamedMeshes.clear();
    infiniteWorld.updateStreaming(0, 0, renderDistanceChunks);
    player = glm::vec3(0.0f, mc::world::InfiniteWorld::heightAt(0, 0) + 2.2f, 0.0f);
    velocity = glm::vec3(0.0f);
}

bool meshSolidAt(int x, int y, int z) {
    return infiniteWorld.getBlock(x, y, z) != 0;
}

bool meshOpaqueAt(int x, int y, int z) {
    const int id = infiniteWorld.getBlock(x, y, z);
    return id != 0 && id != 5;
}

bool solidAt(int x, int y, int z) {
    if (y < 0) return true;
    if (y >= mc::world::WorldHeight) return false;
    return infiniteWorld.getBlock(x, y, z) != 0;
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

struct Particle {
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 color;
    float life;
    float size;
};

std::vector<Particle> particles;
int particleSeed = 0;

void spawnBreakParticles(const glm::ivec3& block, int id) {
    const int tile = tileForBlock(id, 0);
    for (int i = 0; i < 16 && particles.size() < 400; ++i) {
        const int s = particleSeed++;
        const int px = static_cast<int>(atl::rnd(i, s, 300) * 15.99f);
        const int py = static_cast<int>(atl::rnd(i, s, 301) * 15.99f);
        atl::Rgba c = atlasPixels[static_cast<std::size_t>(py * atl::Width + tile * atl::TileSize + px)];
        if (!c.a) c = atl::averageColor(atlasPixels, tile);
        Particle p;
        p.position = glm::vec3(block) + glm::vec3(atl::rnd(i, s, 302) - 0.5f,
                                                   atl::rnd(i, s, 303) - 0.5f,
                                                   atl::rnd(i, s, 304) - 0.5f) * 0.8f;
        p.velocity = glm::vec3((atl::rnd(i, s, 305) - 0.5f) * 3.2f,
                               0.6f + atl::rnd(i, s, 306) * 2.6f,
                               (atl::rnd(i, s, 307) - 0.5f) * 3.2f);
        p.color = glm::vec3(c.r, c.g, c.b) / 255.0f;
        p.life = 0.55f + atl::rnd(i, s, 308) * 0.55f;
        p.size = 0.06f + atl::rnd(i, s, 309) * 0.06f;
        particles.push_back(p);
    }
}

void updateParticles(float dt) {
    for (std::size_t i = 0; i < particles.size();) {
        Particle& p = particles[i];
        p.life -= dt;
        if (p.life <= 0.0f) {
            particles.erase(particles.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        p.velocity.y -= 14.0f * dt;
        glm::vec3 next = p.position + p.velocity * dt;
        if (solidAt(static_cast<int>(std::floor(next.x + 0.5f)), static_cast<int>(std::floor(next.y + 0.5f)),
                    static_cast<int>(std::floor(next.z + 0.5f)))) {
            p.velocity = glm::vec3(0.0f);
        } else {
            p.position = next;
        }
        ++i;
    }
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
        if (infiniteWorld.getBlock(cell.x, cell.y, cell.z) != 0) {
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
    bool a = meshOpaqueAt(x + side1.x, y + side1.y, z + side1.z);
    bool b = meshOpaqueAt(x + side2.x, y + side2.y, z + side2.z);
    bool c = meshOpaqueAt(x + diagonal.x, y + diagonal.y, z + diagonal.z);
    int level = (a && b) ? 0 : 3 - (static_cast<int>(a) + static_cast<int>(b) + static_cast<int>(c));
    return 0.48f + 0.173333f * static_cast<float>(level);
}

void addFace(std::vector<Vertex>& vertices, int localX, int y, int localZ,
             int worldX, int worldZ, int id, int face) {
    const int tile = tileForBlock(id, face);
    const int order[6] = {0,1,2,0,2,3};
    for (int i : order) {
        const float* p = faceCorners[face][i];
        const glm::vec3 n(faceNormals[face]);
        float u = 0.0f, v = 0.0f;
        faceUv(face, p, tile, u, v);
        float ao = vertexAO(worldX, y, worldZ, face, i);
        vertices.push_back({
            localX + p[0], y + p[1], localZ + p[2],
            n.x, n.y, n.z,
            u, v, ao
        });
    }
}

int chunkIndex(int cx, int cz) {
    return cz * CHUNKS_X + cx;
}

void markChunkDirty(int x, int z) {
    const auto position = mc::world::InfiniteWorld::chunkOf(x, z);
    auto found = streamedMeshes.find(position);
    if (found != streamedMeshes.end()) found->second.dirty = true;
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
                    if (!meshOpaqueAt(nx, ny, nz))
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

void rebuildStreamedChunk(mc::world::ChunkPos position) {
    auto [it, inserted] = streamedMeshes.try_emplace(position);
    ChunkMesh& mesh = it->second;
    if (inserted) {
        glGenVertexArrays(1, &mesh.vao);
        glGenBuffers(1, &mesh.vbo);
        glBindVertexArray(mesh.vao);
        glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
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
    if (!mesh.dirty && !infiniteWorld.meshDirty(position)) return;
    std::vector<Vertex> vertices;
    vertices.reserve(24000);
    const int originX = position.x * CHUNK_SIZE;
    const int originZ = position.z * CHUNK_SIZE;
    const int neighbor[6][3] = {{0,0,1},{0,0,-1},{0,1,0},{0,-1,0},{1,0,0},{-1,0,0}};
    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int y = 0; y < mc::world::WorldHeight; ++y) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                const int wx = originX + x;
                const int wz = originZ + z;
                const int id = infiniteWorld.getBlock(wx, y, wz);
                if (!id) continue;
                for (int face = 0; face < 6; ++face) {
                    const int nx = wx + neighbor[face][0];
                    const int ny = y + neighbor[face][1];
                    const int nz = wz + neighbor[face][2];
                    if (!meshOpaqueAt(nx, ny, nz))
                        addFace(vertices, x, y, z, wx, wz, id, face);
                }
            }
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
        vertices.empty() ? nullptr : vertices.data(), GL_DYNAMIC_DRAW);
    mesh.vertexCount = static_cast<GLsizei>(vertices.size());
    mesh.dirty = false;
    infiniteWorld.markMeshClean(position);
}

void rebuildDirtyChunks() {
    infiniteWorld.updateStreaming(static_cast<int>(std::floor(player.x)),
        static_cast<int>(std::floor(player.z)), renderDistanceChunks);
    for (auto it = streamedMeshes.begin(); it != streamedMeshes.end();) {
        if (!infiniteWorld.isLoaded(it->first)) {
            glDeleteVertexArrays(1, &it->second.vao);
            glDeleteBuffers(1, &it->second.vbo);
            it = streamedMeshes.erase(it);
        } else {
            ++it;
        }
    }
    for (const auto& position : infiniteWorld.loadedChunkPositions())
        rebuildStreamedChunk(position);
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

    const float edges[] = {
        -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,   0.5f,-0.5f,-0.5f,  0.5f,-0.5f, 0.5f,
         0.5f,-0.5f, 0.5f, -0.5f,-0.5f, 0.5f,  -0.5f,-0.5f, 0.5f, -0.5f,-0.5f,-0.5f,
        -0.5f, 0.5f,-0.5f,  0.5f, 0.5f,-0.5f,   0.5f, 0.5f,-0.5f,  0.5f, 0.5f, 0.5f,
         0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f,  -0.5f, 0.5f, 0.5f, -0.5f, 0.5f,-0.5f,
        -0.5f,-0.5f,-0.5f, -0.5f, 0.5f,-0.5f,   0.5f,-0.5f,-0.5f,  0.5f, 0.5f,-0.5f,
         0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f,  -0.5f,-0.5f, 0.5f, -0.5f, 0.5f, 0.5f
    };
    glGenVertexArrays(1, &edgeVAO);
    glGenBuffers(1, &edgeVBO);
    glBindVertexArray(edgeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, edgeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(edges), edges, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

    glGenVertexArrays(1, &emptyVAO);

    glGenVertexArrays(1, &uiVAO);
    glGenBuffers(1, &uiVBO);
    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(4 * sizeof(float)));
}

void loadSettings() {
    std::ifstream file("MiniCraft3D.cfg");
    if (!file) return;
    file >> fieldOfView >> mouseSensitivity >> renderDistanceChunks >> showDebugInfo >> viewBobbing;
    fieldOfView = std::clamp(fieldOfView, 60.0f, 100.0f);
    mouseSensitivity = std::clamp(mouseSensitivity, 0.035f, 0.30f);
    renderDistanceChunks = std::clamp(renderDistanceChunks, 4, 16);
}

void saveSettings() {
    std::ofstream file("MiniCraft3D.cfg", std::ios::trunc);
    if (file) file << fieldOfView << '\n' << mouseSensitivity << '\n'
                  << renderDistanceChunks << '\n' << showDebugInfo << '\n' << viewBobbing << '\n';
}

void saveWorld() {
    struct Header {
        char magic[8];
        uint32_t width;
        uint32_t height;
        uint32_t depth;
    };
    Header header{{'M','C','3','D','I','N','F','1'}, WORLD_X, WORLD_Y, WORLD_Z};
    float playerData[5] = {player.x, player.y, player.z, yaw, pitch};
    std::error_code directoryError;
    std::filesystem::path savePath(currentWorldPath);
    if (!savePath.parent_path().empty())
        std::filesystem::create_directories(savePath.parent_path(), directoryError);
    std::ofstream file(currentWorldPath, std::ios::binary | std::ios::trunc);
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
    file.close();
    if (file && infiniteWorld.saveAll())
        std::cout << "World saved: " << currentWorldPath << "\n";
    else
        std::cerr << "World save failed.\n";
}

bool loadWorld() {
    struct Header {
        char magic[8];
        uint32_t width;
        uint32_t height;
        uint32_t depth;
    };
    std::ifstream file(currentWorldPath, std::ios::binary);
    if (!file) return false;
    Header header{};
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    bool oldFormat = std::memcmp(header.magic, "MC3D271", 7) == 0;
    bool currentFormat = std::memcmp(header.magic, "MC3D272", 7) == 0;
    bool survivalFormat = std::memcmp(header.magic, "MC3D273", 7) == 0;
    bool infiniteFormat = std::memcmp(header.magic, "MC3DINF1", 8) == 0;
    if (!file || (!oldFormat && !currentFormat && !survivalFormat && !infiniteFormat) ||
        header.width == 0 || header.height == 0 || header.depth == 0 ||
        header.width > 512 || header.height > 256 || header.depth > 512)
        return false;

    const std::size_t volume = static_cast<std::size_t>(header.width) *
        header.height * header.depth;
    if (volume > 64u * 1024u * 1024u) return false;
    std::vector<int> loaded(volume);
    float playerData[5]{};
    file.read(reinterpret_cast<char*>(loaded.data()),
              static_cast<std::streamsize>(loaded.size() * sizeof(int)));
    file.read(reinterpret_cast<char*>(playerData), sizeof(playerData));
    if (!file || !std::isfinite(playerData[0]) || !std::isfinite(playerData[1]) ||
        !std::isfinite(playerData[2]) || !std::isfinite(playerData[3]) ||
        !std::isfinite(playerData[4])) return false;

    std::array<mc::ItemStack, mc::InventorySize> loadedInventory{};
    std::array<int, mc::InventorySize> loadedDurability{};
    float loadedStats[2] = {mc::PlayerStats::MaxHealth, mc::PlayerStats::MaxHunger};
    std::vector<mc::ItemDrop> loadedDrops;
    if (currentFormat || survivalFormat || infiniteFormat) {
        file.read(reinterpret_cast<char*>(loadedInventory.data()),
                  static_cast<std::streamsize>(sizeof(mc::ItemStack) * mc::InventorySize));
        if (survivalFormat || infiniteFormat) {
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
            bool validState = true;
            for (float value : state) validState = validState && std::isfinite(value);
            if (!validState) continue;
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

    std::memset(world, 0, sizeof(world));
    infiniteWorld = mc::world::InfiniteWorld(currentWorldPath + ".chunks");
    for (auto& entry : streamedMeshes) {
        glDeleteVertexArrays(1, &entry.second.vao);
        glDeleteBuffers(1, &entry.second.vbo);
    }
    streamedMeshes.clear();
    generateWorld();
    const int offsetX = std::max(0, (WORLD_X - static_cast<int>(header.width)) / 2);
    const int offsetZ = std::max(0, (WORLD_Z - static_cast<int>(header.depth)) / 2);
    const int copyX = std::min(WORLD_X, static_cast<int>(header.width));
    const int copyY = std::min(WORLD_Y, static_cast<int>(header.height));
    const int copyZ = std::min(WORLD_Z, static_cast<int>(header.depth));
    for (int x = 0; x < copyX; ++x)
        for (int y = 0; y < copyY; ++y)
            for (int z = 0; z < copyZ; ++z) {
                std::size_t index = (static_cast<std::size_t>(x) * header.height + y) *
                    header.depth + z;
                world[x + offsetX][y][z + offsetZ] = loaded[index];
                if (!infiniteFormat)
                    infiniteWorld.setBlock(x + offsetX, y, z + offsetZ, static_cast<std::uint8_t>(loaded[index]));
            }

    player = infiniteFormat
        ? glm::vec3(playerData[0], std::clamp(playerData[1], 1.0f, static_cast<float>(mc::world::WorldHeight - 1)), playerData[2])
        : glm::vec3(std::clamp(playerData[0] + offsetX, 1.0f, static_cast<float>(WORLD_X - 2)),
                    std::clamp(playerData[1], 1.0f, static_cast<float>(mc::world::WorldHeight - 1)),
                    std::clamp(playerData[2] + offsetZ, 1.0f, static_cast<float>(WORLD_Z - 2)));
    yaw = playerData[3];
    pitch = std::clamp(playerData[4], -89.0f, 89.0f);
    velocity = glm::vec3(0.0f);
    if (survivalFormat) inventory.replace(loadedInventory, loadedDurability);
    else inventory.replace(loadedInventory);
    playerStats.set(loadedStats[0], loadedStats[1]);
    for (mc::ItemDrop& drop : loadedDrops) {
        drop.x += static_cast<float>(offsetX);
        drop.z += static_cast<float>(offsetZ);
    }
    itemDrops = std::move(loadedDrops);
    infiniteWorld.updateStreaming(static_cast<int>(std::floor(player.x)),
        static_cast<int>(std::floor(player.z)), renderDistanceChunks);
    return true;
}


void refreshWorldList() {
    worldPaths.clear();
    worldNames.clear();
    std::error_code error;
    std::filesystem::create_directories("worlds", error);
    if (std::filesystem::exists("MiniCraft3D.world")) {
        worldPaths.push_back("MiniCraft3D.world");
        worldNames.push_back("Legacy World");
    }
    for (const auto& entry : std::filesystem::directory_iterator("worlds", error)) {
        if (error) break;
        if (!entry.is_regular_file() || entry.path().extension() != ".world") continue;
        worldPaths.push_back(entry.path().string());
        worldNames.push_back(entry.path().stem().string());
    }
    std::vector<std::size_t> order(worldPaths.size());
    for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return worldNames[a] < worldNames[b];
    });
    std::vector<std::string> sortedPaths, sortedNames;
    for (std::size_t index : order) {
        sortedPaths.push_back(worldPaths[index]);
        sortedNames.push_back(worldNames[index]);
    }
    worldPaths = std::move(sortedPaths);
    worldNames = std::move(sortedNames);
}

void startSelectedWorld(const std::string& path, bool createNew, GLFWwindow* window) {
    currentWorldPath = path;
    infiniteWorld = mc::world::InfiniteWorld(currentWorldPath + ".chunks");
    for (auto& entry : streamedMeshes) {
        glDeleteVertexArrays(1, &entry.second.vao);
        glDeleteBuffers(1, &entry.second.vbo);
    }
    streamedMeshes.clear();
    if (createNew) {
        std::memset(world, 0, sizeof(world));
        inventory.clear();
        itemDrops.clear();
        playerStats.set(mc::PlayerStats::MaxHealth, mc::PlayerStats::MaxHunger);
        yaw = -90.0f;
        pitch = 0.0f;
        worldTime = 0.0f;
        generateWorld();
    } else if (!loadWorld()) {
        generateWorld();
    }
    for (ChunkMesh& chunk : chunks) chunk.dirty = true;
    rebuildDirtyChunks();
    worldInitialized = true;
    leftWasDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    rightWasDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    inventoryClickWasDown = leftWasDown;
    uiClickWasDown = leftWasDown;
    uiScreen = UiScreen::Gameplay;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    firstMouse = true;
}

std::string createWorldPath() {
    int number = 1;
    std::error_code error;
    while (std::filesystem::exists("worlds/World " + std::to_string(number) + ".world", error))
        ++number;
    return "worlds/World " + std::to_string(number) + ".world";
}

void processInput(GLFWwindow* window, float dt) {
    bool escapeDown = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    if (escapeDown && !pauseWasDown) {
        if (inventoryOpen) {
            inventoryOpen = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        } else if (uiScreen == UiScreen::MainMenu) {
        } else if (uiScreen == UiScreen::WorldSelect) {
            uiScreen = UiScreen::MainMenu;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else if (uiScreen == UiScreen::Gameplay) {
            uiScreen = UiScreen::Pause;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else if (uiScreen == UiScreen::Settings) {
            uiScreen = settingsReturnScreen;
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
    bool recipeBookDown = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
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
    if (recipeBookDown && !craftingWasDown) {
        uiScreen = UiScreen::Crafting;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        firstMouse = true;
    }
    craftingWasDown = recipeBookDown;

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
        player = glm::vec3(64.0f, terrainHeight(64, 64) + 2.2f, 64.0f);
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
    if (uiScreen == UiScreen::MainMenu) {
        if (inside(centerX - 130, centerY + 48, 260, 42)) {
            refreshWorldList();
            uiScreen = UiScreen::WorldSelect;
        } else if (inside(centerX - 130, centerY - 4, 260, 42)) {
            settingsReturnScreen = UiScreen::MainMenu;
            uiScreen = UiScreen::Settings;
        } else if (inside(centerX - 130, centerY - 56, 260, 42)) {
            glfwSetWindowShouldClose(window, true);
        }
    } else if (uiScreen == UiScreen::WorldSelect) {
        bool selected = false;
        for (std::size_t i = 0; i < worldPaths.size() && i < 5; ++i) {
            int rowY = centerY + 82 - static_cast<int>(i) * 43;
            if (inside(centerX - 190, rowY, 380, 34)) {
                startSelectedWorld(worldPaths[i], false, window);
                selected = true;
                break;
            }
        }
        if (!selected && inside(centerX - 190, centerY - 145, 380, 38)) {
            std::string path = createWorldPath();
            startSelectedWorld(path, true, window);
            saveWorld();
        } else if (!selected && inside(centerX - 190, centerY - 195, 380, 34)) {
            uiScreen = UiScreen::MainMenu;
        }
    } else if (uiScreen == UiScreen::Crafting) {
        if (inside(centerX - 210, centerY - 220, 420, 34)) {
            uiScreen = UiScreen::Gameplay;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        } else {
            for (std::size_t i = 0; i < mc::Crafting::recipeCount(); ++i) {
                int rowY = centerY + 155 - static_cast<int>(i) * 30;
                if (inside(centerX - 200, rowY, 400, 26) && mc::Crafting::craft(inventory, i)) {
                    lastCraftMessage = std::string("Crafted ") + mc::Crafting::recipe(i).name;
                    break;
                }
            }
        }
    } else if (uiScreen == UiScreen::Pause) {
        if (inside(centerX - 116, centerY + 48, 232, 38)) {
            uiScreen = UiScreen::Gameplay;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true;
        } else if (inside(centerX - 116, centerY + 1, 232, 38)) {
            saveWorld();
            lastCraftMessage = "World saved";
        } else if (inside(centerX - 116, centerY - 46, 232, 38)) {
            settingsReturnScreen = UiScreen::Pause;
            uiScreen = UiScreen::Settings;
        } else if (inside(centerX - 116, centerY - 93, 232, 38)) {
            glfwSetWindowShouldClose(window, true);
        }
    } else if (uiScreen == UiScreen::Settings) {
        const int left = centerX - 215;
        if (inside(left + 274, centerY + 127, 42, 34))
            fieldOfView = std::clamp(fieldOfView - 2.0f, 60.0f, 100.0f);
        else if (inside(left + 370, centerY + 127, 42, 34))
            fieldOfView = std::clamp(fieldOfView + 2.0f, 60.0f, 100.0f);
        else if (inside(left + 274, centerY + 65, 42, 34))
            mouseSensitivity = std::clamp(mouseSensitivity - 0.01f, 0.035f, 0.30f);
        else if (inside(left + 370, centerY + 65, 42, 34))
            mouseSensitivity = std::clamp(mouseSensitivity + 0.01f, 0.035f, 0.30f);
        else if (inside(left + 274, centerY + 3, 42, 34))
            renderDistanceChunks = std::clamp(renderDistanceChunks - 1, 4, 16);
        else if (inside(left + 370, centerY + 3, 42, 34))
            renderDistanceChunks = std::clamp(renderDistanceChunks + 1, 4, 16);
        else if (inside(left + 274, centerY - 59, 138, 34))
            showDebugInfo = !showDebugInfo;
        else if (inside(left + 274, centerY - 121, 138, 34))
            viewBobbing = !viewBobbing;
        else if (inside(centerX - 116, centerY - 233, 232, 38))
            uiScreen = settingsReturnScreen;
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
    if (left && !leftWasDown) handSwingTime = 1.0f;
    bool changed = false;

    if (left && found) {
        if (hit != miningTarget) {
            miningTarget = hit;
            miningProgress = 0.0f;
        }
        int brokenBlock = infiniteWorld.getBlock(hit.x, hit.y, hit.z);
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
            infiniteWorld.setBlock(hit.x, hit.y, hit.z, 0);
            spawnBreakParticles(hit, brokenBlock);
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
        held.count > 0 && infiniteWorld.getBlock(previous.x, previous.y, previous.z) == 0) {
        infiniteWorld.setBlock(previous.x, previous.y, previous.z, static_cast<std::uint8_t>(held.itemId));
        if (collides(player)) infiniteWorld.setBlock(previous.x, previous.y, previous.z, 0);
        else if (inventory.removeItem(held.itemId, 1)) {
            markChunkDirty(previous.x, previous.z);
            changed = true;
        } else {
            infiniteWorld.setBlock(previous.x, previous.y, previous.z, 0);
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

struct MeshGpu {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLsizei count = 0;
};

std::unordered_map<int, MeshGpu> meshCache;

struct SkyState {
    float day = 1.0f;
    glm::vec3 sunDir{0.0f, 1.0f, 0.0f};
    glm::vec3 horizon{0.74f, 0.84f, 0.98f};
    glm::vec3 zenith{0.40f, 0.60f, 0.96f};
};

SkyState skyState;
constexpr float DayLengthSeconds = 480.0f;
constexpr float CloudHeight = 96.0f;

void updateSky() {
    const float angle = worldTime * (6.2831853f / DayLengthSeconds) + 1.1f;
    skyState.sunDir = glm::normalize(glm::vec3(std::cos(angle), std::sin(angle), 0.28f));
    const float h = skyState.sunDir.y;
    skyState.day = glm::smoothstep(-0.14f, 0.2f, h);
    const glm::vec3 dayHorizon(0.74f, 0.84f, 0.98f), nightHorizon(0.045f, 0.055f, 0.11f);
    const glm::vec3 dayZenith(0.40f, 0.60f, 0.96f), nightZenith(0.008f, 0.012f, 0.035f);
    glm::vec3 horizon = glm::mix(nightHorizon, dayHorizon, skyState.day);
    glm::vec3 zenith = glm::mix(nightZenith, dayZenith, skyState.day);
    const float sunset = std::exp(-(h / 0.2f) * (h / 0.2f));
    horizon = glm::mix(horizon, glm::vec3(1.0f, 0.52f, 0.28f), sunset * 0.7f);
    zenith = glm::mix(zenith, glm::vec3(0.35f, 0.28f, 0.55f), sunset * 0.35f);
    skyState.horizon = horizon;
    skyState.zenith = zenith;
}

void renderSky(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camera) {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glUseProgram(skyProgram);
    const glm::mat4 inverse = glm::inverse(projection * glm::mat4(glm::mat3(view)));
    glUniformMatrix4fv(locSkyInverse, 1, GL_FALSE, glm::value_ptr(inverse));
    glUniform3fv(locSkySun, 1, glm::value_ptr(skyState.sunDir));
    glUniform3fv(locSkyHorizon, 1, glm::value_ptr(skyState.horizon));
    glUniform3fv(locSkyZenith, 1, glm::value_ptr(skyState.zenith));
    glUniform3fv(locSkyCamera, 1, glm::value_ptr(camera));
    glUniform1f(locSkyDay, skyState.day);
    glUniform1f(locSkyTime, worldTime);
    glUniform1f(locSkyCloud, CloudHeight);
    glBindVertexArray(emptyVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

void bindVertexLayout() {
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, nx)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, u)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, ao)));
}

const MeshGpu& uploadMesh(int key, const std::vector<Vertex>& vertices) {
    MeshGpu mesh;
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                 vertices.data(), GL_STATIC_DRAW);
    bindVertexLayout();
    mesh.count = static_cast<GLsizei>(vertices.size());
    return meshCache[key] = mesh;
}

template <typename TileFn>
void appendCube(std::vector<Vertex>& vertices, TileFn tileOf) {
    const int order[6] = {0, 1, 2, 0, 2, 3};
    for (int face = 0; face < 6; ++face) {
        const int tile = tileOf(face);
        const glm::vec3 n(faceNormals[face]);
        for (int i : order) {
            const float* p = faceCorners[face][i];
            float u = 0.0f, v = 0.0f;
            faceUv(face, p, tile, u, v);
            vertices.push_back({p[0], p[1], p[2], n.x, n.y, n.z, u, v, 1.0f});
        }
    }
}

void appendQuad(std::vector<Vertex>& vertices, const glm::vec3 p[4], const glm::vec3& n, float u, float v) {
    const glm::vec3 c = glm::cross(p[1] - p[0], p[2] - p[0]);
    static const int forward[6] = {0, 1, 2, 0, 2, 3};
    static const int reverse[6] = {0, 2, 1, 0, 3, 2};
    const int* order = glm::dot(c, n) >= 0.0f ? forward : reverse;
    for (int i = 0; i < 6; ++i) {
        const glm::vec3& q = p[order[i]];
        vertices.push_back({q.x, q.y, q.z, n.x, n.y, n.z, u, v, 1.0f});
    }
}

void buildSpriteMesh(int tile, std::vector<Vertex>& out) {
    constexpr float step = 1.0f / 16.0f;
    constexpr float half = step * 0.5f;
    auto opaque = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= atl::TileSize || y >= atl::TileSize) return false;
        return atlasPixels[static_cast<std::size_t>(y * atl::Width + tile * atl::TileSize + x)].a != 0;
    };
    for (int y = 0; y < atl::TileSize; ++y) {
        for (int x = 0; x < atl::TileSize; ++x) {
            if (!opaque(x, y)) continue;
            const float u = (static_cast<float>(tile * atl::TileSize + x) + 0.5f) / static_cast<float>(atl::Width);
            const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(atl::TileSize);
            const float x0 = -0.5f + static_cast<float>(x) * step, x1 = x0 + step;
            const float y1 = 0.5f - static_cast<float>(y) * step, y0 = y1 - step;
            const glm::vec3 front[4] = {{x0, y0, half}, {x1, y0, half}, {x1, y1, half}, {x0, y1, half}};
            const glm::vec3 back[4] = {{x0, y0, -half}, {x1, y0, -half}, {x1, y1, -half}, {x0, y1, -half}};
            appendQuad(out, front, {0, 0, 1}, u, v);
            appendQuad(out, back, {0, 0, -1}, u, v);
            if (!opaque(x - 1, y)) {
                const glm::vec3 q[4] = {{x0, y0, -half}, {x0, y0, half}, {x0, y1, half}, {x0, y1, -half}};
                appendQuad(out, q, {-1, 0, 0}, u, v);
            }
            if (!opaque(x + 1, y)) {
                const glm::vec3 q[4] = {{x1, y0, -half}, {x1, y0, half}, {x1, y1, half}, {x1, y1, -half}};
                appendQuad(out, q, {1, 0, 0}, u, v);
            }
            if (!opaque(x, y - 1)) {
                const glm::vec3 q[4] = {{x0, y1, half}, {x1, y1, half}, {x1, y1, -half}, {x0, y1, -half}};
                appendQuad(out, q, {0, 1, 0}, u, v);
            }
            if (!opaque(x, y + 1)) {
                const glm::vec3 q[4] = {{x0, y0, -half}, {x1, y0, -half}, {x1, y0, half}, {x0, y0, half}};
                appendQuad(out, q, {0, -1, 0}, u, v);
            }
        }
    }
}

const MeshGpu& itemMesh(int id) {
    auto found = meshCache.find(id);
    if (found != meshCache.end()) return found->second;
    std::vector<Vertex> vertices;
    if (isBlockItem(id)) appendCube(vertices, [&](int face) { return tileForBlock(id, face); });
    else buildSpriteMesh(itemSpriteTile(id), vertices);
    return uploadMesh(id, vertices);
}

const MeshGpu& crackMesh(int stage) {
    const int key = 1000 + stage;
    auto found = meshCache.find(key);
    if (found != meshCache.end()) return found->second;
    std::vector<Vertex> vertices;
    appendCube(vertices, [&](int) { return atl::Crack0 + stage; });
    return uploadMesh(key, vertices);
}

void drawMesh(const MeshGpu& mesh, const glm::mat4& model) {
    glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(model));
    glBindVertexArray(mesh.vao);
    glDrawArrays(GL_TRIANGLES, 0, mesh.count);
}

void renderDrops(const glm::mat4& view, const glm::mat4& projection,
                 const glm::vec3& camera, const glm::vec3& fog) {
    if (itemDrops.empty()) return;
    setFrameUniforms(view, projection, camera, fog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlasTexture);
    glUniform1i(locTextured, 2);
    for (const mc::ItemDrop& drop : itemDrops) {
        const bool block = isBlockItem(drop.itemId);
        const float bob = std::sin(drop.age * 2.6f) * 0.05f + (block ? 0.0f : 0.09f);
        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(drop.x, drop.y + bob, drop.z));
        model = glm::rotate(model, drop.age * 1.4f, glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(block ? 0.26f : 0.42f));
        drawMesh(itemMesh(drop.itemId), model);
    }
}

void renderParticles(const glm::mat4& view, const glm::mat4& projection,
                     const glm::vec3& camera, const glm::vec3& fog) {
    if (particles.empty()) return;
    setFrameUniforms(view, projection, camera, fog);
    glUniform1i(locTextured, 0);
    glUniform1f(locSolidLight, 0.3f + 0.7f * skyState.day);
    glBindVertexArray(cubeVAO);
    for (const Particle& p : particles) {
        glUniform3fv(locSolidColor, 1, glm::value_ptr(p.color));
        glm::mat4 model = glm::translate(glm::mat4(1.0f), p.position);
        model = glm::scale(model, glm::vec3(p.size));
        glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(model));
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    glUniform1f(locSolidLight, 1.0f);
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
    glm::vec3 maximum((cx + 1) * CHUNK_SIZE - 0.5f, mc::world::WorldHeight - 0.5f,
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
    const float fogEnd = static_cast<float>(renderDistanceChunks * CHUNK_SIZE) * 0.94f;
    glUniform1f(locFogStart, fogEnd * 0.42f);
    glUniform1f(locFogEnd, fogEnd);
    glUniform1f(locDay, skyState.day);
    glUniform1f(locAlpha, 1.0f);
    glUniform1f(locSolidLight, 1.0f);
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
    const auto positions = infiniteWorld.visibleChunks(
        static_cast<int>(std::floor(camera.x)), static_cast<int>(std::floor(camera.z)), renderDistanceChunks);
    for (const auto& position : positions) {
        auto found = streamedMeshes.find(position);
        if (found == streamedMeshes.end()) continue;
        ChunkMesh& mesh = found->second;
        if (!mesh.vertexCount || !chunkVisible(planes, position.x, position.z)) continue;
        glm::mat4 model = glm::translate(glm::mat4(1.0f),
            glm::vec3(position.x * CHUNK_SIZE, 0.0f, position.z * CHUNK_SIZE));
        glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(model));
        glBindVertexArray(mesh.vao);
        glDrawArrays(GL_TRIANGLES, 0, mesh.vertexCount);
        ++visible;
    }
    return visible;
}

void renderTarget(const glm::mat4& view, const glm::mat4& projection,
                  const glm::vec3& camera, const glm::vec3& fog) {
    glm::ivec3 hit, previous;
    if (!raycast(hit, previous)) return;
    setFrameUniforms(view, projection, camera, fog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlasTexture);
    const glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(hit));
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (miningProgress > 0.0f && hit == miningTarget) {
        const int stage = std::min(9, static_cast<int>(miningProgress * 10.0f));
        glUniform1i(locTextured, 3);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(-1.0f, -1.0f);
        drawMesh(crackMesh(stage), glm::scale(model, glm::vec3(1.002f)));
        glDisable(GL_POLYGON_OFFSET_FILL);
    }
    glUniform1i(locTextured, 0);
    glUniform3f(locSolidColor, 0.0f, 0.0f, 0.0f);
    glUniform1f(locAlpha, 0.65f);
    glLineWidth(2.0f);
    glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(glm::scale(model, glm::vec3(1.003f))));
    glBindVertexArray(edgeVAO);
    glDrawArrays(GL_LINES, 0, 24);
    glUniform1f(locAlpha, 1.0f);
    glDisable(GL_BLEND);
}

void renderFirstPersonHand(const glm::mat4& view, const glm::mat4& projection,
                           const glm::vec3& camera) {
    glClear(GL_DEPTH_BUFFER_BIT);
    const glm::vec3 front = cameraFront();
    const glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
    const glm::vec3 up = glm::normalize(glm::cross(right, front));
    glm::mat4 basis(1.0f);
    basis[0] = glm::vec4(right, 0.0f);
    basis[1] = glm::vec4(up, 0.0f);
    basis[2] = glm::vec4(-front, 0.0f);
    basis[3] = glm::vec4(camera, 1.0f);
    const float swing = handSwingTime > 0.0f ? std::sin((1.0f - handSwingTime) * 3.1415926f) : 0.0f;
    float bobX = 0.0f, bobY = 0.0f;
    if (viewBobbing && playerMoving) {
        bobX = std::sin(walkAnimationTime * 10.0f) * 0.018f;
        bobY = -std::fabs(std::cos(walkAnimationTime * 10.0f)) * 0.016f;
    }
    const mc::ItemStack held = inventory.slot(static_cast<std::size_t>(selectedBlock - 1));
    const bool hasItem = held.itemId > 0 && held.count > 0;
    setFrameUniforms(view, projection, camera, skyState.horizon);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlasTexture);
    const float lightFactor = 0.35f + 0.65f * skyState.day;
    glm::mat4 model = glm::translate(basis, glm::vec3(0.46f + bobX - swing * 0.18f,
                                                      (hasItem && !isBlockItem(held.itemId) ? -0.3f : -0.44f) + bobY - swing * 0.16f,
                                                      -0.78f - swing * 0.22f));
    if (!hasItem) {
        model = glm::translate(model, glm::vec3(0.0f, -0.12f, 0.2f));
        model = glm::rotate(model, glm::radians(-48.0f - swing * 40.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, glm::radians(-12.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(0.2f, 0.2f, 0.62f));
        glUniform1i(locTextured, 0);
        glUniform3f(locSolidColor, 0.72f, 0.48f, 0.34f);
        glUniform1f(locSolidLight, lightFactor);
        glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(model));
        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glUniform1f(locSolidLight, 1.0f);
        return;
    }
    glUniform1i(locTextured, 2);
    if (isBlockItem(held.itemId)) {
        model = glm::rotate(model, glm::radians(-10.0f - swing * 45.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, glm::radians(32.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(0.34f));
    } else {
        model = glm::translate(model, glm::vec3(-0.02f, 0.06f, 0.0f));
        model = glm::rotate(model, glm::radians(-swing * 50.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, glm::radians(-38.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, glm::radians(-8.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, glm::vec3(0.5f));
    }
    drawMesh(itemMesh(held.itemId), model);
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
        case 25: return {0.56f,0.59f,0.64f};
        case 26: return {0.62f,0.64f,0.67f};
        case 27: return {0.68f,0.70f,0.74f};
        default: return {0.2f,0.2f,0.2f};
    }
}

std::string uiItemName(int id) {
    switch (id) {
        case 1: return "Grass";
        case 2: return "Dirt";
        case 3: return "Stone";
        case 4: return "Log";
        case 5: return "Leaves";
        case 6: return "Sand";
        case 7: return "Snow";
        case 8: return "Ore";
        case 9: return "Planks";
        case 10: return "Sticks";
        case 11: return "Crafting Table";
        case 20: return "Wooden Pickaxe";
        case 21: return "Stone Pickaxe";
        case 22: return "Wooden Axe";
        case 23: return "Wooden Shovel";
        case 24: return "Wooden Sword";
        case 25: return "Stone Axe";
        case 26: return "Stone Shovel";
        case 27: return "Stone Sword";
        default: return "Item";
    }
}

void drawUiRect(int x, int y, int width, int height, const glm::vec3& color) {
    if (width <= 0 || height <= 0) return;
    glScissor(x, y, width, height);
    glClearColor(color.r, color.g, color.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

struct UiVertex {
    float x, y, u, v, shade;
};

void drawUiVertices(const std::vector<UiVertex>& vertices, int mode, const glm::vec4& tint) {
    if (vertices.empty()) return;
    glScissor(0, 0, screenWidth, screenHeight);
    glUseProgram(uiProgram);
    glUniform2f(locUiScreen, static_cast<float>(screenWidth), static_cast<float>(screenHeight));
    glUniform4fv(locUiTint, 1, glm::value_ptr(tint));
    glUniform1i(locUiMode, mode);
    glUniform1i(locUiAtlas, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlasTexture);
    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(UiVertex)),
                 vertices.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
}

void uiQuad(std::vector<UiVertex>& v, const float p[4][2], const float uv[4][2], float shade) {
    const int order[6] = {0, 1, 2, 0, 2, 3};
    for (int i : order) v.push_back({p[i][0], p[i][1], uv[i][0], uv[i][1], shade});
}

void tileCorners(int tile, float uv[4][2]) {
    const float width = static_cast<float>(atl::Width);
    const float u0 = (static_cast<float>(tile * atl::TileSize) + 0.5f) / width;
    const float u1 = (static_cast<float>((tile + 1) * atl::TileSize) - 0.5f) / width;
    const float v0 = 0.5f / 16.0f, v1 = 15.5f / 16.0f;
    const float c[4][2] = {{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}};
    for (int i = 0; i < 4; ++i) { uv[i][0] = c[i][0]; uv[i][1] = c[i][1]; }
}

void drawUiAlpha(int x, int y, int width, int height, const glm::vec3& color, float alpha) {
    std::vector<UiVertex> v;
    const float p[4][2] = {{float(x), float(y)}, {float(x + width), float(y)},
                           {float(x + width), float(y + height)}, {float(x), float(y + height)}};
    const float uv[4][2] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
    uiQuad(v, p, uv, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    drawUiVertices(v, 1, glm::vec4(color, alpha));
    glDisable(GL_BLEND);
}

void drawUiTiledBackground(int tile, int size, float shade) {
    std::vector<UiVertex> v;
    float uv[4][2];
    tileCorners(tile, uv);
    for (int y = 0; y < screenHeight; y += size)
        for (int x = 0; x < screenWidth; x += size) {
            const float p[4][2] = {{float(x), float(y)}, {float(x + size), float(y)},
                                   {float(x + size), float(y + size)}, {float(x), float(y + size)}};
            const float flipped[4][2] = {{uv[3][0], uv[3][1]}, {uv[2][0], uv[2][1]}, {uv[1][0], uv[1][1]}, {uv[0][0], uv[0][1]}};
            uiQuad(v, p, flipped, shade);
        }
    drawUiVertices(v, 0, glm::vec4(1.0f));
}

void drawUiIcon(int x, int y, int size, int itemId) {
    std::vector<UiVertex> v;
    const float w = static_cast<float>(size);
    const float fx = static_cast<float>(x), fy = static_cast<float>(y);
    float uv[4][2];
    if (isBlockItem(itemId)) {
        const float cx = fx + w * 0.5f;
        const float top[4][2] = {{cx, fy + w}, {fx + w, fy + w * 0.75f}, {cx, fy + w * 0.5f}, {fx, fy + w * 0.75f}};
        tileCorners(tileForBlock(itemId, 2), uv);
        uiQuad(v, top, uv, 1.0f);
        const float left[4][2] = {{fx, fy + w * 0.75f}, {cx, fy + w * 0.5f}, {cx, fy}, {fx, fy + w * 0.25f}};
        tileCorners(tileForBlock(itemId, 0), uv);
        uiQuad(v, left, uv, 0.8f);
        const float right[4][2] = {{cx, fy + w * 0.5f}, {fx + w, fy + w * 0.75f}, {fx + w, fy + w * 0.25f}, {cx, fy}};
        tileCorners(tileForBlock(itemId, 4), uv);
        uiQuad(v, right, uv, 0.6f);
    } else {
        const float quad[4][2] = {{fx, fy}, {fx + w, fy}, {fx + w, fy + w}, {fx, fy + w}};
        tileCorners(itemSpriteTile(itemId), uv);
        const float flipped[4][2] = {{uv[3][0], uv[3][1]}, {uv[2][0], uv[2][1]}, {uv[1][0], uv[1][1]}, {uv[0][0], uv[0][1]}};
        uiQuad(v, quad, flipped, 1.0f);
    }
    drawUiVertices(v, 0, glm::vec4(1.0f));
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
    drawUiIcon(x, y, 32, itemId);
}

void drawUI() {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_SCISSOR_TEST);

    const int centerX = screenWidth / 2;
    const int centerY = screenHeight / 2;
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
                        const glm::vec3& color, bool centered = false, bool shadow = true) {
        int width = value.empty() ? 0 : static_cast<int>(value.size()) * 6 * scale - scale;
        for (int pass = shadow ? 0 : 1; pass < 2; ++pass) {
            int cursor = centered ? x - width / 2 : x;
            const int offset = pass == 0 ? std::max(1, scale / 2 + (scale > 2 ? 1 : 0)) : 0;
            const glm::vec3 tone = pass == 0 ? color * 0.22f : color;
            for (char c : value) {
                auto rows = glyph(c);
                for (int row = 0; row < 7; ++row)
                    for (int column = 0; column < 5; ++column)
                        if (rows[static_cast<std::size_t>(row)] & (1u << (4 - column)))
                            drawUiRect(cursor + column * scale + offset, y + (6 - row) * scale - offset,
                                       scale, scale, tone);
                cursor += 6 * scale;
            }
        }
    };

    auto drawButton = [&](int x, int y, int width, int height, const std::string& label,
                          bool highlighted = false) {
        drawUiRect(x, y, width, height, {0.0f,0.0f,0.0f});
        drawUiRect(x + 1, y + 1, width - 2, height - 2, highlighted ? glm::vec3(0.30f,0.32f,0.45f) : glm::vec3(0.23f,0.23f,0.23f));
        drawUiRect(x + 1, y + 2, width - 3, height - 3, highlighted ? glm::vec3(0.72f,0.76f,0.98f) : glm::vec3(0.66f,0.66f,0.66f));
        drawUiRect(x + 3, y + 3, width - 6, height - 6, highlighted ? glm::vec3(0.40f,0.43f,0.66f) : glm::vec3(0.44f,0.44f,0.44f));
        drawText(label, x + width / 2, y + (height - 7 * 2) / 2 + 1, 2,
                 highlighted ? glm::vec3(1.0f,1.0f,0.63f) : glm::vec3(0.88f,0.88f,0.88f), true);
    };

    if (uiScreen == UiScreen::MainMenu) {
        drawUiTiledBackground(atl::Dirt, 64, 0.32f);
        drawText("MINICRAFT 3D", centerX, centerY + 112, 5, white, true);
        drawText("A VOXEL SURVIVAL GAME", centerX, centerY + 78, 1, muted, true);
        drawButton(centerX - 130, centerY + 48, 260, 42, "SINGLEPLAYER", true);
        drawButton(centerX - 130, centerY - 4, 260, 42, "SETTINGS");
        drawButton(centerX - 130, centerY - 56, 260, 42, "QUIT GAME");
        drawText("28.0 EXPANSION", centerX, centerY - 145, 1, {0.75f,0.75f,0.75f}, true);
    }

    if (uiScreen == UiScreen::WorldSelect) {
        drawUiTiledBackground(atl::Dirt, 64, 0.32f);
        drawText("SELECT WORLD", centerX, centerY + 190, 3, white, true);
        for (std::size_t i = 0; i < worldNames.size() && i < 5; ++i) {
            int rowY = centerY + 82 - static_cast<int>(i) * 43;
            drawButton(centerX - 190, rowY, 380, 34, worldNames[i], i == 0);
        }
        if (worldNames.empty())
            drawText("NO SAVED WORLDS", centerX, centerY + 25, 2, muted, true);
        drawButton(centerX - 190, centerY - 145, 380, 38, "CREATE NEW WORLD", true);
        drawButton(centerX - 190, centerY - 195, 380, 34, "BACK");
    }

    if (uiScreen == UiScreen::Crafting) {
        drawUiAlpha(0, 0, screenWidth, screenHeight, {0.0f,0.0f,0.0f}, 0.6f);
        drawText("RECIPE BOOK", centerX, centerY + 205, 3, white, true);
        for (std::size_t i = 0; i < mc::Crafting::recipeCount(); ++i) {
            const mc::Recipe& recipe = mc::Crafting::recipe(i);
            int rowY = centerY + 112 - static_cast<int>(i) * 36;
            bool available = mc::Crafting::canCraft(inventory, i);
            drawButton(centerX - 200, rowY, 400, 26, recipe.name, available);
        }
        drawButton(centerX - 210, centerY - 220, 420, 34, "BACK", true);
    }

    if (uiScreen == UiScreen::Pause || uiScreen == UiScreen::Settings) {
        const int panelWidth = uiScreen == UiScreen::Pause ? 360 : 430;
        const int panelHeight = uiScreen == UiScreen::Pause ? 340 : 500;
        const int left = centerX - panelWidth / 2;
        const int bottom = centerY - panelHeight / 2;
        drawUiAlpha(0, 0, screenWidth, screenHeight, {0.0f,0.0f,0.0f}, 0.6f);
        drawUiRect(left + 16, bottom + panelHeight - 64, panelWidth - 32, 2, {0.45f,0.45f,0.45f});
        drawText(uiScreen == UiScreen::Pause ? "GAME PAUSED" : "SETTINGS",
                 centerX, bottom + panelHeight - 48, 3, white, true);
        if (uiScreen == UiScreen::Pause) {
            drawButton(centerX - 116, centerY + 48, 232, 38, "RESUME", true);
            drawButton(centerX - 116, centerY + 1, 232, 38, "SAVE WORLD");
            drawButton(centerX - 116, centerY - 46, 232, 38, "SETTINGS");
            drawButton(centerX - 116, centerY - 93, 232, 38, "QUIT GAME");
            drawText(lastCraftMessage.empty() ? "ESC  RESUME" : lastCraftMessage,
                     centerX, bottom + 13, 1, muted, true);
        } else {
            drawText("FIELD OF VIEW", left + 28, centerY + 140, 2, white);
            drawButton(left + 274, centerY + 127, 42, 34, "-");
            drawText(std::to_string(static_cast<int>(fieldOfView)), left + 336, centerY + 140, 2, white, true);
            drawButton(left + 370, centerY + 127, 42, 34, "+");
            drawText("MOUSE SENSITIVITY", left + 28, centerY + 78, 2, white);
            drawButton(left + 274, centerY + 65, 42, 34, "-");
            drawText(std::to_string(static_cast<int>(mouseSensitivity * 1000.0f)),
                     left + 336, centerY + 78, 2, white, true);
            drawButton(left + 370, centerY + 65, 42, 34, "+");
            drawText("RENDER DISTANCE", left + 28, centerY + 16, 2, white);
            drawButton(left + 274, centerY + 3, 42, 34, "-");
            drawText(std::to_string(renderDistanceChunks), left + 336, centerY + 16, 2, white, true);
            drawButton(left + 370, centerY + 3, 42, 34, "+");
            drawText("DEBUG INFO", left + 28, centerY - 46, 2, white);
            drawButton(left + 274, centerY - 59, 138, 34, showDebugInfo ? "ON" : "OFF", showDebugInfo);
            drawText("VIEW BOBBING", left + 28, centerY - 108, 2, white);
            drawButton(left + 274, centerY - 121, 138, 34, viewBobbing ? "ON" : "OFF", viewBobbing);
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
    }

    const int hotbarBox = 48;
    const int hotbarGap = 4;
    const int hotbarTotal = HOTBAR_SIZE * hotbarBox + (HOTBAR_SIZE - 1) * hotbarGap;
    const int hotbarStart = (screenWidth - hotbarTotal) / 2;
    const int hotbarY = 18;
    drawUiRect(hotbarStart - 7, hotbarY - 7, hotbarTotal + 14, hotbarBox + 14, {0.0f,0.0f,0.0f});
    drawUiRect(hotbarStart - 5, hotbarY - 5, hotbarTotal + 10, hotbarBox + 10, {0.30f,0.30f,0.30f});

    auto drawSlot = [&](int x, int y, int box, const mc::ItemStack& stack,
                        bool selected, std::size_t slotIndex, int hotbarNumber) {
        (void)hotbarNumber;
        if (selected) drawUiRect(x - 3, y - 3, box + 6, box + 6, {1.0f,1.0f,1.0f});
        drawUiRect(x, y, box, box, {1.0f,1.0f,1.0f});
        drawUiRect(x, y + 2, box - 2, box - 2, {0.22f,0.22f,0.22f});
        drawUiRect(x + 2, y + 2, box - 4, box - 4, {0.545f,0.545f,0.545f});
        if (stack.itemId > 0 && stack.count > 0) {
            int iconSize = 32;
            drawUiItemIcon(x + (box - iconSize) / 2, y + (box - iconSize) / 2 + 1, stack.itemId);
            if (stack.count > 1) {
                drawUiNumber(stack.count, x + box - 3, y + 3, 1, {0.2f,0.2f,0.2f});
                drawUiNumber(stack.count, x + box - 4, y + 4, 1, white);
            }
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
        drawUiRect(start - 15, 64, total + 30, 194, {0.0f,0.0f,0.0f});
        drawUiRect(start - 14, 65, total + 28, 192, {0.33f,0.33f,0.33f});
        drawUiRect(start - 14, 67, total + 26, 190, {1.0f,1.0f,1.0f});
        drawUiRect(start - 12, 67, total + 24, 188, {0.776f,0.776f,0.776f});
        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 8; ++column) {
                int index = 8 + row * 8 + column;
                int x = start + column * (box + gap);
                int y = 18 + (3 - row) * (box + gap) + 12;
                drawSlot(x, y, box, inventory.slot(static_cast<std::size_t>(index)),
                         false, static_cast<std::size_t>(index), 0);
            }
        }
        drawText("INVENTORY", start - 4, 236, 2, {0.25f,0.25f,0.25f}, false, false);
    }

    for (int i = 0; i < HOTBAR_SIZE; ++i) {
        int x = hotbarStart + i * (hotbarBox + hotbarGap);
        drawSlot(x, hotbarY, hotbarBox, inventory.slot(static_cast<std::size_t>(i)),
                 i + 1 == selectedBlock, static_cast<std::size_t>(i), i + 1);
    }

    if (!inventoryOpen) {
        const mc::ItemStack& selectedStack = inventory.slot(static_cast<std::size_t>(selectedBlock - 1));
        if (selectedStack.itemId > 0)
            drawText(uiItemName(selectedStack.itemId), centerX, hotbarY + hotbarBox + 29, 2, white, true);
        const int iconY = hotbarY + hotbarBox + 13;
        const int iconStart = centerX - 190;
        for (int i = 0; i < 10; ++i) {
            float healthPart = std::clamp((playerStats.health() - i * 2.0f) / 2.0f, 0.0f, 1.0f);
            float hungerPart = std::clamp((playerStats.hunger() - i * 2.0f) / 2.0f, 0.0f, 1.0f);
            drawSurvivalIcon(iconStart + i * 18, iconY, healthPart, false);
            drawSurvivalIcon(centerX + 10 + i * 18, iconY, hungerPart, true);
        }
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

    GLFWwindow* window = glfwCreateWindow(screenWidth, screenHeight, "MiniCraft3D | 28.0", nullptr, nullptr);
    if (!window) {
        std::cerr << "OpenGL window creation failed\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    loadSettings();
    glfwSetFramebufferSizeCallback(window, framebufferCallback);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
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
    for (ChunkMesh& chunk : chunks) chunk.dirty = true;

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
        handSwingTime = std::max(0.0f, handSwingTime - dt * 3.8f);
        if (uiScreen == UiScreen::Gameplay && playerMoving)
            walkAnimationTime += dt * (playerSprinting ? 1.7f : 1.0f);

        processInput(window, dt);
        processUiMouse(window);
        processInventoryMouse(window);
        processBlocks(window, dt);
        if (uiScreen == UiScreen::Gameplay) rebuildDirtyChunks();
        if (uiScreen == UiScreen::Gameplay) playerStats.tick(dt, playerMoving, playerSprinting);
        if (!playerStats.alive()) {
            player = glm::vec3(64.0f, terrainHeight(64, 64) + 2.2f, 64.0f);
            velocity = glm::vec3(0.0f);
            playerStats.set(mc::PlayerStats::MaxHealth, 12.0f);
            lastCraftMessage = "Respawned";
        }
        if (uiScreen == UiScreen::Gameplay) updateDrops(dt);
        if (uiScreen == UiScreen::Gameplay) updateParticles(dt);

        updateSky();
        const glm::vec3 sky = skyState.horizon;
        glClearColor(sky.r, sky.g, sky.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(glm::radians(fieldOfView),
            static_cast<float>(screenWidth) / static_cast<float>(screenHeight), 0.1f,
            static_cast<float>(renderDistanceChunks * CHUNK_SIZE));
        glm::vec3 front = cameraFront();
        glm::vec3 cameraPosition = player;
        if (viewBobbing && uiScreen == UiScreen::Gameplay && playerMoving)
            cameraPosition.y += std::sin(walkAnimationTime * 10.0f) * 0.045f;
        glm::mat4 view = glm::lookAt(cameraPosition, cameraPosition + front, glm::vec3(0,1,0));

        renderSky(view, projection, cameraPosition);
        visibleChunks = renderWorld(view, projection, cameraPosition, sky);
        renderDrops(view, projection, cameraPosition, sky);
        renderParticles(view, projection, cameraPosition, sky);
        renderTarget(view, projection, cameraPosition, sky);
        if (uiScreen == UiScreen::Gameplay && !inventoryOpen)
            renderFirstPersonHand(view, projection, cameraPosition);
        drawUI();

        glfwSwapBuffers(window);
        glfwPollEvents();

        ++frames;
        titleTimer += dt;
        if (titleTimer >= 0.5) {
            fps = frames / static_cast<float>(titleTimer);
            frames = 0;
            titleTimer = 0.0;
            std::string title = "MiniCraft3D 28.0";
            if (showDebugInfo) {
                title += " | " + std::to_string(static_cast<int>(fps)) + " FPS | Chunks " +
                    std::to_string(visibleChunks) + "/" + std::to_string(infiniteWorld.loadedCount()) +
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

    if (worldInitialized) saveWorld();
    for (auto& entry : streamedMeshes) {
        glDeleteVertexArrays(1, &entry.second.vao);
        glDeleteBuffers(1, &entry.second.vbo);
    }
    streamedMeshes.clear();
    saveSettings();
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
