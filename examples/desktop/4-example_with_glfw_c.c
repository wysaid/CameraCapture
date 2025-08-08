/**
 * @file 4-example_with_glfw_c.c
 * @brief GLFW Example with ccap C interface
 * @author wysaid (this@wysaid.org)
 * @date 2025-05
 */

#include "ccap_c.h"
#include "ccap_utils_c.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

static const char* vertexShaderSource = 
"#version 330 core\n"
"layout(location = 0) in vec2 pos;\n"
"out vec2 texCoord;\n"
"void main() {\n"
"    gl_Position = vec4(pos, 0.0, 1.0);\n"
"    texCoord = (pos / 2.0) + 0.5;\n"
"}\n";

static const char* fragmentShaderSource = 
"#version 330 core\n"
"in vec2 texCoord;\n"
"out vec4 fragColor;\n"
"uniform sampler2D tex;\n"
"uniform float progress;\n"
"\n"
"const float angle = 10.0;\n"
"\n"
"void main() {\n"
"    /// Apply a wavy distortion to the texture coordinates based on the progress\n"
"    vec2 newCoord;\n"
"    newCoord.x = texCoord.x + 0.01 * sin(progress + texCoord.x * angle);\n"
"    newCoord.y = texCoord.y + 0.01 * sin(progress + texCoord.y * angle);\n"
"    \n"
"    /// Avoid sampling too close to the edges\n"
"    float edge1 = min(texCoord.x, texCoord.y);\n"
"    float edge2 = max(texCoord.x, texCoord.y);\n"
"    if (edge1 < 0.05 || edge2 > 0.95)\n"
"    {\n"
"        float lengthToEdge = min(edge1, 1.0 - edge2) / 0.05;\n"
"        newCoord = mix(texCoord, newCoord, vec2(lengthToEdge));\n"
"    }\n"
"    \n"
"    fragColor = texture(tex, newCoord);\n"
"}\n";

int selectCamera(CcapProvider* provider) {
    char** deviceNames;
    size_t deviceCount;
    
    if (ccap_provider_find_device_names(provider, &deviceNames, &deviceCount) && deviceCount > 1) {
        printf("Multiple devices found, please select one:\n");
        for (size_t i = 0; i < deviceCount; i++) {
            printf("  %zu: %s\n", i, deviceNames[i]);
        }
        
        int selectedIndex;
        printf("Enter the index of the device you want to use: ");
        if (scanf("%d", &selectedIndex) != 1) {
            selectedIndex = 0;
        }
        
        if (selectedIndex < 0 || selectedIndex >= (int)deviceCount) {
            selectedIndex = 0;
            fprintf(stderr, "Invalid index, using the first device: %s\n", deviceNames[0]);
        } else {
            printf("Using device: %s\n", deviceNames[selectedIndex]);
        }
        
        ccap_provider_free_device_names(deviceNames, deviceCount);
        return selectedIndex;
    }
    
    if (deviceCount > 0) {
        ccap_provider_free_device_names(deviceNames, deviceCount);
    }
    
    return -1; // One or no device, use default.
}

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        fprintf(stderr, "Shader compilation failed: %s\n", infoLog);
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

int main(int argc, char** argv) {
    printf("ccap C Interface GLFW Example\n");
    printf("Version: %s\n\n", ccap_get_version());

    // Enable verbose log to see debug information
    ccap_set_log_level(CCAP_LOG_LEVEL_VERBOSE);

    // Create provider
    CcapProvider* provider = ccap_provider_create();
    if (!provider) {
        printf("Failed to create provider\n");
        return -1;
    }

    // Find and print available devices
    char** deviceNames;
    size_t deviceCount;
    if (ccap_provider_find_device_names(provider, &deviceNames, &deviceCount)) {
        for (size_t i = 0; i < deviceCount; i++) {
            printf("## Found video capture device: %s\n", deviceNames[i]);
        }
        ccap_provider_free_device_names(deviceNames, deviceCount);
    }

    // Set camera properties
    int requestedWidth = 1920;
    int requestedHeight = 1080;
    double requestedFps = 60.0;
    
    CcapPixelFormat cameraOutputPixelFormat = CCAP_PIXEL_FORMAT_RGBA32;
    GLenum pixelFormatGl = GL_RGBA;
    
    ccap_provider_set_property(provider, CCAP_PROPERTY_WIDTH, requestedWidth);
    ccap_provider_set_property(provider, CCAP_PROPERTY_HEIGHT, requestedHeight);
    ccap_provider_set_property(provider, CCAP_PROPERTY_PIXEL_FORMAT_OUTPUT, cameraOutputPixelFormat);
    ccap_provider_set_property(provider, CCAP_PROPERTY_FRAME_RATE, requestedFps);
    ccap_provider_set_property(provider, CCAP_PROPERTY_FRAME_ORIENTATION, CCAP_FRAME_ORIENTATION_BOTTOM_TO_TOP);

    // Select and open camera
    int deviceIndex;
    if (argc > 1 && isdigit(argv[1][0])) {
        deviceIndex = atoi(argv[1]);
    } else {
        deviceIndex = selectCamera(provider);
    }
    
    if (!ccap_provider_open_by_index(provider, deviceIndex, true)) {
        printf("Failed to open camera\n");
        ccap_provider_destroy(provider);
        return -1;
    }

    if (!ccap_provider_is_started(provider)) {
        fprintf(stderr, "Failed to start camera!\n");
        ccap_provider_destroy(provider);
        return -1;
    }

    // Get frame dimensions
    int frameWidth = 0, frameHeight = 0;
    
    // 5s timeout for grab
    CcapVideoFrame* firstFrame = ccap_provider_grab(provider, 5000);
    if (firstFrame) {
        CcapVideoFrameInfo frameInfo;
        if (ccap_video_frame_get_info(firstFrame, &frameInfo)) {
            frameWidth = frameInfo.width;
            frameHeight = frameInfo.height;
            printf("## VideoFrame resolution: %dx%d\n", frameWidth, frameHeight);
        }
        ccap_video_frame_release(firstFrame);
    } else {
        fprintf(stderr, "Failed to grab a frame!\n");
        ccap_provider_destroy(provider);
        return -1;
    }

    // Initialize GLFW
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        ccap_provider_destroy(provider);
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(frameWidth, frameHeight, "ccap gui example (C version)", NULL, NULL);
    if (!window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        ccap_provider_destroy(provider);
        return -1;
    }

    glfwMakeContextCurrent(window);
    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) {
        fprintf(stderr, "Failed to initialize GLAD\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        ccap_provider_destroy(provider);
        return -1;
    }

    // Compile shaders
    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    if (!vs || !fs) {
        glfwDestroyWindow(window);
        glfwTerminate();
        ccap_provider_destroy(provider);
        return -1;
    }

    GLuint prog = glCreateProgram();
    glBindAttribLocation(prog, 0, "pos");
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint linkStatus;
    glGetProgramiv(prog, GL_LINK_STATUS, &linkStatus);
    if (!linkStatus) {
        char infoLog[512];
        glGetProgramInfoLog(prog, 512, NULL, infoLog);
        fprintf(stderr, "Program linking failed: %s\n", infoLog);
        glDeleteProgram(prog);
        glfwDestroyWindow(window);
        glfwTerminate();
        ccap_provider_destroy(provider);
        return -1;
    }

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
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(0);

    GLuint texture;
    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    printf("Starting render loop. Press ESC or close window to exit.\n");

    // Main render loop
    while (!glfwWindowShouldClose(window)) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);

        // Grab new frame
        CcapVideoFrame* frame = ccap_provider_grab(provider, 30); // 30ms timeout
        if (frame) {
            CcapVideoFrameInfo frameInfo;
            if (ccap_video_frame_get_info(frame, &frameInfo)) {
                // Buffer orphaning: pass NULL first, then update with actual data
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frameWidth, frameHeight, 0, pixelFormatGl, GL_UNSIGNED_BYTE, NULL);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frameWidth, frameHeight, pixelFormatGl, GL_UNSIGNED_BYTE, frameInfo.data[0]);
            }
            ccap_video_frame_release(frame);
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

    // Cleanup
    glfwDestroyWindow(window);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    glDeleteTextures(1, &texture);

    ccap_provider_destroy(provider);
    glfwTerminate();
    
    printf("GLFW example completed successfully\n");
    return 0;
}
