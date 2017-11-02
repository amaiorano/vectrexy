#include "GLRender.h"
#include "EngineClient.h"

#include <gl/glew.h> // Must be included before gl.h

#include <SDL_opengl.h> // Wraps OpenGL headers
#include <SDL_opengl_glext.h>
#include <gl/GLU.h>

#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>

namespace {
    int g_screenWidth{}, g_screenHeight{};
    int g_windowWidth{}, g_windowHeight{};
    std::vector<glm::vec2> g_lineVA, g_pointVA;

    namespace ShaderProgram {
        GLuint renderToTexture{};
        GLuint darkenTexture{};
        GLuint textureToScreen{};
    } // namespace ShaderProgram

    GLuint g_textureFB{};
    GLuint g_renderedTexture0{};
    GLuint g_renderedTexture1{};
    int g_renderedTexture0Index{};

    glm::mat4x4 g_projectionMatrix{};
    glm::mat4x4 g_modelViewMatrix{};

    GLuint GenFrameBuffer() {
        GLuint fb;
        glGenFramebuffers(1, &fb);
        return fb;
    }

    GLuint GenTexture(GLsizei width, GLsizei height) {
        GLuint textureId;
        glGenTextures(1, &textureId);
        // "Bind" the newly created texture : all future texture functions will modify this texture
        glBindTexture(GL_TEXTURE_2D, textureId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, width, height, 0, GL_RGB, GL_FLOAT, 0);
        // By default, filtering is GL_LINEAR or GL_NEAREST_MIPMAP_LINEAR. We set to GL_NEAREST to
        // avoid creating mipmaps and to make it less blurry.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        return textureId;
    }

    GLuint GenVBO(const std::vector<glm::vec2>& vertices) {
        GLuint vbo;
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec2), vertices.data(),
                     GL_DYNAMIC_DRAW);
        return vbo;
    }

    void DeleteVBO(GLuint& vbo) {
        glDeleteBuffers(1, &vbo);
        vbo = {};
    }

    void CheckFramebufferStatus() {
        ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE);
    }

    void BindFramebufferAndTexture(GLuint fb, GLuint texture) {
        glBindFramebuffer(GL_FRAMEBUFFER, fb);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture, 0);
    }

    std::optional<std::string> FileToString(const char* file) {
        std::ifstream is(file);
        if (!is)
            return {};
        std::stringstream buffer;
        buffer << is.rdbuf();
        return buffer.str();
    }

    GLuint LoadShaders(const char* vertShaderFile, const char* fragShaderFile) {
        // Create the shaders
        GLuint vertShaderId = glCreateShader(GL_VERTEX_SHADER);
        GLuint fragShaderId = glCreateShader(GL_FRAGMENT_SHADER);

        // Read the Vertex Shader code from the file
        auto vertShaderCode = FileToString(vertShaderFile);
        if (!vertShaderCode) {
            FAIL_MSG("Failed to open %s", vertShaderFile);
            return 0;
        }

        auto fragShaderCode = FileToString(fragShaderFile);
        if (!fragShaderCode) {
            FAIL_MSG("Failed to open %s", vertShaderFile);
            return 0;
        }

        GLint result = GL_FALSE;
        int infoLogLength;

        // Compile Vertex Shader
        printf("Compiling shader : %s\n", vertShaderFile);
        char const* vertSourcePointer = (*vertShaderCode).c_str();
        glShaderSource(vertShaderId, 1, &vertSourcePointer, NULL);
        glCompileShader(vertShaderId);

        // Check Vertex Shader
        glGetShaderiv(vertShaderId, GL_COMPILE_STATUS, &result);
        glGetShaderiv(vertShaderId, GL_INFO_LOG_LENGTH, &infoLogLength);
        if (infoLogLength > 0) {
            std::vector<char> vertShaderErrorMessage(infoLogLength + 1);
            glGetShaderInfoLog(vertShaderId, infoLogLength, NULL, &vertShaderErrorMessage[0]);
            printf("%s\n", &vertShaderErrorMessage[0]);
        }

        // Compile Fragment Shader
        printf("Compiling shader : %s\n", fragShaderFile);
        char const* fragSourcePointer = (*fragShaderCode).c_str();
        glShaderSource(fragShaderId, 1, &fragSourcePointer, NULL);
        glCompileShader(fragShaderId);

        // Check Fragment Shader
        glGetShaderiv(fragShaderId, GL_COMPILE_STATUS, &result);
        glGetShaderiv(fragShaderId, GL_INFO_LOG_LENGTH, &infoLogLength);
        if (infoLogLength > 0) {
            std::vector<char> FragmentShaderErrorMessage(infoLogLength + 1);
            glGetShaderInfoLog(fragShaderId, infoLogLength, NULL, &FragmentShaderErrorMessage[0]);
            printf("%s\n", &FragmentShaderErrorMessage[0]);
        }

        // Link the program
        printf("Linking program\n");
        GLuint programId = glCreateProgram();
        glAttachShader(programId, vertShaderId);
        glAttachShader(programId, fragShaderId);
        glLinkProgram(programId);

        // Check the program
        glGetProgramiv(programId, GL_LINK_STATUS, &result);
        glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &infoLogLength);
        if (infoLogLength > 0) {
            std::vector<char> ProgramErrorMessage(infoLogLength + 1);
            glGetProgramInfoLog(programId, infoLogLength, NULL, &ProgramErrorMessage[0]);
            printf("%s\n", &ProgramErrorMessage[0]);
        }

        glDetachShader(programId, vertShaderId);
        glDetachShader(programId, fragShaderId);

        glDeleteShader(vertShaderId);
        glDeleteShader(fragShaderId);

        return programId;
    }

} // namespace

namespace GLRender {
    void Initialize(int screenWidth, int screenHeight) {
        g_screenWidth = screenWidth;
        g_screenHeight = screenHeight;

        // glShadeModel(GL_SMOOTH);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        // glClearDepth(1.0f);
        // glEnable(GL_DEPTH_TEST);
        // glDepthFunc(GL_LEQUAL);
        // glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

        // Initialize GLEW
        glewExperimental = true; // Needed for core profile
        if (glewInit() != GLEW_OK) {
            FAIL_MSG("Failed to initialize GLEW");
        }

        GLuint VertexArrayID;
        glGenVertexArrays(1, &VertexArrayID);
        glBindVertexArray(VertexArrayID);

        ShaderProgram::renderToTexture =
            LoadShaders("shaders/DrawVectors.vert", "shaders/DrawVectors.frag");

        ShaderProgram::darkenTexture =
            LoadShaders("shaders/PassThrough.vert", "shaders/DarkenTexture.frag");

        ShaderProgram::textureToScreen =
            LoadShaders("shaders/Passthrough.vert", "shaders/DrawTexture.frag");

        g_textureFB = GenFrameBuffer();
        // glBindFramebuffer(GL_FRAMEBUFFER, g_textureFB); // Needed?
        g_renderedTexture0 = GenTexture(g_screenWidth, g_screenHeight);
        g_renderedTexture1 = GenTexture(g_screenWidth, g_screenHeight);

        // Set output of fragment shader to color attachment 0
        glBindFramebuffer(GL_FRAMEBUFFER, g_textureFB);
        GLenum drawBuffers[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, drawBuffers);
        CheckFramebufferStatus();

        // Clear g_renderedTexture0 once
        BindFramebufferAndTexture(g_textureFB, g_renderedTexture0);
        // glBindFramebuffer(GL_FRAMEBUFFER, g_textureFB);
        // glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, g_renderedTexture0, 0);
        glViewport(0, 0, g_screenWidth, g_screenHeight);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    void Shutdown() {
        //
    }

    std::tuple<int, int> GetMajorMinorVersion() { return {3, 3}; }

    bool SetViewport(int windowWidth, int windowHeight) {
        GLfloat ratio;

        if (windowHeight == 0) {
            windowHeight = 1;
        }

        g_windowWidth = windowWidth;
        g_windowHeight = windowHeight;

        ratio = GLfloat(windowWidth) / GLfloat(windowHeight);
        glViewport(0, 0, GLsizei(windowWidth), GLsizei(windowHeight));

        double halfWidth = 128;  // g_screenWidth / 2.0;
        double halfHeight = 128; // g_screenHeight / 2.0;
        g_projectionMatrix = glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight);

        // glMatrixMode(GL_PROJECTION);
        // glLoadIdentity();
        // double halfWidth = screenWidth / 2.0;
        // double halfHeight = screenHeight / 2.0;
        // gluOrtho2D(-halfWidth, halfWidth, -halfHeight, halfHeight);

        // glMatrixMode(GL_MODELVIEW);
        // glLoadIdentity();

        g_modelViewMatrix = {};

        return true;
    }

    //@TODO: remove this function
    void PreRender() {
        // glMatrixMode(GL_MODELVIEW);
        // glLoadIdentity();
    }

    void RenderScene(double frameTime) {
        auto currRenderedTexture0 =
            g_renderedTexture0Index == 0 ? g_renderedTexture0 : g_renderedTexture1;
        auto currRenderedTexture1 =
            g_renderedTexture0Index == 0 ? g_renderedTexture1 : g_renderedTexture0;
        g_renderedTexture0Index = (g_renderedTexture0Index + 1) % 2;

        /////////////////////////////////////////////////////////////////
        // PASS 1: render to texture
        /////////////////////////////////////////////////////////////////
        {
            // Render to our framebuffer
            glBindFramebuffer(GL_FRAMEBUFFER, g_textureFB);
            glViewport(0, 0, g_screenWidth, g_screenHeight);
            glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, currRenderedTexture0, 0);

            // Clear the screen
            // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Use our shader
            glUseProgram(ShaderProgram::renderToTexture);

            GLuint mvpUniform = glGetUniformLocation(ShaderProgram::renderToTexture, "MVP");

            auto mvp = g_projectionMatrix * g_modelViewMatrix;
            glUniformMatrix4fv(mvpUniform, 1, GL_FALSE, &mvp[0][0]);

            // 1st attribute buffer : vertices

            auto DrawVertices = [](auto& VA, GLenum mode) {

                GLuint vbo = GenVBO(VA);

                glEnableVertexAttribArray(0);
                glBindBuffer(GL_ARRAY_BUFFER, vbo);
                glVertexAttribPointer(0,        // attribute
                                      2,        // size
                                      GL_FLOAT, // type
                                      GL_FALSE, // normalized?
                                      0,        // stride
                                      (void*)0  // array buffer offset
                );

                glDrawArrays(mode, 0, VA.size());

                VA.clear();

                glDisableVertexAttribArray(0);
                DeleteVBO(vbo);
            };

            DrawVertices(g_lineVA, GL_LINES);
            DrawVertices(g_pointVA, GL_POINTS);

            // glDeleteVertexArrays(1, &VertexArrayID);

            //@TODO: same as above for g_pointVA
        }

        /////////////////////////////////////////////////////////////////
        // PASS 2: darken texture
        /////////////////////////////////////////////////////////////////

        {
            glBindFramebuffer(GL_FRAMEBUFFER, g_textureFB);
            glViewport(0, 0, g_screenWidth, g_screenHeight);
            glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, currRenderedTexture1, 0);

            glClear(GL_COLOR_BUFFER_BIT);

            glUseProgram(ShaderProgram::darkenTexture);

            // Bind our texture in Texture Unit 0
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, currRenderedTexture0);
            GLuint texID = glGetUniformLocation(ShaderProgram::darkenTexture, "renderedTexture");
            GLuint frameTimeID = glGetUniformLocation(ShaderProgram::darkenTexture, "frameTime");
            GLuint darkenSpeedScaleID =
                glGetUniformLocation(ShaderProgram::darkenTexture, "darkenSpeedScale");
            // Set our "renderedTexture0" sampler to use Texture Unit 0
            glUniform1i(texID, 0);

            static volatile float darkenSpeedScale = 3.0;
            glUniform1f(darkenSpeedScaleID, darkenSpeedScale);
            glUniform1f(frameTimeID, (float)(frameTime));

            //@TODO: create once
            GLuint vbo = [] {
                GLuint quadVbo;
                // The fullscreen quad's FBO
                static const GLfloat vertices[] = {
                    -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, -1.0f, 1.0f, 0.0f,
                    -1.0f, 1.0f,  0.0f, 1.0f, -1.0f, 0.0f, 1.0f,  1.0f, 0.0f,
                };
                glGenBuffers(1, &quadVbo);
                glBindBuffer(GL_ARRAY_BUFFER, quadVbo);
                glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
                return quadVbo;
            }();

            // 1rst attribute buffer : vertices
            glEnableVertexAttribArray(0);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glVertexAttribPointer(
                0, // attribute 0. No particular reason for 0, but must match layout in the shader.
                3, // size
                GL_FLOAT, // type
                GL_FALSE, // normalized?
                0,        // stride
                (void*)0  // array buffer offset
            );

            // Draw the triangles !
            glDrawArrays(GL_TRIANGLES, 0, 6); // 2*3 indices starting at 0 -> 2 triangles

            glDisableVertexAttribArray(0);
            DeleteVBO(vbo);
        }

        /////////////////////////////////////////////////////////////////
        // PASS 3: render to screen
        /////////////////////////////////////////////////////////////////
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, g_windowWidth, g_windowHeight);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Use our shader
            glUseProgram(ShaderProgram::textureToScreen);

            // Bind our texture in Texture Unit 0
            glActiveTexture(GL_TEXTURE0);
            ////////////////////////glBindTexture(GL_TEXTURE_2D, currRenderedTexture1);
            glBindTexture(GL_TEXTURE_2D, currRenderedTexture0);

            GLuint texID = glGetUniformLocation(ShaderProgram::textureToScreen, "renderedTexture");
            // Set our "renderedTexture1" sampler to use Texture Unit 0
            glUniform1i(texID, 0);

            //@TODO: create once
            GLuint vbo = [] {
                GLuint quadVbo;
                // The fullscreen quad's FBO
                static const GLfloat vertices[] = {
                    -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, -1.0f, 1.0f, 0.0f,
                    -1.0f, 1.0f,  0.0f, 1.0f, -1.0f, 0.0f, 1.0f,  1.0f, 0.0f,
                };
                glGenBuffers(1, &quadVbo);
                glBindBuffer(GL_ARRAY_BUFFER, quadVbo);
                glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
                return quadVbo;
            }();

            // 1st attribute buffer : vertices
            glEnableVertexAttribArray(0);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glVertexAttribPointer(0, // attribute 0. No particular reason for 0, but must match the
                                     // layout in the shader.
                                  3, // size
                                  GL_FLOAT, // type
                                  GL_FALSE, // normalized?
                                  0,        // stride
                                  (void*)0  // array buffer offset
            );

            // Draw the triangles !
            glDrawArrays(GL_TRIANGLES, 0, 6); // 2*3 indices starting at 0 -> 2 triangles

            glDisableVertexAttribArray(0);
            DeleteVBO(vbo);
        }
    }
} // namespace GLRender

void Display::Clear() {
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Display::DrawLines(const std::vector<Line>& lines) {
    auto AlmostEqual = [](float a, float b, float epsilon = 0.01f) {
        return abs(a - b) <= epsilon;
    };

    for (auto& line : lines) {
        glm::vec2 p0{line.p0.x, line.p0.y};
        glm::vec2 p1{line.p1.x, line.p1.y};

        if (AlmostEqual(p0.x, p1.x) && AlmostEqual(p0.y, p1.y)) {
            g_pointVA.push_back(p0);
        } else {
            g_lineVA.push_back(p0);
            g_lineVA.push_back(p1);
        }
    }
}

void Display::DrawLine(float x1, float y1, float x2, float y2) {
    auto AlmostEqual = [](float a, float b, float epsilon = 0.01f) {
        return abs(a - b) <= epsilon;
    };

    if (AlmostEqual(x1, x2) && AlmostEqual(y1, y2)) {
        glBegin(GL_POINTS);
        glVertex2f(x1, y1);
        glEnd();
    } else {
        glBegin(GL_LINES);
        glVertex2f(x1, y1);
        glVertex2f(x2, y2);
        glEnd();
    }
}
