#pragma once

#include <gl/glew.h> // Must be included before gl.h

#include <SDL_opengl.h> // Wraps OpenGL headers - TODO: ifndef SDL: include gl.h

#include <array>
#include <cassert>
#include <optional>

namespace GLUtil {
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
    // Vertex Buffers
    ////////////////////////////////

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

    void AllocateTexture(GLuint textureId, GLsizei width, GLsizei height, GLint internalFormat,
                         std::optional<PixelData> pixelData = {});

    bool LoadPngTexture(GLuint textureId, const char* file);

    ////////////////////////////////
    // Shaders
    ////////////////////////////////

    GLuint LoadShaders(const char* vertShaderFile, const char* fragShaderFile);

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

    constexpr GLenum TextureSlotToTextureEnum(GLint slot) {
        switch (slot) {
        case 0:
            return GL_TEXTURE0;
        case 1:
            return GL_TEXTURE1;
        case 2:
            return GL_TEXTURE2;
        case 3:
            return GL_TEXTURE3;
        default:
            assert(false);
            return 0;
        }
    }

    template <GLint textureSlot>
    void SetTextureUniform(GLuint shader, const char* name, GLuint textureId) {
        constexpr auto texEnum = TextureSlotToTextureEnum(textureSlot);
        glActiveTexture(texEnum);
        glBindTexture(GL_TEXTURE_2D, textureId);
        GLuint texLoc = glGetUniformLocation(shader, name);
        glUniform1i(texLoc, textureSlot);
    }

    class Shader {
    public:
        ~Shader() {
            glUseProgram(0);
            glDeleteProgram(m_shaderId);
        }

        void LoadShaders(const char* vertShaderFile, const char* fragShaderFile) {
            m_shaderId = GLUtil::LoadShaders(vertShaderFile, fragShaderFile);
        }

        void Bind() { glUseProgram(m_shaderId); }

        GLuint Id() const { return m_shaderId; }

    private:
        GLuint m_shaderId{};
    };
} // namespace GLUtil
