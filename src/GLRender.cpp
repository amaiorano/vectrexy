#include "GLRender.h"
#include "EngineClient.h"

#include <gl/glew.h> // Must be included before gl.h

#include <SDL_opengl.h> // Wraps OpenGL headers
#include <SDL_opengl_glext.h>
#include <gl/GLU.h>

#include <fstream>
#include <string>
#include <vector>

namespace {
    GLuint LoadShaders(const char* vertShaderFile, const char* fragShaderFile) {
        // Create the shaders
        GLuint vertShaderId = glCreateShader(GL_VERTEX_SHADER);
        GLuint fragShaderId = glCreateShader(GL_FRAGMENT_SHADER);

        // Read the Vertex Shader code from the file
        std::string vertShaderCode;
        std::ifstream vertShaderStream(vertShaderFile, std::ios::in);
        if (vertShaderStream.is_open()) {
            std::string line = "";
            while (getline(vertShaderStream, line))
                vertShaderCode += "\n" + line;
            vertShaderStream.close();
        } else {
            printf("Impossible to open %s. Are you in the right directory ? Don't forget to read "
                   "the FAQ !\n",
                   vertShaderFile);
            getchar();
            return 0;
        }

        // Read the Fragment Shader code from the file
        std::string fragShaderCode;
        std::ifstream fragShaderStream(fragShaderFile, std::ios::in);
        if (fragShaderStream.is_open()) {
            std::string line = "";
            while (getline(fragShaderStream, line))
                fragShaderCode += "\n" + line;
            fragShaderStream.close();
        }

        GLint result = GL_FALSE;
        int infoLogLength;

        // Compile Vertex Shader
        printf("Compiling shader : %s\n", vertShaderFile);
        char const* vertSourcePointer = vertShaderCode.c_str();
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
        char const* fragSourcePointer = fragShaderCode.c_str();
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

    namespace ShaderProgram {
        GLuint renderToTexture{};
        GLuint darkenTexture{};
        GLuint textureToScreen{};
    } // namespace ShaderProgram

} // namespace

namespace GLRender {
    void Initialize() {
        glShadeModel(GL_SMOOTH);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClearDepth(1.0f);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

        // Initialize GLEW
        glewExperimental = true; // Needed for core profile
        if (glewInit() != GLEW_OK) {
            FAIL_MSG("Failed to initialize GLEW");
        }

        ShaderProgram::renderToTexture =
            LoadShaders("shaders/DrawVectors.vert", "shaders/DrawVectors.frag");

        ShaderProgram::darkenTexture =
            LoadShaders("shaders/PassThrough.vert", "shaders/DarkenTexture.frag");

        ShaderProgram::textureToScreen =
            LoadShaders("shaders/Passthrough.vert", "shaders/DrawTexture.frag");
    }

    void Shutdown() {
        //
    }

    std::tuple<int, int> GetMajorMinorVersion() { return {3, 3}; }

    bool SetViewport(int windowWidth, int windowHeight, int screenWidth, int screenHeight) {
        GLfloat ratio;

        if (windowHeight == 0) {
            windowHeight = 1;
        }

        ratio = GLfloat(windowWidth) / GLfloat(windowHeight);
        glViewport(0, 0, GLsizei(windowWidth), GLsizei(windowHeight));

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        double halfWidth = screenWidth / 2.0;
        double halfHeight = screenHeight / 2.0;
        gluOrtho2D(-halfWidth, halfWidth, -halfHeight, halfHeight);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        return true;
    }

    void PreRender() {
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    }

} // namespace GLRender

void Display::Clear() {
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
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
