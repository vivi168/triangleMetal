#pragma once

#include <string>

#include <SDL2/SDL.h>
#include <Metal/Metal.hpp>

#include <glm/glm.hpp>

struct UBO_VS {
    glm::mat4 mvp;
};

class Renderer
{
public:
    Renderer(unsigned int width, unsigned int height, std::string name);

    void init();
    void cleanup();
    void draw();
    float frame_start();

    void update_uniform(UBO_VS* data);

private:
    void create_window();
    void init_resources();
    void cleanup_resources();

    SDL_Window* sdl_window;
    SDL_MetalView metal_view;
    CA::MetalLayer* layer;

    MTL::Device* device;
    MTL::CommandQueue* command_queue;

    // Resources
    MTL::Buffer* vertex_buffer;
    MTL::Buffer* index_buffer;
    MTL::Buffer* uniform_buffer;

    MTL::Library* library;
    MTL::Function* vert_fun;
    MTL::Function* frag_fun;

    MTL::RenderPipelineState* pipeline_state;

    // Misc
    MTL::Viewport viewport;
    std::string title;

    Uint32 last_time;
    Uint32 current_time;
};
