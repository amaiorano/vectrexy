#include "GLUtils.h"
#include <fstream>
#include <optional>
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

GLuint GLUtils::LoadShaders(const char* vertShaderFile, const char* fragShaderFile) {
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
