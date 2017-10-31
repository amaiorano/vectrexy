#include "GLRender.h"
#include "EngineClient.h"

//#include <gl/GL.h>
#include <SDL_opengl.h> // Wraps OpenGL headers
#include <gl/GLU.h>

namespace GLRender {

    void Initialize() {
        glShadeModel(GL_SMOOTH);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClearDepth(1.0f);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    }

    void Shutdown() {
        //
    }

    std::tuple<int, int> GetMajorMinorVersion() { return {2, 1}; }

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
