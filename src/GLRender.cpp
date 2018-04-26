#include "GLRender.h"
#include "ConsoleOutput.h"
#include "EngineClient.h"
#include "GLUtil.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <imgui.h>

#include <optional>
#include <string>
#include <vector>

using namespace GLUtil;

// Vectrex screen dimensions
const int VECTREX_SCREEN_WIDTH = 256;
const int VECTREX_SCREEN_HEIGHT = 256;

namespace {
    // Tweakables - @TODO: make const in final build
    float OverlayAR = 936.f / 1200.f;
    int MaxTextureSize = 1024;
    bool EnableBlur = true;
    bool ThickBaseLines = true;
    float LineWidthNormal = 0.4f;
    float LineWidthGlow = 1.f;
    float GlowRadius = 1.2f;
    float DarkenSpeedScale = 3.0;
    float OverlayAlpha = 1.0f;
    float CrtScaleX = 1.f; // 0.93f;
    float CrtScaleY = 0.8f;

    struct Viewport {
        GLint x{}, y{};
        GLsizei w{}, h{};
    };

    void SetViewport(const Viewport& vp) {
        glViewport((GLint)vp.x, (GLint)vp.y, (GLsizei)vp.w, (GLsizei)vp.h);
    }

    void SetViewportToTextureDims(const Texture& texture) {
        glViewport(0, 0, texture.Width(), texture.Height());
    }

    Viewport GetBestFitViewport(float targetAR, int windowWidth, int windowHeight) {
        float windowWidthF = static_cast<float>(windowWidth);
        float windowHeightF = static_cast<float>(windowHeight);
        float windowAR = windowWidthF / windowHeightF;

        bool fitToWindowHeight = targetAR <= windowAR;

        const auto[targetWidth, targetHeight] = [&] {
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

    std::tuple<int, int> ScaleDimensions(int width, int height, int maxWidth, int maxHeight) {
        if (width > maxWidth || height > maxHeight) {
            if (width >= height) {
                height = static_cast<int>(height * static_cast<float>(maxWidth) / width);
                width = maxWidth;
            } else {
                width = static_cast<int>(width * static_cast<float>(maxHeight) / height);
                height = maxHeight;
            }
        }
        return {width, height};
    }

    struct VertexData {
        glm::vec2 v{};
        float brightness{};
    };

    std::vector<VertexData> CreateQuadVertexArray(const std::vector<Line>& lines, float lineWidth,
                                                  float scaleX, float scaleY) {
        auto AlmostEqual = [](float a, float b, float epsilon = 0.01f) {
            return abs(a - b) <= epsilon;
        };

        std::vector<VertexData> result;

        const float MinPixelDist = 1.f;

        // Make sure line width is at least one pixel wide to ensure it gets rendered
        lineWidth = std::max(lineWidth, MinPixelDist);

        float hlw = lineWidth / 2.0f;

        for (auto& line : lines) {
            // If end points are close, draw a dot instead of a line. We do this before applying any
            // scale.
            const bool isPoint = Magnitude(line.p0 - line.p1) <= 0.1f;

            glm::vec2 p0{line.p0.x * scaleX, line.p0.y * scaleY};
            glm::vec2 p1{line.p1.x * scaleX, line.p1.y * scaleY};

            if (isPoint) {
                auto a = VertexData{p0 + glm::vec2{hlw, hlw}, line.brightness};
                auto b = VertexData{p0 + glm::vec2{hlw, -hlw}, line.brightness};
                auto c = VertexData{p0 + glm::vec2{-hlw, -hlw}, line.brightness};
                auto d = VertexData{p0 + glm::vec2{-hlw, hlw}, line.brightness};

                result.insert(result.end(), {a, b, c, c, d, a});
            } else {
                auto v01 = p1 - p0;
                glm::vec2 n = glm::normalize(v01);

                // Make sure line gets at least one pixel coverage to ensure it gets rendered.
                // Note that we extend p1, the end point, which means we may get some slight errors
                // with attached lines. If we were to store "line strips" instead, we could correct
                // the point in the strip, ensuring that strips don't get detached.
                if (abs(v01.x) < MinPixelDist) {
                    p1.x = p0.x + n.x * MinPixelDist;
                }
                if (abs(v01.y) < MinPixelDist) {
                    p1.y = p0.y + n.y * MinPixelDist;
                }

                glm::vec2 bn(-n.y, n.x);

                auto a = VertexData{p0 + bn * hlw, line.brightness};
                auto b = VertexData{p0 - bn * hlw, line.brightness};
                auto c = VertexData{p1 - bn * hlw, line.brightness};
                auto d = VertexData{p1 + bn * hlw, line.brightness};

                result.insert(result.end(), {a, b, c, c, d, a});
            }
        }
        return result;
    }

    std::tuple<std::vector<VertexData>, std::vector<VertexData>>
    CreateLineAndPointVertexArrays(const std::vector<Line>& lines, float scaleX, float scaleY) {
        auto AlmostEqual = [](float a, float b, float epsilon = 0.01f) {
            return abs(a - b) <= epsilon;
        };

        std::vector<VertexData> lineVA, pointVA;

        for (auto& line : lines) {
            glm::vec2 p0{line.p0.x * scaleX, line.p0.y * scaleY};
            glm::vec2 p1{line.p1.x * scaleX, line.p1.y * scaleY};

            if (AlmostEqual(p0.x, p1.x) && AlmostEqual(p0.y, p1.y)) {
                pointVA.push_back({p0, line.brightness});
            } else {
                lineVA.push_back({p0, line.brightness});
                lineVA.push_back({p1, line.brightness});
            }
        }

        return {lineVA, pointVA};
    }

    std::array<glm::vec3, 6> MakeClipSpaceQuad(float scaleX = 1.f, float scaleY = 1.f) {
        return {glm::vec3{-scaleX, -scaleY, 0.0f}, glm::vec3{scaleX, -scaleY, 0.0f},
                glm::vec3{-scaleX, scaleY, 0.0f},  glm::vec3{-scaleX, scaleY, 0.0f},
                glm::vec3{scaleX, -scaleY, 0.0f},  glm::vec3{scaleX, scaleY, 0.0f}};
    };

    void DrawFullScreenQuad(float scaleX = 1.f, float scaleY = 1.f) {
        //@TODO: create once
        auto vbo = MakeBufferResource();

        // Store attributes so we can send it all at once
        struct Attributes {
            std::array<glm::vec3, 6> vertices;
            glm::vec2 uvs[6] = {{0, 0}, {1, 0}, {0, 1}, {0, 1}, {1, 0}, {1, 1}};
        };
        static_assert(std::is_trivially_copyable_v<Attributes>);

        Attributes attributes;
        attributes.vertices = MakeClipSpaceQuad(scaleX, scaleY);

        glBindBuffer(GL_ARRAY_BUFFER, *vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(attributes), &attributes, GL_DYNAMIC_DRAW);

        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);

        glVertexAttribPointer(0,        // attribute 0. Must match layout in the shader.
                              3,        // size
                              GL_FLOAT, // type
                              GL_FALSE, // normalized?
                              0,        // stride
                              (void*)offsetof(Attributes, vertices) // array buffer offset
        );

        // 2nd attribute buffer : UVs
        glVertexAttribPointer(1,        // attribute
                              2,        // size
                              GL_FLOAT, // type
                              GL_FALSE, // normalized?
                              0,        // stride
                              (void*)offsetof(Attributes, uvs));

        glDrawArrays(GL_TRIANGLES, 0, 6); // 2*3 indices starting at 0 -> 2 triangles

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
    };

    // Globals
    int g_windowWidth{}, g_windowHeight{};

    Viewport g_screenViewport{};

    std::vector<VertexData> g_quadVA;
    std::vector<VertexData> g_lineVA, g_pointVA;

    glm::mat4x4 g_projectionMatrix{};
    glm::mat4x4 g_modelViewMatrix{};
    VertexArrayResource g_topLevelVAO;
    FrameBufferResource g_textureFB;
    int g_vectorsTexture0Index{};
    Texture g_vectorsTexture[2];
    Texture g_vectorsThickTexture[2];
    Texture g_tempTexture;
    Texture g_glowTexture;
    Texture g_screenCrtTexture;
    Texture g_overlayTexture;

} // namespace

namespace {
    // Stringified shader source
    namespace ShaderSource {
        const char* CombineVectorsAndGlow_frag =
#include "shaders/CombineVectorsAndGlow.frag"
            ;

        const char* CopyTexture_frag =
#include "shaders/CopyTexture.frag"
            ;

        const char* DarkenTexture_frag =
#include "shaders/DarkenTexture.frag"
            ;

        const char* DrawScreen_frag =
#include "shaders/DrawScreen.frag"
            ;

        const char* DrawTexture_frag =
#include "shaders/DrawTexture.frag"
            ;

        const char* DrawVectors_frag =
#include "shaders/DrawVectors.frag"
            ;

        const char* DrawVectors_vert =
#include "shaders/DrawVectors.vert"
            ;

        const char* Glow_frag =
#include "shaders/Glow.frag"
            ;

        const char* Passthrough_vert =
#include "shaders/Passthrough.vert"
            ;
    } // namespace ShaderSource

    class ShaderPass {
    protected:
        ShaderPass() = default;
        Shader m_shader;
    };

    class DrawVectorsPass : public ShaderPass {
    public:
        void Init() {
            m_shader.LoadShaders(ShaderSource::DrawVectors_vert, ShaderSource::DrawVectors_frag);
        }

        void Draw(const std::vector<VertexData>& VA1, GLenum mode1,
                  const std::vector<VertexData>& VA2, GLenum mode2, const Texture& outputTexture) {
            ScopedDebugGroup sdg("DrawVectorsPass");

            SetFrameBufferTexture(*g_textureFB, outputTexture.Id());
            SetViewportToTextureDims(outputTexture);

            // Purposely do not clear the target texture as we want to darken it.
            // glClear(GL_COLOR_BUFFER_BIT);

            m_shader.Bind();

            const auto mvp = g_projectionMatrix * g_modelViewMatrix;
            SetUniformMatrix4v(m_shader.Id(), "MVP", &mvp[0][0]);

            auto DrawVertices = [](auto& VA, GLenum mode) {
                if (VA.size() == 0)
                    return;

                auto vbo = MakeBufferResource();

                glBindBuffer(GL_ARRAY_BUFFER, *vbo);
                glBufferData(GL_ARRAY_BUFFER, VA.size() * sizeof(VertexData), VA.data(),
                             GL_DYNAMIC_DRAW);

                glEnableVertexAttribArray(0);
                glEnableVertexAttribArray(1);

                // Vertices
                glVertexAttribPointer(0,                             // attribute
                                      2,                             // size
                                      GL_FLOAT,                      // type
                                      GL_FALSE,                      // normalized?
                                      sizeof(VertexData),            // stride
                                      (void*)offsetof(VertexData, v) // array buffer offset
                );

                // Brightness values
                glVertexAttribPointer(1,                                      // attribute
                                      1,                                      // size
                                      GL_FLOAT,                               // type
                                      GL_FALSE,                               // normalized?
                                      sizeof(VertexData),                     // stride
                                      (void*)offsetof(VertexData, brightness) // array buffer offset
                );

                glDrawArrays(mode, 0, checked_static_cast<GLsizei>(VA.size()));

                glDisableVertexAttribArray(1);
                glDisableVertexAttribArray(0);
            };

            DrawVertices(VA1, mode1);
            DrawVertices(VA2, mode2);
        }
    };

    class DarkenTexturePass : public ShaderPass {
    public:
        void Init() {
            m_shader.LoadShaders(ShaderSource::Passthrough_vert, ShaderSource::DarkenTexture_frag);
        }

        void Draw(const Texture& inputTexture, const Texture& outputTexture, float frameTime) {
            ScopedDebugGroup sdg("DarkenTexturePass");

            ImGui::SliderFloat("DarkenSpeedScale", &DarkenSpeedScale, 0.0f, 10.0f);

            SetFrameBufferTexture(*g_textureFB, outputTexture.Id());
            SetViewportToTextureDims(outputTexture);
            // No need to clear as we write every pixel
            // glClear(GL_COLOR_BUFFER_BIT);

            m_shader.Bind();

            SetTextureUniform(m_shader.Id(), "vectorsTexture", inputTexture.Id(), 0);
            SetUniform(m_shader.Id(), "darkenSpeedScale", DarkenSpeedScale);
            SetUniform(m_shader.Id(), "frameTime", frameTime);

            DrawFullScreenQuad();
        };
    };

    class GlowPass : public ShaderPass {
    public:
        void Init() {
            m_shader.LoadShaders(ShaderSource::Passthrough_vert, ShaderSource::Glow_frag);
        }

        void Draw(const Texture& inputTexture, const Texture& tempTexture,
                  const Texture& outputTexture) {
            ScopedDebugGroup sdg("GlowPass");

            ImGui::SliderFloat("GlowRadius", &GlowRadius, 0.0f, 5.0f);

            for (size_t i = 0; i < m_glowKernelValues.size(); ++i) {
                ImGui::SliderFloat(FormattedString<>("kernelValue[%d]", i), &m_glowKernelValues[i],
                                   0.f, 1.f);
            }

            GlowInDirection(inputTexture, tempTexture, {1.f, 0.f});
            GlowInDirection(tempTexture, outputTexture, {0.f, 1.f});
        };

    private:
        void GlowInDirection(const Texture& inputTexture, const Texture& outputTexture,
                             glm::vec2 dir) {
            SetFrameBufferTexture(*g_textureFB, outputTexture.Id());
            SetViewportToTextureDims(outputTexture);

            m_shader.Bind();

            SetTextureUniform(m_shader.Id(), "inputTexture", inputTexture.Id(), 0);
            SetUniform(m_shader.Id(), "dir", dir.x, dir.y);
            SetUniform(m_shader.Id(), "resolution", static_cast<float>(outputTexture.Width()));
            SetUniform(m_shader.Id(), "radius", GlowRadius);
            SetUniform(m_shader.Id(), "kernalValues", &m_glowKernelValues[0],
                       checked_static_cast<GLint>(m_glowKernelValues.size()));

            DrawFullScreenQuad();
        };

        std::array<float, 5> m_glowKernelValues = {
            0.2270270270f, 0.1945945946f, 0.1216216216f, 0.0540540541f, 0.0162162162f,
        };
    };

    class CombineVectorsAndGlowPass : public ShaderPass {
    public:
        void Init() {
            m_shader.LoadShaders(ShaderSource::Passthrough_vert,
                                 ShaderSource::CombineVectorsAndGlow_frag);
        }

        void Draw(const Texture& inputVectorsTexture, const Texture& inputGlowTexture,
                  const Texture& outputTexture) {
            ScopedDebugGroup sdg("CombineVectorsAndGlowPass");

            SetFrameBufferTexture(*g_textureFB, outputTexture.Id());
            SetViewportToTextureDims(outputTexture);
            // No need to clear as we write every pixel
            // glClear(GL_COLOR_BUFFER_BIT);

            m_shader.Bind();

            SetTextureUniform(m_shader.Id(), "vectorsTexture", inputVectorsTexture.Id(), 0);
            SetTextureUniform(m_shader.Id(), "glowTexture", inputGlowTexture.Id(), 1);

            DrawFullScreenQuad();
        }
    };

    class ScaleTexturePass : public ShaderPass {
    public:
        void Init() {
            m_shader.LoadShaders(ShaderSource::Passthrough_vert, ShaderSource::DrawTexture_frag);
        }

        void Draw(const Texture& inputTexture, const Texture& outputTexture, float scaleX,
                  float scaleY) {
            ScopedDebugGroup sdg("ScaleTexturePass");

            SetFrameBufferTexture(*g_textureFB, outputTexture.Id());
            SetViewportToTextureDims(outputTexture);
            // Clear target texture as we only write to scaled portion which can change over time
            // (only because we allow tweaking).
            glClear(GL_COLOR_BUFFER_BIT);

            m_shader.Bind();

            SetTextureUniform(m_shader.Id(), "vectorsTexture", inputTexture.Id(), 0);

            DrawFullScreenQuad(scaleX, scaleY);
        }
    };

    class RenderToScreenPass : public ShaderPass {
    public:
        void Init() {
            m_shader.LoadShaders(ShaderSource::Passthrough_vert, ShaderSource::DrawScreen_frag);
        }

        void Draw(const Texture& inputCrtTexture, const Texture& inputOverlayTexture) {
            ScopedDebugGroup sdg("RenderToScreenPass");

            ImGui::SliderFloat("OverlayAlpha", &OverlayAlpha, 0.0f, 1.0f);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            SetViewport(g_screenViewport);
            // No need to clear as we write every pixel
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            m_shader.Bind();

            SetTextureUniform(m_shader.Id(), "crtTexture", inputCrtTexture.Id(), 0);
            SetTextureUniform(m_shader.Id(), "overlayTexture", inputOverlayTexture.Id(), 1);
            SetUniform(m_shader.Id(), "overlayAlpha", OverlayAlpha);

            DrawFullScreenQuad();
        }
    };

    void GLDebugMessageCallback(const GLUtil::GLDebugMessageInfo& info) {
        Errorf("OpenGL Debug Message: %s [source=0x%X type=0x%X severity=0x%X]\n", info.message,
               info.source, info.type, info.severity);
    }

    // Global shader pass instances
    DrawVectorsPass g_drawVectorsPass;
    DarkenTexturePass g_darkenTexturePass;
    GlowPass g_glowPass;
    CombineVectorsAndGlowPass g_combineVectorsAndGlowPass;
    ScaleTexturePass g_scaleTexturePass;
    RenderToScreenPass g_renderToScreenPass;

} // namespace

namespace GLRender {
    std::tuple<int, int> GetMajorMinorVersion() { return {3, 3}; }
    bool EnableGLDebugging() { return true; }

    void Initialize() {
        // glShadeModel(GL_SMOOTH);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        // glClearDepth(1.0f);
        glDisable(GL_DEPTH_TEST);
        // glDepthFunc(GL_LEQUAL);
        // glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

        // Initialize GLEW
        glewExperimental = GL_TRUE; // Needed for core profile
        if (glewInit() != GLEW_OK) {
            FAIL_MSG("Failed to initialize GLEW");
        }

        if (EnableGLDebugging()) {
            GLUtil::SetDebugMessageCallback(GLDebugMessageCallback);
        }

        // We don't actually use VAOs, but we must create one and bind it so that we can use VBOs
        g_topLevelVAO = MakeVertexArrayResource();
        glBindVertexArray(*g_topLevelVAO);

        // Load shaders
        g_drawVectorsPass.Init();
        g_darkenTexturePass.Init();
        g_glowPass.Init();
        g_combineVectorsAndGlowPass.Init();
        g_scaleTexturePass.Init();
        g_renderToScreenPass.Init();

        // Create resources
        g_textureFB = MakeFrameBufferResource();
        // Set output of fragment shader to color attachment 0
        glBindFramebuffer(GL_FRAMEBUFFER, *g_textureFB);
        GLenum drawBuffers[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, drawBuffers);
        CheckFramebufferStatus();

        ResetOverlay();
    }

    void Shutdown() {
        //
    }

    void ResetOverlay(const char* file) {
        auto CreateEmptyOverlayTexture = [] {
            std::vector<uint8_t> emptyTexture;
            emptyTexture.resize(64 * 64 * 4);
            g_overlayTexture.Allocate(64, 64, GL_RGBA, {},
                                      PixelData{&emptyTexture[0], GL_RGBA, GL_UNSIGNED_BYTE});
        };

        if (!file) {
            CreateEmptyOverlayTexture();
        } else if (!g_overlayTexture.LoadPng(file, GL_LINEAR)) {
            Errorf("Failed to load overlay: %s\n", file);

            // If we fail, then allocate a min-sized transparent texture
            CreateEmptyOverlayTexture();
        }

        g_overlayTexture.SetName("g_overlayTexture");
    }

    bool OnWindowResized(int windowWidth, int windowHeight) {
        if (windowHeight == 0) {
            windowHeight = 1;
        }
        g_windowWidth = windowWidth;
        g_windowHeight = windowHeight;

        //  ------------------------
        // |     |   -----   |      |<- Window
        // |     |  |     |  |      |
        // |     |  |     |  |<- Overlay (also Screen)
        // |     |  |     |  |      |
        // |     |  |     |<- CRT   |
        // |     |   -----   |      |
        //  ------------------------

        // "Screen" is the game view area. Overlay fills this area, so we use the overlay aspect
        // ratio to determine the screen size as a ratio of the window size.
        g_screenViewport = GetBestFitViewport(OverlayAR, windowWidth, windowHeight);

        // Now we scale the screen width/height used to determine our texture sizes so that they're
        // not too large. This is especially important on high DPI displays.
        const auto[screenTextureWidth, screenTextureHeight] =
            ScaleDimensions(g_screenViewport.w, g_screenViewport.h, MaxTextureSize, MaxTextureSize);

        // "CRT" represents the physical CRT screen where the line vectors are drawn. The size is
        // smaller than the screen since the overlay is larger than the CRT on the Vectrex.
        const auto[crtWidth, crtHeight] =
            std::make_tuple(static_cast<GLsizei>(screenTextureWidth * CrtScaleX),
                            static_cast<GLsizei>(screenTextureHeight * CrtScaleY));

        // We use orthographic projection to scale 256x256 vectrex screen to CRT texture size. We do
        // this so that the lines we draw won't be scaled/skewed since the target isn't square.
        const double halfWidth = crtWidth / 2.0;
        const double halfHeight = crtHeight / 2.0;
        g_projectionMatrix = glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight);

        g_modelViewMatrix = {};

        // (Re)create resources that depend on viewport size
        g_vectorsTexture[0].Allocate(crtWidth, crtHeight, GL_RGB32F);
        g_vectorsTexture[1].Allocate(crtWidth, crtHeight, GL_RGB32F);
        g_vectorsThickTexture[0].Allocate(crtWidth, crtHeight, GL_RGB32F);
        g_vectorsThickTexture[1].Allocate(crtWidth, crtHeight, GL_RGB32F);

        g_tempTexture.Allocate(crtWidth, crtHeight, GL_RGB32F);
        g_tempTexture.SetName("g_tempTexture");

        g_glowTexture.Allocate(crtWidth, crtHeight, GL_RGB32F);
        g_glowTexture.SetName("g_glowTexture");

        // Final CRT texture is the same size as the screen so we can combine it with the overlay
        // texture (also same size)
        g_screenCrtTexture.Allocate(screenTextureWidth, screenTextureHeight, GL_RGB);
        g_screenCrtTexture.SetName("g_screenCrtTexture");

        // Clear g_vectorsTexture[0] once
        SetFrameBufferTexture(*g_textureFB, g_vectorsTexture[0].Id());
        SetViewportToTextureDims(g_vectorsTexture[0]);
        glClear(GL_COLOR_BUFFER_BIT);

        return true;
    }

    void RenderScene(double frameTime, const RenderContext& renderContext) {
        // Force resize on crt scale change
        {
            static float scaleX = CrtScaleX;
            static float scaleY = CrtScaleY;
            ImGui::SliderFloat("ScaleX", &scaleX, 0.0f, 1.0f);
            ImGui::SliderFloat("ScaleY", &scaleY, 0.0f, 1.0f);

            static int maxTextureSize = MaxTextureSize;
            ImGui::SliderInt("MaxTextureSize", &maxTextureSize, 100, 2000);

            if (scaleX != CrtScaleX || scaleY != CrtScaleY || maxTextureSize != MaxTextureSize) {
                CrtScaleX = scaleX;
                CrtScaleY = scaleY;
                MaxTextureSize = maxTextureSize;
                OnWindowResized(g_windowWidth, g_windowHeight);
            }
        }

        if (frameTime > 0)
            g_vectorsTexture0Index = (g_vectorsTexture0Index + 1) % 2;

        auto& currVectorsTexture0 = g_vectorsTexture[g_vectorsTexture0Index];
        auto& currVectorsTexture1 = g_vectorsTexture[(g_vectorsTexture0Index + 1) % 2];
        currVectorsTexture0.SetName("currVectorsTexture0");
        currVectorsTexture1.SetName("currVectorsTexture1");

        auto& currVectorsThickTexture0 = g_vectorsThickTexture[g_vectorsTexture0Index];
        auto& currVectorsThickTexture1 = g_vectorsThickTexture[(g_vectorsTexture0Index + 1) % 2];
        currVectorsThickTexture0.SetName("currVectorsThickTexture0");
        currVectorsThickTexture1.SetName("currVectorsThickTexture1");

        // Scale lines from vectrex-space to CRT texture space
        const float lineScaleX =
            static_cast<float>(currVectorsTexture0.Width()) / VECTREX_SCREEN_WIDTH;
        const float lineScaleY =
            static_cast<float>(currVectorsTexture0.Height()) / VECTREX_SCREEN_HEIGHT;
        const float lineWidthScale = lineScaleX;

        // Render normal lines and points, and darken
        ImGui::Checkbox("ThickBaseLines", &ThickBaseLines);
        if (!ThickBaseLines) {
            std::tie(g_lineVA, g_pointVA) =
                CreateLineAndPointVertexArrays(renderContext.lines, lineScaleX, lineScaleY);
            g_drawVectorsPass.Draw(g_lineVA, GL_LINES, g_pointVA, GL_POINTS, currVectorsTexture0);
        } else {
            ImGui::SliderFloat("LineWidthNormal", &LineWidthNormal, 0.1f, 3.0f);
            g_quadVA = CreateQuadVertexArray(renderContext.lines, LineWidthNormal * lineWidthScale,
                                             lineScaleX, lineScaleY);
            g_drawVectorsPass.Draw(g_quadVA, GL_TRIANGLES, {}, {}, currVectorsTexture0);
        }
        g_darkenTexturePass.Draw(currVectorsTexture0, currVectorsTexture1,
                                 static_cast<float>(frameTime));

        ImGui::Checkbox("EnableBlur", &EnableBlur);
        if (EnableBlur) {

            // Render thicker lines for blurring, darken, and apply glow
            ImGui::SliderFloat("LineWidthGlow", &LineWidthGlow, 0.1f, 2.0f);
            g_quadVA = CreateQuadVertexArray(renderContext.lines, LineWidthGlow * lineWidthScale,
                                             lineScaleX, lineScaleY);
            g_drawVectorsPass.Draw(g_quadVA, GL_TRIANGLES, {}, {}, currVectorsThickTexture0);
            g_darkenTexturePass.Draw(currVectorsThickTexture0, currVectorsThickTexture1,
                                     static_cast<float>(frameTime));
            g_glowPass.Draw(currVectorsThickTexture0, g_tempTexture, g_glowTexture);

            // Combine glow and normal lines
            g_combineVectorsAndGlowPass.Draw(currVectorsTexture0, g_glowTexture, g_tempTexture);

            // Scale CRT to screen
            g_scaleTexturePass.Draw(g_tempTexture, g_screenCrtTexture, CrtScaleX, CrtScaleY);
        } else {
            // Scale CRT to screen
            g_scaleTexturePass.Draw(currVectorsTexture0, g_screenCrtTexture, CrtScaleX, CrtScaleY);
        }

        // Present
        g_renderToScreenPass.Draw(g_screenCrtTexture, g_overlayTexture);
    }

} // namespace GLRender
