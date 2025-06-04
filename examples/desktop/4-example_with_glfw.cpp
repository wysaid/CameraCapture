/**
 * @file example_with_glfw.cpp
 * @author wysaid (this@wysaid.org)
 * @brief GLFW Example with ccap.
 * @date 2025-05
 *
 */

#include <ccap.h>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

constexpr const char* vertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec2 pos;
out vec2 texCoord;
void main() {
    gl_Position = vec4(pos, 0.0, 1.0);
    texCoord = (pos / 2.0) + 0.5;
}
)";

constexpr const char* fragmentShaderSource = R"(
#version 330 core
in vec2 texCoord;
out vec4 fragColor;
uniform sampler2D tex;
uniform float progress;

const float angle = 10.0;

void main() {
    /// Apply a wavy distortion to the texture coordinates based on the progress
    vec2 newCoord;
    newCoord.x = texCoord.x + 0.01 * sin(progress + texCoord.x * angle);
    newCoord.y = texCoord.y + 0.01 * sin(progress + texCoord.y * angle);
    
    /// Avoid sampling too close to the edges
    float edge1 = min(texCoord.x, texCoord.y);
    float edge2 = max(texCoord.x, texCoord.y);
    if (edge1 < 0.05 || edge2 > 0.95)
    {
        float lengthToEdge = min(edge1, 1.0 - edge2) / 0.05;
        newCoord = mix(texCoord, newCoord, vec2(lengthToEdge));
    }
    
    fragColor = texture(tex, newCoord);
}
)";

int selectCamera(ccap::Provider& provider)
{
    if (auto names = provider.findDeviceNames(); names.size() > 1) {
        std::cout << "Multiple devices found, please select one:" << std::endl;
        for (size_t i = 0; i < names.size(); ++i) {
            std::cout << "  " << i << ": " << names[i] << std::endl;
        }
        int selectedIndex;
        std::cout << "Enter the index of the device you want to use: ";
        std::cin >> selectedIndex;
        if (selectedIndex < 0 || selectedIndex >= static_cast<int>(names.size())) {
            selectedIndex = 0;
            std::cerr << "Invalid index, using the first device:" << names[0] << std::endl;
        }
        else {
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

    if (auto deviceNames = cameraProvider.findDeviceNames(); !deviceNames.empty()) {
        for (const auto& name : deviceNames) {
            std::cout << "## Found video capture device: " << name << std::endl;
        }
    }

    int requestedWidth = 1920;
    int requestedHeight = 1080;
    double requestedFps = 60;

    constexpr ccap::PixelFormat cameraOutputPixelFormat = ccap::PixelFormat::RGBA32;
    constexpr GLenum pixelFormatGl = GL_RGBA;

    cameraProvider.set(ccap::PropertyName::Width, requestedWidth);
    cameraProvider.set(ccap::PropertyName::Height, requestedHeight);
    cameraProvider.set(ccap::PropertyName::PixelFormatOutput, cameraOutputPixelFormat);
    // cameraProvider.set(ccap::PropertyName::PixelFormatInternal, ccap::PixelFormat::NV12);
    cameraProvider.set(ccap::PropertyName::FrameRate, requestedFps);
    cameraProvider.set(ccap::PropertyName::FrameOrientation, ccap::FrameOrientation::BottomToTop);

    int deviceIndex;
    if (argc > 1 && std::isdigit(argv[1][0])) {
        deviceIndex = std::stoi(argv[1]);
    }
    else {
        deviceIndex = selectCamera(cameraProvider);
    }
    cameraProvider.open(deviceIndex, true);

    if (!cameraProvider.isStarted()) {
        std::cerr << "Failed to start camera!" << std::endl;
        return -1;
    }

    int frameWidth{}, frameHeight{};

    /// 5s timeout for grab
    if (auto frame = cameraProvider.grab(5000)) {
        frameWidth = frame->width;
        frameHeight = frame->height;
        std::cout << "## VideoFrame resolution: " << frameWidth << "x" << frameHeight << std::endl;
    }
    else {
        std::cerr << "Failed to grab a frame!" << std::endl;
        return -1;
    }

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(frameWidth, frameHeight, "ccap gui example", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    gladLoadGL(glfwGetProcAddress);

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexShaderSource, nullptr);
    glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fs);
    GLuint prog = glCreateProgram();
    glBindAttribLocation(prog, 0, "pos");
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint progressUniformLocation = glGetUniformLocation(prog, "progress");
    GLint texUniformLocation = glGetUniformLocation(prog, "tex");

    glUseProgram(prog);
    glUniform1i(texUniformLocation, 0);

    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    const float vertData[8] = { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };

    glBufferData(GL_ARRAY_BUFFER, sizeof(vertData), vertData, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);

    GLuint texture;
    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    for (; !glfwWindowShouldClose(window);) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);

        if (auto frame = cameraProvider.grab(
                30)) { // buffer orphaning: <https://www.khronos.org/opengl/wiki/Buffer_Object_Streaming>, pass nullptr first.
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frameWidth, frameHeight, 0, pixelFormatGl, GL_UNSIGNED_BYTE, nullptr);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frameWidth, frameHeight, pixelFormatGl, GL_UNSIGNED_BYTE, frame->data[0]);
        }
        int windowWidth, windowHeight;
        glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
        glViewport(0, 0, windowWidth, windowHeight);

        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(prog);

        float progress = fmod(glfwGetTime(), M_PI * 2.0) * 3.0;
        glUniform1f(progressUniformLocation, progress);

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup, this can be omitted if the program is exiting

    glfwDestroyWindow(window);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    glDeleteTextures(1, &texture);

    cameraProvider.close();
    glfwTerminate();
    return 0;
}
