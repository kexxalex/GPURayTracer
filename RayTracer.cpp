#include <iostream>
#include <fstream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "Scene.hpp"
#include <glm/gtx/transform.hpp>

static constexpr glm::dvec3 rot_x(1.0f, 0.0f, 0.0f);
static constexpr glm::dvec3 rot_y(0.0f, 1.0f, 0.0f);
static int WIDTH(2048), HEIGHT(2048);

//static glm::dvec2 MVP_rot(-0.7,-2.47);
static glm::dvec2 MVP_rot(-0.55,-2.88);
// static glm::dvec3 MVP_translation(-2.35978,3.87126,4.10415);
static glm::dvec3 MVP_translation(-2.1225,3.41671,3.85816);



void finalRender(GLFWwindow *window, Scene &scene, int width, int height) {
    glm::dmat4 ROT = glm::rotate(-MVP_rot.y, rot_y) * glm::rotate(-MVP_rot.x, rot_x);
    glm::fmat4 MVP = glm::translate(MVP_translation) * ROT;
    int sample = 0;
    while (true) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        double t0 = glfwGetTime();
        scene.render(width, height, false, MVP, ++sample);
        glfwSwapBuffers(window);
        std::cout << glfwGetTime() - t0 << '\n';
        glfwPollEvents();
        glfwSetWindowTitle(window, ("GPU RT - Samples: " + std::to_string(sample+1)).c_str());
        if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS)
            break;
    }
    std::cout << "Samples: " << sample+1 << '\n';
    scene.exportRAW("./res/final/final.bytes");
}



void mainLoop(GLFWwindow *window, Scene &scene) {
    glm::dmat4 ROT = glm::rotate(-MVP_rot.y, rot_y) * glm::rotate(-MVP_rot.x, rot_x);

    glm::dvec2 mouse, lastMouse;
    bool lastMoving = true;
    glfwSwapInterval(1);

    double lastUpdate = glfwGetTime();

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.8, 0.86, 0.9, 1.0);
    static unsigned int sample = 0;

    while (!glfwWindowShouldClose(window)) {
        double time = glfwGetTime();
        double deltaT = time - lastUpdate;
        lastUpdate = time;

        bool moving = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE);
        bool shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT);
        bool ctrl = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL);

        glfwGetCursorPos(window, &mouse.y, &mouse.x);
        glm::dvec2 delta = lastMouse - mouse;
        lastMouse = mouse;

        if (moving && glm::dot(delta, delta) > 0) {
            MVP_rot += delta * 0.01;
        }
        if (glfwGetKey(window, GLFW_KEY_W)) {
            MVP_translation += glm::dvec3(ROT*glm::dvec4(0, 0, deltaT * (shift ? 8.0 : 1.0) * (ctrl ? 0.125 : 1.0), 0));
            moving = true;
        }
        if (glfwGetKey(window, GLFW_KEY_S)) {
            MVP_translation += glm::dvec3(ROT*glm::dvec4(0,0,-deltaT * (shift ? 8.0 : 1.0) * (ctrl ? 0.125 : 1.0), 0));
            moving = true;
        }
        if (glfwGetKey(window, GLFW_KEY_A)) {
            MVP_translation += glm::dvec3(ROT*glm::dvec4(-deltaT * (shift ? 8.0 : 1.0) * (ctrl ? 0.125 : 1.0), 0, 0, 0));
            moving = true;
        }
        if (glfwGetKey(window, GLFW_KEY_D)) {
            MVP_translation += glm::dvec3(ROT*glm::dvec4(deltaT * (shift ? 8.0 : 1.0) * (ctrl ? 0.125 : 1.0), 0, 0, 0));
            moving = true;
        }
        if (glfwGetKey(window, GLFW_KEY_SPACE)) {
            MVP_translation += glm::dvec3(ROT*glm::dvec4(0, deltaT * (shift ? 8.0 : 1.0) * (ctrl ? 0.125 : 1.0), 0, 0));
            moving = true;
        }
        if (glfwGetKey(window, GLFW_KEY_C)) {
            MVP_translation += glm::dvec3(ROT*glm::dvec4(0, -deltaT * (shift ? 8.0 : 1.0) * (ctrl ? 0.125 : 1.0), 0, 0));
            moving = true;
        }

        if (moving || lastMoving != moving)
        {
            // std::cout << MVP_translation.x << ',' << MVP_translation.y << ',' << MVP_translation.z << '\n';
            // std::cout << MVP_rot.x << ',' << MVP_rot.y << '\n';
            ROT = glm::rotate(-MVP_rot.y, rot_y) * glm::rotate(-MVP_rot.x, rot_x);
            glm::fmat4 CameraTransform = glm::translate(MVP_translation) * ROT;
            glm::fmat4 MVP = glm::perspectiveFov(glm::radians(90.0), (double)WIDTH, (double)HEIGHT, 0.03, 1024.0) * glm::rotate(-MVP_rot.x, rot_x) * glm::rotate(-MVP_rot.y, rot_y) * glm::translate(glm::dvec3(-1,-1,1)*MVP_translation);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            if (!moving) {
                glfwSetWindowTitle(window, ("GPU RT - Samples: " + std::to_string(sample+1)).c_str());
                scene.render(WIDTH, HEIGHT, moving, CameraTransform, sample++);
            }
            else {
                sample = 0;
                glfwSetWindowTitle(window, "GPU RT - OpenGL Phong");
                scene.forwardRender(MVP, MVP_translation);
            }
            glfwSwapBuffers(window);
        }
        lastMoving = moving;

        if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS) {
            finalRender(window, scene, WIDTH, HEIGHT);
        }
        glfwPollEvents();
    }
}


int main() {
    std::string name("./res/ObiWan");

    /*
    std::cout << "Wavefront File: ";
    std::cin >> name;

    std::cout << "Width: ";
    std::cin >> width;

    std::cout << "Height: ";
    std::cin >> height;
     */

    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    glm::ivec2 display(720.0f*(float)WIDTH/(float)HEIGHT, 720);
    GLFWwindow *window = glfwCreateWindow(display.x, display.y, "GPU RT", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glViewport(0, 0, display.x, display.y);

    glewInit();

    Scene scene;
    std::cout << (scene.addWavefrontModel(name) ? "Scene Loaded" : "Scene does not exist") << '\n';

    scene.finalizeObjects();
    mainLoop(window, scene);

    glfwTerminate();

    return 0;
}
