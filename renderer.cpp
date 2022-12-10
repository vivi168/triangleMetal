#include <iostream>
#include <cstdlib>
#include <cassert>

#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#include "renderer.h"

#define NSSTRING(s) (NS::String::string((s), NS::ASCIIStringEncoding))

enum {
    X = 0, Y, Z, W
};

typedef float vec2[2];
typedef float vec3[3];
typedef float quat[4];

struct Vertex {
    vec3 position; // attributes 0
    vec3 color;    // attributes 1
};

Renderer::Renderer(unsigned int w, unsigned int h, std::string t) : title(t)
{
    viewport = { 0.0, 0.0, (double)w, (double)h, 0.1, 1000.0 };
}

void Renderer::init()
{
    std::cout << "init\n";

    assert(SDL_Init(SDL_INIT_EVERYTHING) == 0);

    create_window();

    // init metal
    {
        metal_view = SDL_Metal_CreateView(sdl_window);
        layer = (CA::MetalLayer*)SDL_Metal_GetLayer(metal_view);

        // create device
        device = MTL::CreateSystemDefaultDevice();
        layer->setDevice(device);

        command_queue = device->newCommandQueue();
    }

    init_resources();
}

void Renderer::create_window()
{
    std::cout << "create window\n";

    sdl_window = SDL_CreateWindow(title.c_str(),
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            (int)viewport.width, (int)viewport.height,
            SDL_WINDOW_METAL | SDL_WINDOW_SHOWN);

    assert(sdl_window);

    // SDL_SetRelativeMouseMode(SDL_TRUE);
}

void Renderer::init_resources()
{
    NS::Error* error;
    std::cout << "init resources\n";

    Vertex vertex_data[3] = {
        {{  1.0f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }},
        {{ -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }},
        {{  0.0f,  1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }},
    };

    uint32_t index_data[3] = { 0, 1, 2 };

    // vertex buffer
    vertex_buffer = device->newBuffer(sizeof(Vertex) * 3, MTL::CPUCacheModeDefaultCache);
    vertex_buffer->setLabel(NSSTRING("VBO"));
    memcpy(vertex_buffer->contents(), vertex_data, sizeof(Vertex) * 3);

    // index buffer
    index_buffer = device->newBuffer(sizeof(uint32_t) * 3, MTL::CPUCacheModeDefaultCache);
    index_buffer->setLabel(NSSTRING("IBO"));
    memcpy(index_buffer->contents(), index_data, sizeof(uint32_t) * 3);

    // uniform buffer
    uniform_buffer = device->newBuffer((sizeof(UBO_VS) + 0xff) & ~0xff, MTL::CPUCacheModeDefaultCache);
    uniform_buffer->setLabel(NSSTRING("UBO"));

    // loading shaders
    NS::String* filePath = NSSTRING("shader.metallib");
    library = device->newLibrary(filePath, &error);

    if(error) {
        std::cerr << "Error when loading default library\n";
        exit(EXIT_FAILURE);
    }

    if(!library) {
        std::cerr << "Failed to load default library\n";
        exit(EXIT_FAILURE);
    }

    vert_fun = library->newFunction(NSSTRING("VS"));
    frag_fun = library->newFunction(NSSTRING("FS"));

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
    descriptor->setLabel(NSSTRING("Simple pipeline"));
    descriptor->setVertexFunction(vert_fun);
    descriptor->setFragmentFunction(frag_fun);
    descriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormat::PixelFormatRGBA8Unorm_sRGB);

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

    vert_fun->release();
    frag_fun->release();
    library->release();
    descriptor->release();
}

void Renderer::cleanup()
{
    cleanup_resources();

    std::cout << "cleanup\n";

    command_queue->release();
    device->release();

    SDL_Metal_DestroyView(metal_view);
    SDL_DestroyWindow(sdl_window);
    SDL_Quit();
}

void Renderer::cleanup_resources()
{
    std::cout << "cleanup resources\n";

    vertex_buffer->release();
    index_buffer->release();
    uniform_buffer->release();

    pipeline_state->release();
}

void Renderer::draw()
{
    // update_uniform();

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
    MTL::CommandBuffer* command_buffer = command_queue->commandBuffer();
    command_buffer->setLabel(NSSTRING("My command"));

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
    encoder->setLabel(NSSTRING("My encoder"));

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
            NS::UInteger(0));

    encoder->endEncoding();

    command_buffer->presentDrawable(drawable);
    command_buffer->commit();

    pool->release();
}

float Renderer::frame_start()
{
    last_time = current_time;
    current_time = SDL_GetTicks();

    float delta_time = (float)(current_time - last_time) / 1000;

    // show FPS
    {
        std::string s = title + " FPS: " + std::to_string(1.0f / delta_time);
        SDL_SetWindowTitle(sdl_window, s.c_str());
    }

    return delta_time;
}

void Renderer::update_uniform(UBO_VS* data)
{
    memcpy(uniform_buffer->contents(), data, sizeof(UBO_VS));
}
