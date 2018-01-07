#include "GLUtil.h"
#include "ConsoleOutput.h"
#include "ImageFileUtils.h"
#include <fstream>
#include <sstream>
#include <vector>

namespace {
    std::optional<std::string> FileToString(const char* file) {
        std::ifstream is(file);
        if (!is)
            return {};
        std::stringstream buffer;
        buffer << is.rdbuf();
        return buffer.str();
    }
} // namespace

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
    auto imgData = ImageFileUtils::loadPngImage(file);

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
            Errorf("%s", info.data());
        }
        return true;
    };

    auto CompileShader = [CheckStatus](GLuint shaderId, const char* shaderCode) {
        // Printf("Compiling shader : %s\n", shaderFile);
        auto sourcePtr = shaderCode;
        glShaderSource(shaderId, 1, &sourcePtr, NULL);
        glCompileShader(shaderId);
        return CheckStatus(shaderId, GL_COMPILE_STATUS);
    };

    auto LinkProgram = [CheckStatus](GLuint programId) {
        // Printf("Linking program\n");
        glLinkProgram(programId);
        return CheckStatus(programId, GL_LINK_STATUS);
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
