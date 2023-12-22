#pragma once

#include <GL/glew.h> // Must be included before gl.h

#include <SDL_opengl.h> // Wraps OpenGL headers - TODO: ifndef SDL: include gl.h

#include <array>
#include <cassert>
#include <cstdio>
#include <functional>
#include <optional>

namespace GLUtil {

    ////////////////////////////////
    // GL Errors
    ////////////////////////////////

    inline void ResetGLError() {
        while (glGetError() != GL_NO_ERROR) {
        }
    }

    inline void CheckGLError() {
        auto error = glGetError();
        if (error != GL_NO_ERROR) {
            auto errorString = glewGetErrorString(error);
            printf("GL Error: %s\n", errorString);
            assert(false); //@TODO: do better than this
        }
    }

    struct ScopedCheckGLErrors {
        ScopedCheckGLErrors() { ResetGLError(); }
        ~ScopedCheckGLErrors() { CheckGLError(); }
    };

    struct GLDebugMessageInfo {
        GLenum source;
        GLenum type;
        GLuint id;
        GLenum severity;
        GLsizei length;
        const GLchar* message;
        const GLvoid* userParam;
    };
    using GLDebugMessageCallbackFunc = void(const GLDebugMessageInfo&);

    void SetDebugMessageCallback(std::function<GLDebugMessageCallbackFunc> callback);

    ////////////////////////////////
    // Debug
    ////////////////////////////////

    inline void PushDebugGroup(const char* name) {
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name);
    }

    inline void PopDebugGroup() { glPopDebugGroup(); }

    struct ScopedDebugGroup {
        ScopedDebugGroup(const char* name) { PushDebugGroup(name); }
        ~ScopedDebugGroup() { PopDebugGroup(); }
    };

    ////////////////////////////////
    // Resources
    ////////////////////////////////

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
        GLResource(GLResource&& rhs) noexcept
            : m_id(rhs.m_id)
            , m_deleteFunc(rhs.m_deleteFunc) {
            rhs.m_id = INVALID_RESOURCE_ID;
        }
        GLResource& operator=(GLResource&& rhs) noexcept {
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

    inline auto MakeTextureResource() {
        struct Deleter {
            auto operator()(GLuint id) { glDeleteTextures(1, &id); }
        };
        GLuint id;
        glGenTextures(1, &id);
        return GLResource<decltype(Deleter{})>{id};
    }
    using TextureResource = decltype(MakeTextureResource());

    inline auto MakeVertexArrayResource() {
        struct Deleter {
            auto operator()(GLuint id) { glDeleteVertexArrays(1, &id); }
        };
        GLuint id;
        glGenVertexArrays(1, &id);
        return GLResource<decltype(Deleter{})>{id};
    }
    using VertexArrayResource = decltype(MakeVertexArrayResource());

    inline auto MakeFrameBufferResource() {
        struct Deleter {
            auto operator()(GLuint id) { glDeleteFramebuffers(1, &id); }
        };
        GLuint id;
        glGenFramebuffers(1, &id);
        return GLResource<decltype(Deleter{})>{id};
    }
    using FrameBufferResource = decltype(MakeFrameBufferResource());

    inline auto MakeBufferResource() {
        struct Deleter {
            auto operator()(GLuint id) { glDeleteBuffers(1, &id); }
        };
        GLuint id;
        glGenBuffers(1, &id);
        return GLResource<decltype(Deleter{})>{id};
    }
    using BufferResource = decltype(MakeBufferResource());

    ////////////////////////////////
    // Frame Buffers
    ////////////////////////////////

    void BindFrameBuffer(GLuint frameBufferId);

    inline void SetFrameBufferTexture(GLuint frameBufferId, GLuint textureId) {
        BindFrameBuffer(frameBufferId);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textureId, 0);
    }

    ////////////////////////////////
    // Vertex Buffers
    ////////////////////////////////

    template <typename T, size_t N>
    void SetVertexBufferData(T (&vertices)[N]) {
        glBufferData(GL_ARRAY_BUFFER, N * sizeof(vertices[0]), &vertices[0], GL_DYNAMIC_DRAW);
    }

    template <typename T, size_t N>
    void SetVertexBufferData(const std::array<T, N>& vertices) {
        glBufferData(GL_ARRAY_BUFFER, N * sizeof(vertices[0]), &vertices[0], GL_DYNAMIC_DRAW);
    }

    inline void CheckFramebufferStatus() {
        assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE);
    }

    ////////////////////////////////
    // Textures
    ////////////////////////////////

    struct PixelData {
        uint8_t* pixels{};
        GLenum format{};
        GLenum type{};
    };

    const GLint DEFAULT_FILTERING = GL_NEAREST;

    void AllocateTexture(GLuint textureId, GLsizei width, GLsizei height, GLint internalFormat,
                         std::optional<GLint> filtering = {},
                         std::optional<PixelData> pixelData = {});

    bool LoadPngTexture(GLuint textureId, const char* file, std::optional<GLint> filtering = {});

    class Texture {
    public:
        void Allocate(GLsizei width, GLsizei height, GLint internalFormat,
                      std::optional<GLint> filtering = {},
                      std::optional<PixelData> pixelData = {}) {
            m_resource = MakeTextureResource();
            AllocateTexture(*m_resource, width, height, internalFormat, filtering, pixelData);
            m_width = width;
            m_height = height;
        }

        bool LoadPng(const char* file, std::optional<GLint> filtering = {}) {
            TextureResource resource = MakeTextureResource();
            if (LoadPngTexture(*resource, file, filtering)) {
                m_resource = std::move(resource);
                return true;
            }
            return false;
        }

        void SetName(const char* name) { glObjectLabel(GL_TEXTURE, Id(), -1, name); }

        GLuint Id() const { return *m_resource; }
        GLsizei Width() const { return m_width; }
        GLsizei Height() const { return m_height; }

    private:
        TextureResource m_resource;
        GLsizei m_width{}, m_height{};
    };

    ////////////////////////////////
    // Shaders
    ////////////////////////////////

    GLuint LoadShadersFromFiles(const char* vertShaderFile, const char* fragShaderFile);
    GLuint LoadShaders(const char* vertShaderCode, const char* fragShaderCode);

    inline void SetUniform(GLuint shader, const char* name, GLint value) {
        GLuint id = glGetUniformLocation(shader, name);
        glUniform1i(id, value);
    }
    inline void SetUniform(GLuint shader, const char* name, GLfloat value) {
        GLuint id = glGetUniformLocation(shader, name);
        glUniform1f(id, value);
    }
    inline void SetUniform(GLuint shader, const char* name, GLfloat value1, GLfloat value2) {
        GLuint id = glGetUniformLocation(shader, name);
        glUniform2f(id, value1, value2);
    }
    inline void SetUniform(GLuint shader, const char* name, const GLfloat* values, GLsizei size) {
        GLuint id = glGetUniformLocation(shader, name);
        glUniform1fv(id, size, values);
    }
    inline void SetUniformMatrix4v(GLuint shader, const char* name, const GLfloat* matrix) {
        GLuint id = glGetUniformLocation(shader, name);
        glUniformMatrix4fv(id, 1, GL_FALSE, matrix);
    }
    inline void SetTextureUniform(GLuint shader, const char* name, GLuint textureId,
                                  GLint textureSlot) {
        glActiveTexture(GL_TEXTURE0 + textureSlot);
        glBindTexture(GL_TEXTURE_2D, textureId);
        GLuint texLoc = glGetUniformLocation(shader, name);
        glUniform1i(texLoc, textureSlot);
    }

    class Shader {
    public:
        ~Shader() {
            glUseProgram(0);
            glDeleteProgram(m_programId);
        }

        void LoadShaders(const char* vertShaderFile, const char* fragShaderFile) {
            m_programId = GLUtil::LoadShaders(vertShaderFile, fragShaderFile);
        }

        void SetName(const char* name) {
            assert(m_programId != GLuint{}); // Call this after LoadShaders
            glObjectLabel(GL_PROGRAM, m_programId, -1, name);
        }

        void Bind() { glUseProgram(m_programId); }

        GLuint Id() const { return m_programId; }

    private:
        GLuint m_programId{};
    };
} // namespace GLUtil
