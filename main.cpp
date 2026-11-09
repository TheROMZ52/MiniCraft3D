
#define GLFW_INCLUDE_NONE
#define NOMINMAX
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <algorithm>

const int W = 32, D = 32, H = 16;
int world[W][H][D]{};

glm::vec3 player(16.0f, 8.0f, 16.0f);
glm::vec3 velocity(0.0f);
float yaw = -90.0f;
float pitch = 0.0f;
float lastX = 640.0f, lastY = 360.0f;
bool firstMouse = true;
bool grounded = false;
int selectedBlock = 1;
bool leftWasDown = false;
bool rightWasDown = false;

GLuint VAO, VBO, shaderProgram;
int width = 1280, height = 720;

const char* vertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

out vec3 Normal;
out vec3 FragPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    vec4 worldPos = model * vec4(aPos, 1.0);
    FragPos = worldPos.xyz;
    Normal = mat3(transpose(inverse(model))) * aNormal;
    gl_Position = projection * view * worldPos;
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
in vec3 Normal;
in vec3 FragPos;

out vec4 FragColor;

uniform vec3 blockColor;

void main()
{
    vec3 lightDir = normalize(vec3(-0.5, 1.0, -0.3));
    float diffuse = max(dot(normalize(Normal), lightDir), 0.0);
    float lighting = 0.38 + diffuse * 0.62;
    FragColor = vec4(blockColor * lighting, 1.0);
}
)";

float cubeVertices[] = {
    -0.5f,-0.5f, 0.5f, 0,0,1,  0.5f,-0.5f, 0.5f, 0,0,1,  0.5f,0.5f,0.5f, 0,0,1,
     0.5f,0.5f,0.5f, 0,0,1, -0.5f,0.5f,0.5f, 0,0,1, -0.5f,-0.5f,0.5f, 0,0,1,

    -0.5f,-0.5f,-0.5f, 0,0,-1, -0.5f,0.5f,-0.5f, 0,0,-1, 0.5f,0.5f,-0.5f, 0,0,-1,
     0.5f,0.5f,-0.5f, 0,0,-1, 0.5f,-0.5f,-0.5f, 0,0,-1, -0.5f,-0.5f,-0.5f, 0,0,-1,

    -0.5f,0.5f,0.5f, 0,1,0, 0.5f,0.5f,0.5f, 0,1,0, 0.5f,0.5f,-0.5f, 0,1,0,
     0.5f,0.5f,-0.5f, 0,1,0, -0.5f,0.5f,-0.5f, 0,1,0, -0.5f,0.5f,0.5f, 0,1,0,

    -0.5f,-0.5f,0.5f, 0,-1,0, -0.5f,-0.5f,-0.5f, 0,-1,0, 0.5f,-0.5f,-0.5f, 0,-1,0,
     0.5f,-0.5f,-0.5f, 0,-1,0, 0.5f,-0.5f,0.5f, 0,-1,0, -0.5f,-0.5f,0.5f, 0,-1,0,

     0.5f,-0.5f,0.5f, 1,0,0, 0.5f,-0.5f,-0.5f, 1,0,0, 0.5f,0.5f,-0.5f, 1,0,0,
     0.5f,0.5f,-0.5f, 1,0,0, 0.5f,0.5f,0.5f, 1,0,0, 0.5f,-0.5f,0.5f, 1,0,0,

    -0.5f,-0.5f,0.5f, -1,0,0, -0.5f,0.5f,0.5f, -1,0,0, -0.5f,0.5f,-0.5f, -1,0,0,
    -0.5f,0.5f,-0.5f, -1,0,0, -0.5f,-0.5f,-0.5f, -1,0,0, -0.5f,-0.5f,0.5f, -1,0,0
};

glm::vec3 blockColor(int id)
{
    switch (id)
    {
        case 1: return {0.25f, 0.68f, 0.19f};
        case 2: return {0.48f, 0.29f, 0.13f};
        case 3: return {0.48f, 0.49f, 0.52f};
        case 4: return {0.48f, 0.28f, 0.10f};
        case 5: return {0.12f, 0.43f, 0.12f};
        default: return {1.0f, 1.0f, 1.0f};
    }
}

GLuint compileShader(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << log << '\n';
    }

    return shader;
}

void createShader()
{
    GLuint vertex = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertex);
    glAttachShader(shaderProgram, fragment);
    glLinkProgram(shaderProgram);

    GLint success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);

    if (!success)
    {
        char log[1024];
        glGetProgramInfoLog(shaderProgram, sizeof(log), nullptr, log);
        std::cerr << log << '\n';
    }

    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

int terrainHeight(int x, int z)
{
    float a = std::sin(x * 0.35f) * 1.2f;
    float b = std::cos(z * 0.29f) * 1.1f;
    float c = std::sin((x + z) * 0.18f) * 0.8f;
    return std::clamp(3 + (int)std::round(a + b + c), 1, 7);
}

void generateWorld()
{
    for (int x = 0; x < W; x++)
    {
        for (int z = 0; z < D; z++)
        {
            int top = terrainHeight(x, z);

            for (int y = 0; y <= top; y++)
            {
                if (y == top)
                    world[x][y][z] = 1;
                else if (y >= top - 2)
                    world[x][y][z] = 2;
                else
                    world[x][y][z] = 3;
            }
        }
    }

    for (int x = 5; x < W - 5; x += 8)
    {
        for (int z = 5; z < D - 5; z += 9)
        {
            int ground = terrainHeight(x, z);

            for (int y = 1; y <= 3; y++)
                if (ground + y < H)
                    world[x][ground + y][z] = 4;

            for (int dx = -2; dx <= 2; dx++)
            {
                for (int dz = -2; dz <= 2; dz++)
                {
                    for (int dy = 2; dy <= 4; dy++)
                    {
                        int xx = x + dx;
                        int yy = ground + dy;
                        int zz = z + dz;

                        if (xx >= 0 && xx < W && zz >= 0 && zz < D &&
                            yy >= 0 && yy < H &&
                            std::abs(dx) + std::abs(dz) + std::abs(dy - 3) <= 4)
                        {
                            if (world[xx][yy][zz] == 0)
                                world[xx][yy][zz] = 5;
                        }
                    }
                }
            }
        }
    }

    player = glm::vec3(16.0f, terrainHeight(16, 16) + 2.15f, 16.0f);
}

bool solidAt(int x, int y, int z)
{
    if (x < 0 || x >= W || z < 0 || z >= D || y < 0)
        return true;

    if (y >= H)
        return false;

    return world[x][y][z] != 0;
}

bool collides(glm::vec3 p)
{
    float minX = p.x - 0.3f;
    float maxX = p.x + 0.3f;
    float minY = p.y - 1.65f;
    float maxY = p.y + 0.1f;
    float minZ = p.z - 0.3f;
    float maxZ = p.z + 0.3f;

    for (int x = (int)std::floor(minX + 0.5f); x <= (int)std::floor(maxX + 0.5f); x++)
    {
        for (int y = (int)std::floor(minY + 0.5f); y <= (int)std::floor(maxY + 0.5f); y++)
        {
            for (int z = (int)std::floor(minZ + 0.5f); z <= (int)std::floor(maxZ + 0.5f); z++)
            {
                if (solidAt(x, y, z))
                    return true;
            }
        }
    }

    return false;
}

glm::vec3 cameraFront()
{
    glm::vec3 front;
    front.x = std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch));
    front.y = std::sin(glm::radians(pitch));
    front.z = std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch));
    return glm::normalize(front);
}

void mouseCallback(GLFWwindow*, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
    }

    float xoffset = (float)xpos - lastX;
    float yoffset = lastY - (float)ypos;

    lastX = (float)xpos;
    lastY = (float)ypos;

    xoffset *= 0.12f;
    yoffset *= 0.12f;

    yaw += xoffset;
    pitch += yoffset;
    pitch = std::clamp(pitch, -89.0f, 89.0f);
}

bool raycast(glm::ivec3& hit, glm::ivec3& previous)
{
    glm::vec3 direction = cameraFront();
    glm::vec3 origin = player;

    glm::ivec3 last(-1);

    for (float distance = 0.1f; distance <= 6.0f; distance += 0.05f)
    {
        glm::vec3 point = origin + direction * distance;
        glm::ivec3 cell(
            (int)std::floor(point.x + 0.5f),
            (int)std::floor(point.y + 0.5f),
            (int)std::floor(point.z + 0.5f)
        );

        if (cell == last)
            continue;

        last = cell;

        if (cell.x >= 0 && cell.x < W &&
            cell.y >= 0 && cell.y < H &&
            cell.z >= 0 && cell.z < D &&
            world[cell.x][cell.y][cell.z] != 0)
        {
            hit = cell;
            previous = glm::ivec3(
                (int)std::floor(origin.x + direction.x * (distance - 0.05f) + 0.5f),
                (int)std::floor(origin.y + direction.y * (distance - 0.05f) + 0.5f),
                (int)std::floor(origin.z + direction.z * (distance - 0.05f) + 0.5f)
            );
            return true;
        }
    }

    return false;
}

void processInput(GLFWwindow* window, float dt)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) selectedBlock = 1;
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) selectedBlock = 2;
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) selectedBlock = 3;
    if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) selectedBlock = 4;
    if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS) selectedBlock = 5;

    glm::vec3 front = cameraFront();
    glm::vec3 forward = glm::normalize(glm::vec3(front.x, 0.0f, front.z));
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));

    float speed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 8.0f : 5.0f;
    glm::vec3 movement(0.0f);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) movement += forward;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) movement -= forward;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) movement += right;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) movement -= right;

    if (glm::length(movement) > 0.0f)
        movement = glm::normalize(movement) * speed * dt;

    glm::vec3 next = player;
    next.x += movement.x;
    if (!collides(next)) player.x = next.x;

    next = player;
    next.z += movement.z;
    if (!collides(next)) player.z = next.z;

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && grounded)
    {
        velocity.y = 7.0f;
        grounded = false;
    }

    velocity.y -= 19.0f * dt;
    velocity.y = std::max(velocity.y, -25.0f);

    next = player;
    next.y += velocity.y * dt;

    if (!collides(next))
    {
        player.y = next.y;
        grounded = false;
    }
    else
    {
        if (velocity.y < 0.0f)
            grounded = true;

        velocity.y = 0.0f;
    }

    if (player.y < -10.0f)
    {
        player = glm::vec3(16.0f, terrainHeight(16, 16) + 2.15f, 16.0f);
        velocity = glm::vec3(0.0f);
    }
}

void processBlocks(GLFWwindow* window)
{
    glm::ivec3 hit, previous;
    bool found = raycast(hit, previous);

    bool left = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool right = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

    if (left && !leftWasDown && found)
        world[hit.x][hit.y][hit.z] = 0;

    if (right && !rightWasDown && found)
    {
        if (previous.x >= 0 && previous.x < W &&
            previous.y >= 0 && previous.y < H &&
            previous.z >= 0 && previous.z < D)
        {
            if (world[previous.x][previous.y][previous.z] == 0)
            {
                int old = world[previous.x][previous.y][previous.z];
                world[previous.x][previous.y][previous.z] = selectedBlock;

                if (collides(player))
                    world[previous.x][previous.y][previous.z] = old;
            }
        }
    }

    leftWasDown = left;
    rightWasDown = right;
}

void renderWorld(glm::mat4 view, glm::mat4 projection)
{
    glUseProgram(shaderProgram);

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    glBindVertexArray(VAO);

    for (int x = 0; x < W; x++)
    {
        for (int y = 0; y < H; y++)
        {
            for (int z = 0; z < D; z++)
            {
                int id = world[x][y][z];
                if (!id)
                    continue;

                glm::vec3 color = blockColor(id);

                glUniform3fv(glGetUniformLocation(shaderProgram, "blockColor"), 1, glm::value_ptr(color));

                glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
                glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));

                glDrawArrays(GL_TRIANGLES, 0, 36);
            }
        }
    }
}

void drawUI()
{
    glDisable(GL_DEPTH_TEST);
    glUseProgram(0);

    glEnable(GL_SCISSOR_TEST);

    int cx = width / 2;
    int cy = height / 2;

    glScissor(cx - 1, cy - 8, 2, 16);
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glScissor(cx - 8, cy - 1, 16, 2);
    glClear(GL_COLOR_BUFFER_BIT);

    int box = 34;
    int gap = 6;
    int total = 5 * box + 4 * gap;
    int start = (width - total) / 2;

    for (int i = 0; i < 5; i++)
    {
        int x = start + i * (box + gap);
        glScissor(x, 20, box, box);

        glm::vec3 c = blockColor(i + 1);
        if (i + 1 == selectedBlock)
            c = glm::vec3(1.0f, 0.85f, 0.15f);

        glClearColor(c.r, c.g, c.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
}

void framebufferCallback(GLFWwindow*, int w, int h)
{
    width = std::max(w, 1);
    height = std::max(h, 1);
    glViewport(0, 0, width, height);
}

int main()
{
    if (!glfwInit())
    {
        std::cerr << "GLFW initialization failed\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "MiniCraft 3D", nullptr, nullptr);

    if (!window)
    {
        std::cerr << "Window creation failed\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferCallback);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSwapInterval(1);

    glewExperimental = GL_TRUE;

    if (glewInit() != GLEW_OK)
    {
        std::cerr << "GLEW initialization failed\n";
        glfwTerminate();
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    createShader();

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    generateWorld();

    float lastFrame = (float)glfwGetTime();

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = (float)glfwGetTime();
        float dt = std::min(currentFrame - lastFrame, 0.05f);
        lastFrame = currentFrame;

        processInput(window, dt);
        processBlocks(window);

        glClearColor(0.48f, 0.72f, 0.94f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(
            glm::radians(75.0f),
            (float)width / (float)height,
            0.1f,
            100.0f
        );

        glm::vec3 front = cameraFront();
        glm::mat4 view = glm::lookAt(player, player + front, glm::vec3(0, 1, 0));

        renderWorld(view, projection);
        drawUI();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
