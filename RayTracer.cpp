#include <iostream>
#include <fstream>
#include "Scene.hpp"

#ifdef __linux__

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/gtx/transform.hpp>

#elif _WIN32

constexpr double M_PI = 3.141592653589793;
#include "GL/glew.h"
#include "GLFW/glfw3.h"
#include "glm/gtx/transform.hpp"

#endif

static constexpr glm::dvec3 rot_x(1.0f, 0.0f, 0.0f);
static constexpr glm::dvec3 rot_y(0.0f, 1.0f, 0.0f);
static int WIDTH(3440), HEIGHT(1440);

static glm::dvec2 MVP_rot(0, M_PI);
static glm::dvec3 MVP_translation(0.0, 1.0, 4.0);

static glm::fvec3 LIGHT_DIR = glm::normalize(glm::fvec3(0.0, -1.0, 2.0)) * 0.0f;
static glm::fvec3 AMBIENT = glm::fvec3(0.9, 0.96, 1.0) * 0.5f;



void finalRender(GLFWwindow *window, Scene &scene, int width, int height, unsigned int &sample) {
    glm::dmat4 ROT = glm::rotate(-MVP_rot.y, rot_y) * glm::rotate(-MVP_rot.x, rot_x);
    glm::fmat4 CAMERA = glm::translate(MVP_translation) * ROT;
    glm::fmat4 MVP = glm::perspectiveFov(glm::radians(90.0), (double)WIDTH, (double)HEIGHT, 0.03, 1024.0) * glm::rotate(-MVP_rot.x, rot_x) * glm::rotate(-MVP_rot.y, rot_y) * glm::translate(glm::dvec3(-1,-1,1)*MVP_translation);
    const auto t0 = glfwGetTime();
    double lastSwap = t0;
    double t;
    while (sample < 127) {
        scene.render(width, height, false, CAMERA, ++sample);
    }
    glFinish();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    scene.display(sample);
    glfwSwapBuffers(window);
    glfwPollEvents();
    double sps = (sample+1) / (glfwGetTime() - t0);// * WIDTH * HEIGHT;
    std::cout << sps << std::endl;
    glfwSwapInterval(1);
    glfwSetWindowTitle(window, ("GPU RT - Samples: " + std::to_string(sample+1) + " - Finished").c_str());
    scene.exportRAW("./res/final/final.bytes");
}


void mainLoop(GLFWwindow *window, Scene &scene) {
    static glm::dmat4 P = glm::perspectiveFov(glm::radians(90.0), (double)WIDTH, (double)HEIGHT, 0.03, 1024.0);
    static glm::dmat4 ROT = glm::rotate(-MVP_rot.y, rot_y) * glm::rotate(-MVP_rot.x, rot_x);

    glm::dvec2 mouse, lastMouse;
    bool lastMoving = true;
    glfwSwapInterval(1);

    double lastUpdate = glfwGetTime();

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glFrontFace(GL_CW);
    glCullFace(GL_BACK);
    glClearColor(AMBIENT.r, AMBIENT.g, AMBIENT.b, 1.0);
    static unsigned int sample = 0;

    while (!glfwWindowShouldClose(window)) {
        const double time = glfwGetTime();
        const double deltaT = time - lastUpdate;
        lastUpdate = time;

        const bool rightBtn = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);
        const bool middleBtn = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE);
        bool moving = (rightBtn || middleBtn);
        const bool shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT);
        const bool ctrl = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL);

        glfwGetCursorPos(window, &mouse.y, &mouse.x);
        glm::dvec2 delta = lastMouse - mouse;
        lastMouse = mouse;

        if (moving && glm::dot(delta, delta) > 0) {
            MVP_rot += delta * 0.01;
            sample = 0;
        }
        if (glfwGetKey(window, GLFW_KEY_W)) {
            MVP_translation += glm::dvec3(ROT*glm::dvec4(0, 0, deltaT * (shift ? 8.0 : 1.0) * (ctrl ? 0.125 : 1.0), 0));
            moving = true;
            sample = 0;
        }
        if (glfwGetKey(window, GLFW_KEY_S)) {
            MVP_translation += glm::dvec3(ROT*glm::dvec4(0,0,-deltaT * (shift ? 8.0 : 1.0) * (ctrl ? 0.125 : 1.0), 0));
            moving = true;
            sample = 0;
        }
        if (glfwGetKey(window, GLFW_KEY_A)) {
            MVP_translation += glm::dvec3(ROT*glm::dvec4(-deltaT * (shift ? 8.0 : 1.0) * (ctrl ? 0.125 : 1.0), 0, 0, 0));
            moving = true;
            sample = 0;
        }
        if (glfwGetKey(window, GLFW_KEY_D)) {
            MVP_translation += glm::dvec3(ROT*glm::dvec4(deltaT * (shift ? 8.0 : 1.0) * (ctrl ? 0.125 : 1.0), 0, 0, 0));
            moving = true;
            sample = 0;
        }
        if (glfwGetKey(window, GLFW_KEY_SPACE)) {
            MVP_translation += glm::dvec3(ROT*glm::dvec4(0, deltaT * (shift ? 8.0 : 1.0) * (ctrl ? 0.125 : 1.0), 0, 0));
            moving = true;
            sample = 0;
        }
        if (glfwGetKey(window, GLFW_KEY_C)) {
            MVP_translation += glm::dvec3(ROT*glm::dvec4(0, -deltaT * (shift ? 8.0 : 1.0) * (ctrl ? 0.125 : 1.0), 0, 0));
            moving = true;
            sample = 0;
        }

        if (moving || lastMoving != moving)
        {
            if (lastMoving != moving)
                sample = 0;
            ROT = glm::rotate(-MVP_rot.y, rot_y) * glm::rotate(-MVP_rot.x, rot_x);
            glm::fmat4 CameraTransform = glm::translate(MVP_translation) * ROT;
            glm::fmat4 MVP = P * glm::rotate(-MVP_rot.x, rot_x) * glm::rotate(-MVP_rot.y, rot_y) * glm::translate(glm::dvec3(-1,-1,1)*MVP_translation);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            if (!moving) {
                sample = 0;
                glfwSetWindowTitle(window, ("GPU RT - Samples: " + std::to_string(sample+1)).c_str());
                scene.render(WIDTH, HEIGHT, false, CameraTransform, sample++);
            }
            else if (middleBtn) {
                glfwSetWindowTitle(window, ("GPU RT - Samples: " + std::to_string(sample+1)).c_str());
                scene.render(WIDTH, HEIGHT, true, CameraTransform, sample++);
            }
            else {
                glfwSetWindowTitle(window, "GPU RT - OpenGL Phong");
                scene.forwardRender(MVP, MVP_translation);
            }
            if (glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS) {
                scene.renderWireframe(MVP, MVP_translation);
            }
            scene.display(sample);
            glfwSwapBuffers(window);
        }
        lastMoving = moving;

        if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS) {
            finalRender(window, scene, WIDTH, HEIGHT, sample);
        }
        glfwPollEvents();
    }
}


int main(int argc, char* args[]) {
    std::string name;

    std::cout << "Wavefront File: ";
    std::cin >> name;

    std::cout << "Width: ";
    std::cin >> WIDTH;

    std::cout << "Height: ";
    std::cin >> HEIGHT;

    if (glfwInit() != GLFW_TRUE) {
        std::cerr << "Cannot initialize GLFW\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);

    glm::ivec2 display(720.0f*(float)WIDTH/(float)HEIGHT, 720);
    GLFWwindow *window = glfwCreateWindow(display.x, display.y, "GPU RT", nullptr, nullptr);
    glfwMakeContextCurrent(window);

    glViewport(0, 0, display.x, display.y);

    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "GLEW Init error:\n" << glewGetErrorString(err) << '\n';
        return 2;
    }

    Scene scene;
    std::cout << (scene.addWavefrontModel("./res/models/"+name) ? "Scene Loaded" : "Scene does not exist") << '\n';

    scene.finalizeObjects();
    scene.setAmbientLight(AMBIENT);
    scene.setDirectionLight(LIGHT_DIR);
    mainLoop(window, scene);

    glfwTerminate();

    return 0;
}
