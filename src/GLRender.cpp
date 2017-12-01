#include "GLRender.h"
#include "EngineClient.h"
#include "ImageFileUtils.h"

#include <gl/glew.h> // Must be included before gl.h

#include <SDL_opengl.h> // Wraps OpenGL headers
#include <SDL_opengl_glext.h>

#include <gl/GLU.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <imgui.h>

#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

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

    template <typename T, size_t N>
    void SetVertexBufferData(GLuint vboId, const std::array<T, N>& vertices) {
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
            int infoLogLength{};
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

    struct PixelData {
        uint8_t* pixels{};
        GLenum format{};
        GLenum type{};
    };
    void AllocateTexture(GLuint textureId, GLsizei width, GLsizei height, GLint internalFormat,
                         std::optional<PixelData> pixelData = {}) {

        auto pd = pixelData.value_or(PixelData{nullptr, GL_RGBA, GL_UNSIGNED_BYTE});

        glBindTexture(GL_TEXTURE_2D, textureId);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, pd.format, pd.type,
                     pd.pixels);
        // By default, filtering is GL_LINEAR or GL_NEAREST_MIPMAP_LINEAR. We set to GL_NEAREST to
        // avoid creating mipmaps and to make it less blurry.
        // auto filtering = GL_LINEAR;
        auto filtering = GL_NEAREST;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filtering);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filtering);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    bool LoadPngTexture(GLuint textureId, const char* file) {
        auto imgData = ImageFileUtils::loadPngImage(file);

        if (!imgData) {
            return false;
        }

        AllocateTexture(
            textureId, imgData->width, imgData->height, imgData->hasAlpha ? GL_RGBA : GL_RGB,
            PixelData{imgData->data.get(),
                      static_cast<GLenum>(imgData->hasAlpha ? GL_RGBA : GL_RGB), GL_UNSIGNED_BYTE});
        return true;
    }

    struct Viewport {
        GLint x{}, y{};
        GLsizei w{}, h{};
    };
    Viewport GetBestFitViewport(float targetAR, int windowWidth, int windowHeight) {
        float windowWidthF = static_cast<float>(windowWidth);
        float windowHeightF = static_cast<float>(windowHeight);
        float windowAR = windowWidthF / windowHeightF;

        bool fitToWindowHeight = targetAR <= windowAR;

        const auto [targetWidth, targetHeight] = [&] {
            if (fitToWindowHeight) {
                // fit to window height
                return std::make_tuple(targetAR * windowHeightF, windowHeightF);
            } else {
                // fit to window width
                return std::make_tuple(windowWidthF, 1 / targetAR * windowWidthF);
            }
        }();

        return {static_cast<GLint>((windowWidthF - targetWidth) / 2.0f),
                static_cast<GLint>((windowHeightF - targetHeight) / 2.0f),
                static_cast<GLsizei>(targetWidth), static_cast<GLsizei>(targetHeight)};
    }

    // Globals
    float CRT_SCALE_X = 0.93f;
    float CRT_SCALE_Y = 0.76f;
    int g_windowWidth{}, g_windowHeight{};

    Viewport g_windowViewport{}, g_overlayViewport{}, g_crtViewport{};
    std::vector<glm::vec2> g_lineVA, g_pointVA;

    namespace ShaderProgram {
        GLuint renderToTexture{};
        GLuint darkenTexture{};
        GLuint gameScreenToCrtTexture{};
        GLuint drawScreen{};
    } // namespace ShaderProgram

    glm::mat4x4 g_projectionMatrix{};
    glm::mat4x4 g_modelViewMatrix{};
    VertexArrayResource g_topLevelVAO;
    FrameBufferResource g_textureFB;
    int g_renderedTexture0Index{};
    TextureResource g_renderedTexture0;
    TextureResource g_renderedTexture1;
    TextureResource g_crtTexture;
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
        //@TODO: Use PassThrough.vert for renderToTexture as well (don't forget to pass UVs along)
        ShaderProgram::renderToTexture =
            LoadShaders("shaders/DrawVectors.vert", "shaders/DrawVectors.frag");

        ShaderProgram::darkenTexture =
            LoadShaders("shaders/PassThrough.vert", "shaders/DarkenTexture.frag");

        ShaderProgram::gameScreenToCrtTexture =
            LoadShaders("shaders/Passthrough.vert", "shaders/DrawTexture.frag");

        ShaderProgram::drawScreen =
            LoadShaders("shaders/Passthrough.vert", "shaders/DrawScreen.frag");

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

            // If we fail, then allocate a min-sized transparent texture
            std::vector<uint8_t> emptyTexture;
            emptyTexture.resize(64 * 64 * 4);
            AllocateTexture(*g_overlayTexture, 64, 64, GL_RGBA,
                            PixelData{&emptyTexture[0], GL_RGBA, GL_UNSIGNED_BYTE});
        }
    }

    void Shutdown() {
        //
    }

    std::tuple<int, int> GetMajorMinorVersion() { return {3, 3}; }

    void SetViewport(const Viewport& vp) {
        glViewport((GLint)vp.x, (GLint)vp.y, (GLsizei)vp.w, (GLsizei)vp.h);
    }

    bool OnWindowResized(int windowWidth, int windowHeight) {
        if (windowHeight == 0) {
            windowHeight = 1;
        }
        g_windowWidth = windowWidth;
        g_windowHeight = windowHeight;

        // Overlay
        float overlayAR = 936.f / 1200.f;
        g_windowViewport = GetBestFitViewport(overlayAR, windowWidth, windowHeight);
        g_overlayViewport = {0, 0, g_windowViewport.w, g_windowViewport.h};
        g_crtViewport = {0, 0, static_cast<GLsizei>(g_overlayViewport.w * CRT_SCALE_X),
                         static_cast<GLsizei>(g_overlayViewport.h * CRT_SCALE_Y)};

        SetViewport(g_overlayViewport);

        double halfWidth = VECTREX_SCREEN_WIDTH / 2.0;
        double halfHeight = VECTREX_SCREEN_HEIGHT / 2.0;
        g_projectionMatrix = glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight);

        g_modelViewMatrix = {};

        // (Re)create resources that depend on viewport size
        g_renderedTexture0 = MakeTextureResource();
        AllocateTexture(*g_renderedTexture0, g_crtViewport.w, g_crtViewport.h, GL_RGB32F);
        g_renderedTexture1 = MakeTextureResource();
        AllocateTexture(*g_renderedTexture1, g_crtViewport.w, g_crtViewport.h, GL_RGB32F);
        g_crtTexture = MakeTextureResource();
        AllocateTexture(*g_crtTexture, g_overlayViewport.w, g_overlayViewport.h, GL_RGB);
        glObjectLabel(GL_TEXTURE, *g_crtTexture, -1, "g_crtTexture");

        // Clear g_renderedTexture0 once
        glBindFramebuffer(GL_FRAMEBUFFER, *g_textureFB);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, *g_renderedTexture0, 0);
        SetViewport(g_overlayViewport);
        glClear(GL_COLOR_BUFFER_BIT);

        return true;
    }

    void RenderScene(double frameTime) {
        // Force resize on crt scale change
        {
            static float scaleX = CRT_SCALE_X;
            static float scaleY = CRT_SCALE_Y;
            ImGui::SliderFloat("scaleX", &scaleX, 0.0f, 1.0f);
            ImGui::SliderFloat("scaleY", &scaleY, 0.0f, 1.0f);

            if (scaleX != CRT_SCALE_X || scaleY != CRT_SCALE_Y) {
                CRT_SCALE_X = scaleX;
                CRT_SCALE_Y = scaleY;
                OnWindowResized(g_windowWidth, g_windowHeight);
            }
        }

        if (frameTime > 0)
            g_renderedTexture0Index = (g_renderedTexture0Index + 1) % 2;

        auto& currRenderedTexture0 =
            g_renderedTexture0Index == 0 ? g_renderedTexture0 : g_renderedTexture1;
        auto& currRenderedTexture1 =
            g_renderedTexture0Index == 0 ? g_renderedTexture1 : g_renderedTexture0;

        glObjectLabel(GL_TEXTURE, *currRenderedTexture0, -1, "currRenderedTexture0");
        glObjectLabel(GL_TEXTURE, *currRenderedTexture1, -1, "currRenderedTexture1");

        auto MakeClipSpaceQuad = [](float scaleX = 1.f, float scaleY = 1.f) {
            std::array<glm::vec3, 6> quad_vertices{
                glm::vec3{-scaleX, -scaleY, 0.0f}, glm::vec3{scaleX, -scaleY, 0.0f},
                glm::vec3{-scaleX, scaleY, 0.0f},  glm::vec3{-scaleX, scaleY, 0.0f},
                glm::vec3{scaleX, -scaleY, 0.0f},  glm::vec3{scaleX, scaleY, 0.0f}};
            return quad_vertices;
        };

        /////////////////////////////////////////////////////////////////
        // PASS 1: render to texture
        /////////////////////////////////////////////////////////////////
        {
            // Render to our framebuffer
            glBindFramebuffer(GL_FRAMEBUFFER, *g_textureFB);
            SetViewport(g_crtViewport); //@TODO: maybe get Viewport from texture
                                        // dimensions? ({0,0,texW,textH})
            glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, *currRenderedTexture0, 0);

            // Purposely do not clear the target texture.
            // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Use our shader
            glUseProgram(ShaderProgram::renderToTexture);

            const auto mvp = g_projectionMatrix * g_modelViewMatrix;
            GLuint mvpUniform = glGetUniformLocation(ShaderProgram::renderToTexture, "MVP");
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
            SetViewport(g_crtViewport);
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

            static float darkenSpeedScale = 3.0;
            ImGui::SliderFloat("darkenSpeedScale", &darkenSpeedScale, 0.0f, 10.0f);
            glUniform1f(darkenSpeedScaleID, darkenSpeedScale);
            glUniform1f(frameTimeID, (float)(frameTime));

            //@TODO: create once
            auto vbo = MakeBufferResource();
            auto quad_vertices = MakeClipSpaceQuad();
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

            glm::vec2 quad_uvs[] = {{0, 0}, {1, 0}, {0, 1}, {0, 1}, {1, 0}, {1, 1}};
            auto uvBuffer = MakeBufferResource();
            SetVertexBufferData(*uvBuffer, quad_uvs);

            // 2nd attribute buffer : UVs
            glEnableVertexAttribArray(1);
            glBindBuffer(GL_ARRAY_BUFFER, *uvBuffer);
            glVertexAttribPointer(1,        // attribute
                                  2,        // size
                                  GL_FLOAT, // type
                                  GL_FALSE, // normalized?
                                  0,        // stride
                                  (void*)0  // array buffer offset
            );

            glDrawArrays(GL_TRIANGLES, 0, 6); // 2*3 indices starting at 0 -> 2 triangles

            glDisableVertexAttribArray(0);
            glDisableVertexAttribArray(1);
        }

        /////////////////////////////////////////////////////////////////
        // PASS 3: render game screen texture to crt texture that is
        //         larger (same size as overlay texture)
        /////////////////////////////////////////////////////////////////
        {
            glBindFramebuffer(GL_FRAMEBUFFER, *g_textureFB);
            SetViewport(g_overlayViewport);
            glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, *g_crtTexture, 0);

            glClear(GL_COLOR_BUFFER_BIT);

            glUseProgram(ShaderProgram::gameScreenToCrtTexture);

            // Bind our texture in Texture Unit 0
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, *currRenderedTexture0);
            GLuint texID = glGetUniformLocation(ShaderProgram::darkenTexture, "renderedTexture");
            // Set our "renderedTexture0" sampler to use Texture Unit 0
            glUniform1i(texID, 0);

            auto vbo = MakeBufferResource();
            auto quad_vertices = MakeClipSpaceQuad(CRT_SCALE_X, CRT_SCALE_Y);
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

            glm::vec2 quad_uvs[] = {{0, 0}, {1, 0}, {0, 1}, {0, 1}, {1, 0}, {1, 1}};
            auto uvBuffer = MakeBufferResource();
            SetVertexBufferData(*uvBuffer, quad_uvs);

            // 2nd attribute buffer : UVs
            glEnableVertexAttribArray(1);
            glBindBuffer(GL_ARRAY_BUFFER, *uvBuffer);
            glVertexAttribPointer(1,        // attribute
                                  2,        // size
                                  GL_FLOAT, // type
                                  GL_FALSE, // normalized?
                                  0,        // stride
                                  (void*)0  // array buffer offset
            );

            glDrawArrays(GL_TRIANGLES, 0, 6); // 2*3 indices starting at 0 -> 2 triangles

            glDisableVertexAttribArray(0);
            glDisableVertexAttribArray(1);
        }

        /////////////////////////////////////////////////////////////////
        // PASS 4: render to screen
        /////////////////////////////////////////////////////////////////
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            SetViewport(g_windowViewport);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Use our shader
            glUseProgram(ShaderProgram::drawScreen);

            //@TODO: function SetTextureUniform
            {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, *g_crtTexture);
                GLuint texLoc = glGetUniformLocation(ShaderProgram::drawScreen, "crtTexture");
                glUniform1i(texLoc, 0);
            }
            {
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, *g_overlayTexture);
                GLuint texLoc = glGetUniformLocation(ShaderProgram::drawScreen, "overlayTexture");
                glUniform1i(texLoc, 1);
            }

            {
                GLuint overlayAlphaLoc =
                    glGetUniformLocation(ShaderProgram::drawScreen, "overlayAlpha");
                static float overlayAlpha = 1.0f;
                ImGui::SliderFloat("overlayAlpha", &overlayAlpha, 0.0f, 1.0f);
                glUniform1f(overlayAlphaLoc, overlayAlpha);
            }

            auto vbo = MakeBufferResource();
            auto quad_vertices = MakeClipSpaceQuad();
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

            glm::vec2 quad_uvs[] = {{0, 0}, {1, 0}, {0, 1}, {0, 1}, {1, 0}, {1, 1}};
            auto uvBuffer = MakeBufferResource();
            SetVertexBufferData(*uvBuffer, quad_uvs);

            // 2nd attribute buffer : UVs
            glEnableVertexAttribArray(1);
            glBindBuffer(GL_ARRAY_BUFFER, *uvBuffer);
            glVertexAttribPointer(1,        // attribute
                                  2,        // size
                                  GL_FLOAT, // type
                                  GL_FALSE, // normalized?
                                  0,        // stride
                                  (void*)0  // array buffer offset
            );

            glDrawArrays(GL_TRIANGLES, 0, 6); // 2*3 indices starting at 0 -> 2 triangles

            glDisableVertexAttribArray(0);
            glDisableVertexAttribArray(1);
        }
    } // namespace GLRender
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
