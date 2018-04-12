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
        auto quad_vertices = MakeClipSpaceQuad(scaleX, scaleY);
        SetVertexBufferData(*vbo, quad_vertices);

        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, *vbo);
        glVertexAttribPointer(0,        // attribute 0. No particular reason for 0, but must match
                                        // layout in the shader.
                              3,        // size
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
    };

    // Globals
    int g_windowWidth{}, g_windowHeight{};

    Viewport g_windowViewport{};

    std::vector<VertexData> g_quadVA;
    std::vector<VertexData> g_lineVA, g_pointVA;

    glm::mat4x4 g_projectionMatrix{};
    glm::mat4x4 g_modelViewMatrix{};
    VertexArrayResource g_topLevelVAO;
    FrameBufferResource g_textureFB;
    int g_vectorsTexture0Index{};
    Texture g_vectorsTexture0;
    Texture g_vectorsTexture1;
    Texture g_vectorsThickTexture0;
    Texture g_vectorsThickTexture1;
    Texture g_tempTexture;
    Texture g_glowTexture;
    Texture g_crtTexture;
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
            SetFrameBufferTexture(*g_textureFB, outputTexture.Id());
            SetViewportToTextureDims(outputTexture);

            // Purposely do not clear the target texture.
            // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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

                glDrawArrays(mode, 0, VA.size());

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
            ImGui::SliderFloat("DarkenSpeedScale", &DarkenSpeedScale, 0.0f, 10.0f);

            SetFrameBufferTexture(*g_textureFB, outputTexture.Id());
            SetViewportToTextureDims(outputTexture);
            glClear(GL_COLOR_BUFFER_BIT);

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
                       m_glowKernelValues.size());

            DrawFullScreenQuad();
        };

        std::array<float, 5> m_glowKernelValues = {
            0.2270270270f, 0.1945945946f, 0.1216216216f, 0.0540540541f, 0.0162162162f,
        };
    };

    class CombineVectorsAndGlowPass : ShaderPass {
    public:
        void Init() {
            m_shader.LoadShaders(ShaderSource::Passthrough_vert,
                                 ShaderSource::CombineVectorsAndGlow_frag);
        }

        void Draw(const Texture& inputVectorsTexture, const Texture& inputGlowTexture,
                  const Texture& outputTexture) {
            SetFrameBufferTexture(*g_textureFB, outputTexture.Id());
            SetViewportToTextureDims(outputTexture);
            glClear(GL_COLOR_BUFFER_BIT);

            m_shader.Bind();

            SetTextureUniform(m_shader.Id(), "vectorsTexture", inputVectorsTexture.Id(), 0);
            SetTextureUniform(m_shader.Id(), "glowTexture", inputGlowTexture.Id(), 1);

            DrawFullScreenQuad();
        }
    };

    class GameScreenToCrtTexturePass : ShaderPass {
    public:
        void Init() {
            m_shader.LoadShaders(ShaderSource::Passthrough_vert, ShaderSource::DrawTexture_frag);
        }

        void Draw(const Texture& inputTexture, const Texture& outputTexture) {
            SetFrameBufferTexture(*g_textureFB, outputTexture.Id());
            SetViewportToTextureDims(outputTexture);
            glClear(GL_COLOR_BUFFER_BIT);

            m_shader.Bind();

            SetTextureUniform(m_shader.Id(), "vectorsTexture", inputTexture.Id(), 0);

            DrawFullScreenQuad(CrtScaleX, CrtScaleY);
        }
    };

    class RenderToScreenPass : ShaderPass {
    public:
        void Init() {
            m_shader.LoadShaders(ShaderSource::Passthrough_vert, ShaderSource::DrawScreen_frag);
        }

        void Draw(const Texture& inputCrtTexture, const Texture& inputOverlayTexture) {
            ImGui::SliderFloat("OverlayAlpha", &OverlayAlpha, 0.0f, 1.0f);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            SetViewport(g_windowViewport);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            m_shader.Bind();

            SetTextureUniform(m_shader.Id(), "crtTexture", inputCrtTexture.Id(), 0);
            SetTextureUniform(m_shader.Id(), "overlayTexture", inputOverlayTexture.Id(), 1);
            SetUniform(m_shader.Id(), "overlayAlpha", OverlayAlpha);

            DrawFullScreenQuad();
        }
    };

    // Global shader pass instances
    DrawVectorsPass g_drawVectorsPass;
    DarkenTexturePass g_darkenTexturePass;
    GlowPass g_glowPass;
    CombineVectorsAndGlowPass g_combineVectorsAndGlowPass;
    GameScreenToCrtTexturePass g_gameScreenToCrtTexturePass;
    RenderToScreenPass g_renderToScreenPass;

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
        g_drawVectorsPass.Init();
        g_darkenTexturePass.Init();
        g_glowPass.Init();
        g_combineVectorsAndGlowPass.Init();
        g_gameScreenToCrtTexturePass.Init();
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

    std::tuple<int, int> GetMajorMinorVersion() { return {3, 3}; }

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

        glObjectLabel(GL_TEXTURE, g_overlayTexture.Id(), -1, "g_overlayTexture");
    }

    bool OnWindowResized(int windowWidth, int windowHeight) {
        if (windowHeight == 0) {
            windowHeight = 1;
        }
        g_windowWidth = windowWidth;
        g_windowHeight = windowHeight;

        g_windowViewport = GetBestFitViewport(OverlayAR, windowWidth, windowHeight);

        auto[overlayWidth, overlayHeight] = std::make_tuple(g_windowViewport.w, g_windowViewport.h);

        auto[crtWidth, crtHeight] =
            std::make_tuple(static_cast<GLsizei>(overlayWidth * CrtScaleX),
                            static_cast<GLsizei>(overlayHeight * CrtScaleY));

        double halfWidth = crtWidth / 2.0;
        double halfHeight = crtHeight / 2.0;
        g_projectionMatrix = glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight);

        g_modelViewMatrix = {};

        // (Re)create resources that depend on viewport size
        g_vectorsTexture0.Allocate(crtWidth, crtHeight, GL_RGB32F);
        g_vectorsTexture1.Allocate(crtWidth, crtHeight, GL_RGB32F);
        g_vectorsThickTexture0.Allocate(crtWidth, crtHeight, GL_RGB32F);
        g_vectorsThickTexture1.Allocate(crtWidth, crtHeight, GL_RGB32F);
        g_tempTexture.Allocate(crtWidth, crtHeight, GL_RGB32F);
        glObjectLabel(GL_TEXTURE, g_tempTexture.Id(), -1, "g_tempTexture");
        g_glowTexture.Allocate(crtWidth, crtHeight, GL_RGB32F);
        glObjectLabel(GL_TEXTURE, g_glowTexture.Id(), -1, "g_glowTexture");
        g_crtTexture.Allocate(overlayWidth, overlayHeight, GL_RGB);
        glObjectLabel(GL_TEXTURE, g_crtTexture.Id(), -1, "g_crtTexture");

        // Clear g_vectorsTexture0 once
        SetFrameBufferTexture(*g_textureFB, g_vectorsTexture0.Id());
        SetViewportToTextureDims(g_vectorsTexture0);
        glClear(GL_COLOR_BUFFER_BIT);

        return true;
    }

    void RenderScene(double frameTime, const RenderContext& renderContext) {
        // Force resize on crt scale change
        {
            static float scaleX = CrtScaleX;
            static float scaleY = CrtScaleY;
            ImGui::SliderFloat("scaleX", &scaleX, 0.0f, 1.0f);
            ImGui::SliderFloat("scaleY", &scaleY, 0.0f, 1.0f);

            if (scaleX != CrtScaleX || scaleY != CrtScaleY) {
                CrtScaleX = scaleX;
                CrtScaleY = scaleY;
                OnWindowResized(g_windowWidth, g_windowHeight);
            }
        }

        if (frameTime > 0)
            g_vectorsTexture0Index = (g_vectorsTexture0Index + 1) % 2;

        auto& currVectorsTexture0 =
            g_vectorsTexture0Index == 0 ? g_vectorsTexture0 : g_vectorsTexture1;
        auto& currVectorsTexture1 =
            g_vectorsTexture0Index == 0 ? g_vectorsTexture1 : g_vectorsTexture0;

        auto& currVectorsThickTexture0 =
            g_vectorsTexture0Index == 0 ? g_vectorsThickTexture0 : g_vectorsThickTexture1;
        auto& currVectorsThickTexture1 =
            g_vectorsTexture0Index == 0 ? g_vectorsThickTexture1 : g_vectorsThickTexture0;

        glObjectLabel(GL_TEXTURE, currVectorsTexture0.Id(), -1, "currVectorsTexture0");
        glObjectLabel(GL_TEXTURE, currVectorsTexture1.Id(), -1, "currVectorsTexture1");

        glObjectLabel(GL_TEXTURE, currVectorsThickTexture0.Id(), -1, "currVectorsThickTexture0");
        glObjectLabel(GL_TEXTURE, currVectorsThickTexture1.Id(), -1, "currVectorsThickTexture1");

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

            // Scale game screen (lines) to CRT texture
            g_gameScreenToCrtTexturePass.Draw(g_tempTexture, g_crtTexture);
        } else {
            // Scale game screen (lines) to CRT texture
            g_gameScreenToCrtTexturePass.Draw(currVectorsTexture0, g_crtTexture);
        }

        // Present
        g_renderToScreenPass.Draw(g_crtTexture, g_overlayTexture);
    }

} // namespace GLRender
