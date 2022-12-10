#include <iostream>
#include <string>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

#include "renderer.h"

#include "input_manager.h"
#include "camera.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

struct Model {
    glm::vec3 translate;
    glm::vec3 rotate;
    glm::vec3 scale;

    glm::mat4 model_mat()
    {
        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 rot = glm::eulerAngleZYX(rotate.z, rotate.y, rotate.x);
        model = glm::translate(model, translate);
        model *= rot;
        model = glm::scale(model, scale);

        return model;
    }
};

class Application
{
public:
    Application() : quit(false), delta_time(0.0f)
    {
        triangle.translate = glm::vec3(0.0f, 0.0f, 0.0f);
        triangle.scale = glm::vec3(1.0f, 1.0f, 1.0f);
        triangle.rotate = glm::vec3(0.0f, 0.0f, glm::half_pi<float>());
    }

    int run(Renderer* renderer)
    {
        renderer->init();

        while (!quit) {
            delta_time = renderer->frame_start();

            input_mgr.update();
            process_input();

            {
                glm::mat4 p = glm::perspective(camera.zoom(), (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT, 0.1f, 1000.0f);
                glm::mat4 v = camera.look_at();
                glm::mat4 m = triangle.model_mat();
                ubo_data.mvp = p * v * m;
                renderer->update_uniform(&ubo_data);

            }
            renderer->draw();
        }

        renderer->cleanup();

        return 0;
    }

private:
    void process_input()
    {
        if (input_mgr.quit_requested() || input_mgr.is_pressed(KEY_QUIT)) {
            quit = true;
        }

        if (input_mgr.is_held(KEY_UP)) {
            camera.process_keyboard(CameraDirection::FORWARD, delta_time);
        }

        if (input_mgr.is_held(KEY_DOWN)) {
            camera.process_keyboard(CameraDirection::BACKWARD, delta_time);
        }

        if (input_mgr.is_held(KEY_LEFT)) {
            triangle.rotate.z += 0.05;
        }
        else if (input_mgr.is_held(KEY_RIGHT)) {
            triangle.rotate.z -= 0.05;
        }
    }

    InputManager &input_mgr = InputManager::instance();

    bool quit;
    float delta_time;

    Camera camera;
    Model triangle;
    UBO_VS ubo_data;
};

int main(int argc, char** argv)
{
    Renderer renderer(WINDOW_WIDTH, WINDOW_HEIGHT, "Hello Metal");
    Application app;

    return app.run(&renderer);
}
