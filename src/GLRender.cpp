#include "GLRender.h"
#include "EngineClient.h"
#include "ImageFileUtils.h"

#include <gl/glew.h> // Must be included before gl.h

#include <SDL_opengl.h> // Wraps OpenGL headers
#include <SDL_opengl_glext.h>
#include <gl/GLU.h>

#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>

// Vectrex screen dimensions
const int VECTREX_SCREEN_WIDTH = 256;
const int VECTREX_SCREEN_HEIGHT = 256;

namespace {
    // Wraps an OpenGL resource id and automatically calls the delete function on destruction
    template <typename DeleteFunc>
    class GLResource {
    public:
        static const GLuint INVALID_RESOURCE_ID = ~0u;

        GLResource(GLuint id = INVALID_RESOURCE_ID, DeleteFunc deleteFunc = DeleteFunc{})
            : m_id(id)
            , m_deleteFunc(std::move(deleteFunc)) {}

        ~GLResource() {
            if (m_id != INVALID_RESOURCE_ID) {
                m_deleteFunc(m_id);
            }
        }

        // Disable copy
        GLResource(const GLResource&) = delete;
        GLResource& operator=(const GLResource&) = delete;

        // Enable move
        GLResource(GLResource&& rhs)
            : m_id(rhs.m_id)
            , m_deleteFunc(rhs.m_deleteFunc) {
            rhs.m_id = INVALID_RESOURCE_ID;
        }
        GLResource& operator=(GLResource&& rhs) {
            if (this != &rhs) {
                std::swap(m_id, rhs.m_id);
                std::swap(m_deleteFunc, m_deleteFunc);
            }
            return *this;
        }

        GLuint get() const {
            assert(m_id != INVALID_RESOURCE_ID);
            return m_id;
        }
        GLuint operator*() const { return get(); }

    private:
        GLuint m_id;
        DeleteFunc m_deleteFunc;
    };

    auto MakeTextureResource() {
        struct Deleter {
            auto operator()(GLuint id) { glDeleteTextures(1, &id); }
        };
        GLuint id;
        glGenTextures(1, &id);
        return GLResource<decltype(Deleter{})>{id};
    }
    using TextureResource = decltype(MakeTextureResource());

    auto MakeVertexArrayResource() {
        struct Deleter {
            auto operator()(GLuint id) { glDeleteVertexArrays(1, &id); }
        };
        GLuint id;
        glGenVertexArrays(1, &id);
        return GLResource<decltype(Deleter{})>{id};
    }
    using VertexArrayResource = decltype(MakeVertexArrayResource());

    auto MakeFrameBufferResource() {
        struct Deleter {
            auto operator()(GLuint id) { glDeleteFramebuffers(1, &id); }
        };
        GLuint id;
        glGenFramebuffers(1, &id);
        return GLResource<decltype(Deleter{})>{id};
    }
    using FrameBufferResource = decltype(MakeFrameBufferResource());

    auto MakeBufferResource() {
        struct Deleter {
            auto operator()(GLuint id) { glDeleteBuffers(1, &id); }
        };
        GLuint id;
        glGenBuffers(1, &id);
        return GLResource<decltype(Deleter{})>{id};
    }
    using BufferResource = decltype(MakeBufferResource());

    std::optional<std::string> FileToString(const char* file) {
        std::ifstream is(file);
        if (!is)
            return {};
        std::stringstream buffer;
        buffer << is.rdbuf();
        return buffer.str();
    }

    void AllocateTexture(GLuint textureId, GLsizei width, GLsizei height) {
        glBindTexture(GL_TEXTURE_2D, textureId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, width, height, 0, GL_RGB, GL_FLOAT, 0);
        // By default, filtering is GL_LINEAR or GL_NEAREST_MIPMAP_LINEAR. We set to GL_NEAREST to
        // avoid creating mipmaps and to make it less blurry.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    void SetVertexBufferData(GLuint vboId, const std::vector<glm::vec2>& vertices) {
        glBindBuffer(GL_ARRAY_BUFFER, vboId);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec2), vertices.data(),
                     GL_DYNAMIC_DRAW);
    }

    template <typename T, size_t N>
    void SetVertexBufferData(GLuint vboId, T (&vertices)[N]) {
        glBindBuffer(GL_ARRAY_BUFFER, vboId);
        glBufferData(GL_ARRAY_BUFFER, N * sizeof(vertices[0]), &vertices[0], GL_DYNAMIC_DRAW);
    }

    void CheckFramebufferStatus() {
        ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE);
    }

    GLuint LoadShaders(const char* vertShaderFile, const char* fragShaderFile) {
        auto CheckStatus = [](GLuint id, GLenum pname) {
            assert(pname == GL_COMPILE_STATUS || pname == GL_LINK_STATUS);
            GLint result = GL_FALSE;
            int infoLogLength;
            glGetShaderiv(id, pname, &result);
            glGetShaderiv(id, GL_INFO_LOG_LENGTH, &infoLogLength);
            if (infoLogLength > 0) {
                std::vector<char> info(infoLogLength + 1);
                if (pname == GL_COMPILE_STATUS)
                    glGetShaderInfoLog(id, infoLogLength, NULL, info.data());
                else
                    glGetProgramInfoLog(id, infoLogLength, NULL, info.data());
                printf("%s", info.data());
            }
            return true;
        };

        auto CompileShader = [CheckStatus](GLuint shaderId, const char* shaderFile) {
            auto shaderCode = FileToString(shaderFile);
            if (!shaderCode) {
                printf("Failed to open %s", shaderFile);
                return false;
            }
            printf("Compiling shader : %s\n", shaderFile);
            auto sourcePtr = shaderCode->c_str();
            glShaderSource(shaderId, 1, &sourcePtr, NULL);
            glCompileShader(shaderId);
            return CheckStatus(shaderId, GL_COMPILE_STATUS);
        };

        auto LinkProgram = [CheckStatus](GLuint programId) {
            printf("Linking program\n");
            glLinkProgram(programId);
            return CheckStatus(programId, GL_LINK_STATUS);
        };

        GLuint vertShaderId = glCreateShader(GL_VERTEX_SHADER);
        if (!CompileShader(vertShaderId, vertShaderFile))
            return 0;

        GLuint fragShaderId = glCreateShader(GL_FRAGMENT_SHADER);
        if (!CompileShader(fragShaderId, fragShaderFile))
            return 0;

        GLuint programId = glCreateProgram();
        glAttachShader(programId, vertShaderId);
        glAttachShader(programId, fragShaderId);
        if (!LinkProgram(programId))
            return 0;

        glDetachShader(programId, vertShaderId);
        glDetachShader(programId, fragShaderId);

        glDeleteShader(vertShaderId);
        glDeleteShader(fragShaderId);

        return programId;
    }

    bool LoadPngTexture(GLuint textureId, const char* file) {
        auto imgData = ImageFileUtils::loadPngImage(file);

        if (!imgData) {
            return false;
        }

        glBindTexture(GL_TEXTURE_2D, textureId);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, imgData->hasAlpha ? GL_RGBA : GL_RGB, imgData->width,
                     imgData->height, 0, imgData->hasAlpha ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE,
                     imgData->data.get());
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST /*GL_LINEAR*/);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST /*GL_LINEAR*/);

        return true;
    }

    // Globals
    int g_windowWidth{}, g_windowHeight{};
    std::vector<glm::vec2> g_lineVA, g_pointVA;

    namespace ShaderProgram {
        GLuint renderToTexture{};
        GLuint darkenTexture{};
        GLuint textureToScreen{};
    } // namespace ShaderProgram

    glm::mat4x4 g_projectionMatrix{};
    glm::mat4x4 g_modelViewMatrix{};
    VertexArrayResource g_topLevelVAO;
    FrameBufferResource g_textureFB;
    int g_renderedTexture0Index{};
    TextureResource g_renderedTexture0;
    TextureResource g_renderedTexture1;
    TextureResource g_overlayTexture;

} // namespace

namespace GLRender {
    void Initialize() {
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

        // We don't actually use VAOs, but we must create one and bind it so that we can use VBOs
        g_topLevelVAO = MakeVertexArrayResource();
        glBindVertexArray(*g_topLevelVAO);

        // Load shaders
        ShaderProgram::renderToTexture =
            LoadShaders("shaders/DrawVectors.vert", "shaders/DrawVectors.frag");

        ShaderProgram::darkenTexture =
            LoadShaders("shaders/PassThrough.vert", "shaders/DarkenTexture.frag");

        ShaderProgram::textureToScreen =
            LoadShaders("shaders/Passthrough.vert", "shaders/DrawTexture.frag");

        // Create resources
        g_textureFB = MakeFrameBufferResource();
        // Set output of fragment shader to color attachment 0
        glBindFramebuffer(GL_FRAMEBUFFER, *g_textureFB);
        GLenum drawBuffers[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, drawBuffers);
        CheckFramebufferStatus();

        // For now, always attempt to load the mine storm overlay
        g_overlayTexture = MakeTextureResource();
        glObjectLabel(GL_TEXTURE, *g_overlayTexture, -1, "g_overlayTexture");
        const char* overlayFile = "overlays/mine.png";
        if (!LoadPngTexture(*g_overlayTexture, overlayFile)) {
            std::cerr << "Failed to load overlay: " << overlayFile << "\n";
        }
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

        ratio = GLfloat(g_windowWidth) / GLfloat(g_windowHeight);
        glViewport(0, 0, GLsizei(g_windowWidth), GLsizei(g_windowHeight));

        double halfWidth = VECTREX_SCREEN_WIDTH / 2.0;
        double halfHeight = VECTREX_SCREEN_HEIGHT / 2.0;
        g_projectionMatrix = glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight);

        g_modelViewMatrix = {};

        // (Re)create resources that depend on viewport size
        g_renderedTexture0 = MakeTextureResource();
        AllocateTexture(*g_renderedTexture0, g_windowWidth, g_windowHeight);
        g_renderedTexture1 = MakeTextureResource();
        AllocateTexture(*g_renderedTexture1, g_windowWidth, g_windowHeight);

        // Clear g_renderedTexture0 once
        glBindFramebuffer(GL_FRAMEBUFFER, *g_textureFB);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, *g_renderedTexture0, 0);
        glViewport(0, 0, g_windowWidth, g_windowHeight);
        glClear(GL_COLOR_BUFFER_BIT);

        return true;
    }

    void RenderScene(double frameTime) {
        auto& currRenderedTexture0 =
            g_renderedTexture0Index == 0 ? g_renderedTexture0 : g_renderedTexture1;
        auto& currRenderedTexture1 =
            g_renderedTexture0Index == 0 ? g_renderedTexture1 : g_renderedTexture0;
        g_renderedTexture0Index = (g_renderedTexture0Index + 1) % 2;

        glObjectLabel(GL_TEXTURE, *currRenderedTexture0, -1, "currRenderedTexture0");
        glObjectLabel(GL_TEXTURE, *currRenderedTexture1, -1, "currRenderedTexture1");

        /////////////////////////////////////////////////////////////////
        // PASS 1: render to texture
        /////////////////////////////////////////////////////////////////
        {
            // Render to our framebuffer
            glBindFramebuffer(GL_FRAMEBUFFER, *g_textureFB);
            glViewport(0, 0, g_windowWidth, g_windowHeight);
            glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, *currRenderedTexture0, 0);

            // Purposely do not clear the target texture.
            // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Use our shader
            glUseProgram(ShaderProgram::renderToTexture);

            GLuint mvpUniform = glGetUniformLocation(ShaderProgram::renderToTexture, "MVP");

            auto mvp = g_projectionMatrix * g_modelViewMatrix;
            glUniformMatrix4fv(mvpUniform, 1, GL_FALSE, &mvp[0][0]);

            auto DrawVertices = [](auto& VA, GLenum mode) {
                auto vbo = MakeBufferResource();
                SetVertexBufferData(*vbo, VA);

                glEnableVertexAttribArray(0);
                glBindBuffer(GL_ARRAY_BUFFER, *vbo);
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
            };

            DrawVertices(g_lineVA, GL_LINES);
            DrawVertices(g_pointVA, GL_POINTS);
        }

        /////////////////////////////////////////////////////////////////
        // PASS 2: darken texture
        /////////////////////////////////////////////////////////////////
        {
            glBindFramebuffer(GL_FRAMEBUFFER, *g_textureFB);
            glViewport(0, 0, g_windowWidth, g_windowHeight);
            glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, *currRenderedTexture1, 0);

            glClear(GL_COLOR_BUFFER_BIT);

            glUseProgram(ShaderProgram::darkenTexture);

            // Bind our texture in Texture Unit 0
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, *currRenderedTexture0);
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
            auto vbo = MakeBufferResource();
            static const GLfloat quad_vertices[] = {
                -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, -1.0f, 1.0f, 0.0f,
                -1.0f, 1.0f,  0.0f, 1.0f, -1.0f, 0.0f, 1.0f,  1.0f, 0.0f,
            };
            SetVertexBufferData(*vbo, quad_vertices);

            glEnableVertexAttribArray(0);
            glBindBuffer(GL_ARRAY_BUFFER, *vbo);
            glVertexAttribPointer(
                0, // attribute 0. No particular reason for 0, but must match layout in the shader.
                3, // size
                GL_FLOAT, // type
                GL_FALSE, // normalized?
                0,        // stride
                (void*)0  // array buffer offset
            );

            glDrawArrays(GL_TRIANGLES, 0, 6); // 2*3 indices starting at 0 -> 2 triangles
            glDisableVertexAttribArray(0);
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

            {
                // Bind our texture in Texture Unit 0
                glActiveTexture(GL_TEXTURE0);
                // NOTE: We render texture 0 rather than 1 so that lines just drawn this frame will
                // be at their full brightness for one frame before being rendered darkened.
                // glBindTexture(GL_TEXTURE_2D, *currRenderedTexture1);
                glBindTexture(GL_TEXTURE_2D, *currRenderedTexture0);
                GLuint texLoc =
                    glGetUniformLocation(ShaderProgram::textureToScreen, "renderedTexture");
                // Set our "renderedTexture1" sampler to use Texture Unit 0
                glUniform1i(texLoc, 0);
            }

            {
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, *g_overlayTexture);
                GLuint texLoc =
                    glGetUniformLocation(ShaderProgram::textureToScreen, "overlayTexture");
                glUniform1i(texLoc, 1);
                GLuint overlayAlphaLoc =
                    glGetUniformLocation(ShaderProgram::textureToScreen, "overlayAlpha");
                static volatile float overlayAlpha = 1.0f;
                glUniform1f(overlayAlphaLoc, overlayAlpha);
            }

            //@TODO: create once
            auto vbo = MakeBufferResource();
            static const GLfloat quad_vertices[] = {
                -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, -1.0f, 1.0f, 0.0f,
                -1.0f, 1.0f,  0.0f, 1.0f, -1.0f, 0.0f, 1.0f,  1.0f, 0.0f,
            };
            SetVertexBufferData(*vbo, quad_vertices);

            glEnableVertexAttribArray(0);
            glBindBuffer(GL_ARRAY_BUFFER, *vbo);
            glVertexAttribPointer(0, // attribute 0. No particular reason for 0, but must match the
                                     // layout in the shader.
                                  3, // size
                                  GL_FLOAT, // type
                                  GL_FALSE, // normalized?
                                  0,        // stride
                                  (void*)0  // array buffer offset
            );

            glDrawArrays(GL_TRIANGLES, 0, 6); // 2*3 indices starting at 0 -> 2 triangles
            glDisableVertexAttribArray(0);
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
