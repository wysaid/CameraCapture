/**
 * @file example_with_glfw.cpp
 * @author wysaid (this@wysaid.org)
 * @brief GLFW Example with ccap.
 * @date 2025-05
 *
 */

#include <ccap.h>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <string>

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

constexpr const char* vertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 texCoord;
out vec2 TexCoord;
void main() {
    gl_Position = vec4(pos, 0.0, 1.0);
    TexCoord = texCoord;
}
)";

constexpr const char* fragmentShaderSource = R"(
#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D tex;
void main() {
    FragColor = texture(tex, TexCoord);
}
)";

constexpr float vertices[] = {
    // positions   // texCoords
    -1.0f, 1.0f, 0.0f, 1.0f,  // 左上
    -1.0f, -1.0f, 0.0f, 0.0f, // 左下
    1.0f, -1.0f, 1.0f, 0.0f,  // 右下
    1.0f, 1.0f, 1.0f, 1.0f    // 右上
};
constexpr unsigned int indices[] = { 0, 1, 2, 0, 2, 3 };

int selectCamera(ccap::Provider& provider)
{
    if (auto names = provider.findDeviceNames(); names.size() > 1)
    {
        std::cout << "Multiple devices found, please select one:" << std::endl;
        for (size_t i = 0; i < names.size(); ++i)
        {
            std::cout << "  " << i << ": " << names[i] << std::endl;
        }
        int selectedIndex;
        std::cout << "Enter the index of the device you want to use: ";
        std::cin >> selectedIndex;
        if (selectedIndex < 0 || selectedIndex >= static_cast<int>(names.size()))
        {
            selectedIndex = 0;
            std::cerr << "Invalid index, using the first device:" << names[0] << std::endl;
        }
        else
        {
            std::cout << "Using device: " << names[selectedIndex] << std::endl;
        }
        return selectedIndex;
    }

    return -1; // One or no device, use default.
}

int main(int argc, char** argv)
{
    /// Enable verbose log to see debug information
    ccap::setLogLevel(ccap::LogLevel::Verbose);

    ccap::Provider cameraProvider;

    if (auto deviceNames = cameraProvider.findDeviceNames(); !deviceNames.empty())
    {
        for (const auto& name : deviceNames)
        {
            std::cout << "## Found video capture device: " << name << std::endl;
        }
    }

    int requestedWidth = 1920;
    int requestedHeight = 1080;
    double requestedFps = 60;
    ccap::PixelFormat cameraOutputPixelFormat = ccap::PixelFormat::RGBA32;
    GLenum pixelFormat = GL_RGBA;

    cameraProvider.set(ccap::PropertyName::Width, requestedWidth);
    cameraProvider.set(ccap::PropertyName::Height, requestedHeight);
    cameraProvider.set(ccap::PropertyName::PixelFormatOutput, cameraOutputPixelFormat);
    // cameraProvider.set(ccap::PropertyName::PixelFormatInternal, ccap::PixelFormat::NV12);
    cameraProvider.set(ccap::PropertyName::FrameRate, requestedFps);
    cameraProvider.set(ccap::PropertyName::FrameOrientation, ccap::FrameOrientation::BottomToTop);

    int deviceIndex;
    if (argc > 1 && std::isdigit(argv[1][0]))
    {
        deviceIndex = std::stoi(argv[1]);
    }
    else
    {
        deviceIndex = selectCamera(cameraProvider);
    }
    cameraProvider.open(deviceIndex, true);

    if (!cameraProvider.isStarted())
    {
        std::cerr << "Failed to start camera!" << std::endl;
        return -1;
    }

    int frameWidth{}, frameHeight{};

    /// 5s timeout for grab
    if (auto frame = cameraProvider.grab(5000))
    {
        frameWidth = frame->width;
        frameHeight = frame->height;
        std::cout << "## Frame resolution: " << frameWidth << "x" << frameHeight << std::endl;
    }
    else
    {
        std::cerr << "Failed to grab a frame!" << std::endl;
        return -1;
    }

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(frameWidth, frameHeight, "ccap gui example", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    gladLoadGL(glfwGetProcAddress);

    // 编译着色器
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexShaderSource, nullptr);
    glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fs);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    // 顶点数据
    GLuint VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    GLuint tex;
    glGenTextures(1, &tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, pixelFormat, frameWidth, frameHeight, 0, pixelFormat, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    for (; !glfwWindowShouldClose(window);)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);

        if (auto frame = cameraProvider.grab(30))
        { // buffer orphaning: <https://www.khronos.org/opengl/wiki/Buffer_Object_Streaming>, pass nullptr first.
            glTexImage2D(GL_TEXTURE_2D, 0, pixelFormat, frameWidth, frameHeight, 0, pixelFormat, GL_UNSIGNED_BYTE, nullptr);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frameWidth, frameHeight, pixelFormat, GL_UNSIGNED_BYTE, frame->data[0]);
        }
        int winWidth, winHeight;
        glfwGetFramebufferSize(window, &winWidth, &winHeight);
        glViewport(0, 0, winWidth, winHeight);

        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(prog);
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup, this can be omitted if the program is exiting

    glfwDestroyWindow(window);
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(prog);
    glDeleteTextures(1, &tex);

    cameraProvider.close();
    glfwTerminate();
    return 0;
}
