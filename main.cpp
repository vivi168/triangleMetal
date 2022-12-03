#include <iostream>
#include <cstdlib>

#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#include <Metal/Metal.hpp>

#include <SDL2/SDL.h>
#include <SDL2/SDL_metal.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "input_manager.h"
#include "camera.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define TICKS_PER_FRAME 60

enum {
    X = 0, Y, Z, W
};

typedef float vec2[2];
typedef float vec3[3];
typedef float quat[4];

struct Model {
    glm::vec3 translate;
    glm::vec3 rotate;
    glm::vec3 scale;
};

struct Vertex {
    vec3 position; // attributes 0
    vec3 color;    // attributes 1
};

struct UBO_VS {
    glm::mat4 mvp;
};

void init_metal();
void init_resources();
void cleanup_metal();
void cleanup_resources();

// Global variables
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

MTL::CommandBuffer* command_buffer;
MTL::RenderPipelineState* pipeline_state;

// Other
InputManager &input_mgr = InputManager::instance();

float delta_time;
Uint32 last_time;
Uint32 current_time;
bool quit;

Camera camera;
Model mymodel;

Vertex vertex_data[3] = {
    {{  1.0f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }},
    {{ -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }},
    {{  0.0f,  1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }},
};

uint32_t index_data[3] = { 0, 1, 2 };

UBO_VS ubo_data;

// Implementation

glm::mat4 model_mat(Model* m)
{
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, m->translate);
    model = glm::scale(model, m->scale);

    return model;
}

void create_window()
{
    std::cout << "create window\n";

    const char* title = "Triangle Metal";
    const int width = WINDOW_WIDTH;
    const int height = WINDOW_HEIGHT;

    sdl_window = SDL_CreateWindow(title,
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            width, height,
            SDL_WINDOW_METAL | SDL_WINDOW_SHOWN);

    if (sdl_window == NULL) {
        std::cerr << "Error while creating SDL_Window\n";
        exit(EXIT_FAILURE);
    }

    // SDL_SetRelativeMouseMode(SDL_TRUE);
}

void init()
{
    std::cout << "init\n";

    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        std::cerr << "Unable to init SDL\n";
        exit(EXIT_FAILURE);
    }

    create_window();

    init_metal();
    init_resources();
}

void init_metal()
{
    std::cout << "init metal\n";

    metal_view = SDL_Metal_CreateView(sdl_window);
    layer = (CA::MetalLayer*)SDL_Metal_GetLayer(metal_view);

    // create device
    device = MTL::CreateSystemDefaultDevice();
    layer->setDevice(device);

    command_queue = device->newCommandQueue();
}

void init_resources()
{
    NS::Error* error;
    std::cout << "init resources\n";

    // vertex buffer
    vertex_buffer = device->newBuffer(sizeof(Vertex) * 3, MTL::CPUCacheModeDefaultCache);
    vertex_buffer->setLabel(NS::String::string("VBO", NS::ASCIIStringEncoding));
    memcpy(vertex_buffer->contents(), vertex_data, sizeof(Vertex) * 3);

    // index buffer
    index_buffer = device->newBuffer(sizeof(uint32_t) * 3, MTL::CPUCacheModeDefaultCache);
    index_buffer->setLabel(NS::String::string("IBO", NS::ASCIIStringEncoding));
    memcpy(index_buffer->contents(), index_data, sizeof(uint32_t) * 3);

    // uniform buffer
    uniform_buffer = device->newBuffer((sizeof(UBO_VS) + 0xff) & ~0xff, MTL::CPUCacheModeDefaultCache);
    uniform_buffer->setLabel(NS::String::string("UBO", NS::ASCIIStringEncoding));

    {
        mymodel.translate = glm::vec3(0.0f, 0.0f, 0.0f);
        mymodel.scale = glm::vec3(1.0f, 1.0f, 1.0f);

        glm::mat4 p = glm::perspective(camera.zoom(), (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT, 0.1f, 1000.0f);
        glm::mat4 v = camera.look_at();
        glm::mat4 m = model_mat(&mymodel);
        ubo_data.mvp = p * v * m;
        /* ubo_data.mvp = glm::mat4(1.0f); */

        memcpy(uniform_buffer->contents(), &ubo_data, sizeof(ubo_data));
    }

    // loading shaders
    NS::String* filePath = NS::String::string("shader.metallib", NS::ASCIIStringEncoding);
    library = device->newLibrary(filePath, &error);

    if(error) {
        std::cerr << "Error when loading default library\n";
        exit(EXIT_FAILURE);
    }

    if(!library) {
        std::cerr << "Failed to load default library\n";
        exit(EXIT_FAILURE);
    }

    vert_fun = library->newFunction(NS::String::string("VS", NS::ASCIIStringEncoding));
    frag_fun = library->newFunction(NS::String::string("FS", NS::ASCIIStringEncoding));

    if (!vert_fun) {
        std::cerr << "Failed to load VS function from library\n";
        exit(EXIT_FAILURE);
    }

    if (!frag_fun) {
        std::cerr << "Failed to load FS function from library\n";
        exit(EXIT_FAILURE);
    }

    // pipeline
    MTL::RenderPipelineDescriptor* descriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    descriptor->setLabel(NS::String::string("Simple pipeline", NS::ASCIIStringEncoding));
    descriptor->setVertexFunction(vert_fun);
    descriptor->setFragmentFunction(frag_fun);
    descriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB);

    MTL::VertexDescriptor* vert_desc = MTL::VertexDescriptor::vertexDescriptor();
    // position attr
    vert_desc->attributes()->object(0)->setFormat(MTL::VertexFormatFloat3);
    vert_desc->attributes()->object(0)->setOffset(0);
    vert_desc->attributes()->object(0)->setBufferIndex(0);
    // color attr
    vert_desc->attributes()->object(1)->setFormat(MTL::VertexFormatFloat3);
    vert_desc->attributes()->object(1)->setOffset(sizeof(float) * 3);
    vert_desc->attributes()->object(1)->setBufferIndex(0);
    // layout
    vert_desc->layouts()->object(0)->setStepFunction(MTL::VertexStepFunctionPerVertex);
    vert_desc->layouts()->object(0)->setStride(sizeof(Vertex));

    descriptor->setVertexDescriptor(vert_desc);

    pipeline_state = device->newRenderPipelineState(descriptor, &error);

    if (!pipeline_state) {
        std::cout << "Failed to create pipeline state: " << error->localizedDescription()->utf8String() << "\n";
        exit(EXIT_FAILURE);
    }
}

void cleanup()
{
    cleanup_resources();
    cleanup_metal();

    std::cout << "cleanup\n";

    SDL_Metal_DestroyView(metal_view);
    SDL_DestroyWindow(sdl_window);
    SDL_Quit();
}

void cleanup_metal()
{
    std::cout << "cleanup metal\n";

    if (command_buffer)
        command_buffer->release();

    command_queue->release();

    device->release();
}

void cleanup_resources()
{
    std::cout << "cleanup resources\n";

    vert_fun->release();
    frag_fun->release();

    library->release();

    vertex_buffer->release();
    index_buffer->release();
    uniform_buffer->release();

    pipeline_state->release();
}

void render()
{
    if (command_buffer)
        command_buffer->release();

    command_buffer = command_queue->commandBuffer();
    command_buffer->setLabel(NS::String::string("My command", NS::ASCIIStringEncoding));

    CA::MetalDrawable* drawable = layer->nextDrawable();

    assert(drawable);

    MTL::RenderPassDescriptor* renderpass_desc = MTL::RenderPassDescriptor::renderPassDescriptor();
    renderpass_desc->colorAttachments()->object(0)->setTexture(drawable->texture());
    renderpass_desc->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);

    MTL::ClearColor clearcol;
    clearcol.red = 0.5;
    clearcol.green = 0.0;
    clearcol.blue = 0.5;
    clearcol.alpha = 1.0;

    renderpass_desc->colorAttachments()->object(0)->setClearColor(clearcol);

    assert(renderpass_desc);

    MTL::RenderCommandEncoder* encoder = command_buffer->renderCommandEncoder(renderpass_desc);
    encoder->setLabel(NS::String::string("My encoder", NS::ASCIIStringEncoding));

    MTL::Viewport viewport = {
        0.0,
        0.0,
        (double)WINDOW_WIDTH,
        (double)WINDOW_HEIGHT,
        0.1, // ZNEAR
        1000.0 // ZFAR
    };
    encoder->setViewport(viewport);

    encoder->setRenderPipelineState(pipeline_state);
    encoder->setCullMode(MTL::CullModeNone);

    encoder->setVertexBuffer(vertex_buffer, 0, 0);
    encoder->setVertexBuffer(uniform_buffer, 0, 1);


    encoder->drawIndexedPrimitives(
            MTL::PrimitiveTypeTriangle,
            NS::UInteger(3),
            MTL::IndexTypeUInt32,
            index_buffer,
            NS::UInteger(0)
            );

    encoder->endEncoding();

    command_buffer->presentDrawable(drawable);
    command_buffer->commit();

}

void process_input()
{
    if (input_mgr.quit_requested() || input_mgr.is_pressed(SDLK_ESCAPE)) {
        quit = true;
    }
}

void frame_start()
{
    last_time = current_time;
    current_time = SDL_GetTicks();

    delta_time = (float)(current_time - last_time) / 1000;
}

void delay()
{
    Uint32 frame_time;

    frame_time = SDL_GetTicks() - current_time;
    if (TICKS_PER_FRAME > frame_time) {
        SDL_Delay(TICKS_PER_FRAME - frame_time);
    }
}

void mainloop()
{
    last_time = SDL_GetTicks();
    current_time = SDL_GetTicks();
    delta_time = 0.0f;

    while (!quit) {
        frame_start();

        input_mgr.update();
        process_input();

        render();
        delay();
    }
}

int main(int argc, char** argv)
{
    init();
    mainloop();
    cleanup();

    return 0;
}
