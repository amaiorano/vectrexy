#include "GLUtil.h"
#include "core/ConsoleOutput.h"
#include "core/ImageUtil.h"
#include <fstream>
#include <sstream>
#include <vector>

namespace {
    std::function<GLUtil::GLDebugMessageCallbackFunc> g_debugMessageCallback;
    GLuint g_frameBufferId = ~0u;

    void GLAPIENTRY GLDebugMessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                                           GLsizei length, const GLchar* message,
                                           const GLvoid* userParam) {

        assert(g_debugMessageCallback != nullptr);
        g_debugMessageCallback({source, type, id, severity, length, message, userParam});
    }

    std::optional<std::string> FileToString(const char* file) {
        std::ifstream is(file);
        if (!is)
            return {};
        std::stringstream buffer;
        buffer << is.rdbuf();
        return buffer.str();
    }
} // namespace

void GLUtil::SetDebugMessageCallback(std::function<void(const GLDebugMessageInfo&)> callback) {
    g_debugMessageCallback = std::move(callback);
    if (g_debugMessageCallback) {
        if (glDebugMessageCallback) {
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(&GLDebugMessageCallback, nullptr);

            GLenum disableSources[] = {
                // GL_DEBUG_SOURCE_API,
                // GL_DEBUG_SOURCE_WINDOW_SYSTEM,
                GL_DEBUG_SOURCE_SHADER_COMPILER,
                // GL_DEBUG_SOURCE_THIRD_PARTY,
                // GL_DEBUG_SOURCE_APPLICATION,
                // GL_DEBUG_SOURCE_OTHER
            };

            GLenum disableTypes[] = {
                // GL_DEBUG_TYPE_ERROR,
                // GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR,
                // GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR,
                // GL_DEBUG_TYPE_PORTABILITY,
                // GL_DEBUG_TYPE_PERFORMANCE,
                GL_DEBUG_TYPE_PUSH_GROUP, GL_DEBUG_TYPE_POP_GROUP,
                // GL_DEBUG_TYPE_MARKER,
                // GL_DEBUG_TYPE_OTHER
            };

            // Enable all by default
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);

            // Disable specific disableSources
            for (auto source : disableSources) {
                glDebugMessageControl(source, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_FALSE);
            }

            // Disable specific types
            for (auto type : disableTypes) {
                glDebugMessageControl(GL_DONT_CARE, type, GL_DONT_CARE, 0, nullptr, GL_FALSE);
            }
        } else {
            Errorf("GL Debug not supported.\n");
        }
    } else {
        glDisable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDisable(GL_DEBUG_OUTPUT);
    }
}

void GLUtil::BindFrameBuffer(GLuint frameBufferId) {
    if (g_frameBufferId == frameBufferId)
        return;
    glBindFramebuffer(GL_FRAMEBUFFER, frameBufferId);
    g_frameBufferId = frameBufferId;
}

void GLUtil::AllocateTexture(GLuint textureId, GLsizei width, GLsizei height, GLint internalFormat,
                             std::optional<GLint> filtering, std::optional<PixelData> pixelData) {

    auto pd = pixelData.value_or(GLUtil::PixelData{nullptr, GL_RGBA, GL_UNSIGNED_BYTE});

    glBindTexture(GL_TEXTURE_2D, textureId);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, pd.format, pd.type, pd.pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filtering.value_or(DEFAULT_FILTERING));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filtering.value_or(DEFAULT_FILTERING));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

bool GLUtil::LoadPngTexture(GLuint textureId, const char* file, std::optional<GLint> filtering) {
    auto imgData = ImageUtil::loadPngImage(file);

    if (!imgData) {
        return false;
    }

    AllocateTexture(
        textureId, imgData->width, imgData->height, imgData->hasAlpha ? GL_RGBA : GL_RGB, filtering,
        PixelData{imgData->data.get(), static_cast<GLenum>(imgData->hasAlpha ? GL_RGBA : GL_RGB),
                  GL_UNSIGNED_BYTE});
    return true;
}

GLuint GLUtil::LoadShadersFromFiles(const char* vertShaderFile, const char* fragShaderFile) {
    auto vertShaderCode = FileToString(vertShaderFile);
    if (!vertShaderCode) {
        Errorf("Failed to open %s\n", vertShaderFile);
        return 0;
    }

    auto fragShaderCode = FileToString(fragShaderFile);
    if (!fragShaderCode) {
        Errorf("Failed to open %s\n", fragShaderFile);
        return 0;
    }

    return LoadShaders(vertShaderCode->c_str(), fragShaderCode->c_str());
}

GLuint GLUtil::LoadShaders(const char* vertShaderCode, const char* fragShaderCode) {
    auto CheckCompileStatus = [](GLuint id, GLenum pname) {
        assert(pname == GL_COMPILE_STATUS);
        GLint result = GL_FALSE;
        GLint infoLogLength{};
        glGetShaderiv(id, pname, &result);
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &infoLogLength);
        if (infoLogLength > 0) {
            std::vector<char> info(infoLogLength + 1);
            if (pname == GL_COMPILE_STATUS)
                glGetShaderInfoLog(id, infoLogLength, nullptr, info.data());
            else
                glGetProgramInfoLog(id, infoLogLength, nullptr, info.data());
            Errorf("%s", info.data());
        }
        return true;
    };

    auto CompileShader = [CheckCompileStatus](GLuint shaderId, const char* shaderCode) {
        // Printf("Compiling shader : %s\n", shaderFile);
        auto sourcePtr = shaderCode;
        glShaderSource(shaderId, 1, &sourcePtr, nullptr);
        glCompileShader(shaderId);
        return CheckCompileStatus(shaderId, GL_COMPILE_STATUS);
    };

    auto LinkProgram = [](GLuint programId) {
        // Printf("Linking program\n");
        glLinkProgram(programId);
        return true; //@TODO: check if valid
    };

    GLuint vertShaderId = glCreateShader(GL_VERTEX_SHADER);
    if (!CompileShader(vertShaderId, vertShaderCode))
        return 0;

    GLuint fragShaderId = glCreateShader(GL_FRAGMENT_SHADER);
    if (!CompileShader(fragShaderId, fragShaderCode))
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
