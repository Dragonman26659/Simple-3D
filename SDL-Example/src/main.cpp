#define SDL_WINDOW

#include "Simple3D.h"
#include <SDL.h>
#include <stdio.h>


#define main main

void error_callback(const char* description) {
    fprintf(stderr, "Error: %s\n", description);
}

int main() {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        fprintf(stderr, "Failed to initialize SDL: %s\n", SDL_GetError());
        return -1;
    }

    // Create window
    SDL_Window* window = SDL_CreateWindow(
        "SDL example",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        640,
        480,
        SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        SDL_Quit();
        return -1;
    }

    // Create Objects
    Simple3D::Renderer* renderer = new Simple3D::Renderer(window, "No engine", "SDL Example");
    Simple3D::Model* myModel = new Simple3D::Model(TriangleVertices, TriangleIndices);
    Simple3D::Camera* mainCam = new Simple3D::Camera();

    // Setup Materials for all models
    Simple3D::MaterialInfo info;
    info.FragmentSource = "shaders/frag.spv";
    info.vertexSource = "shaders/vert.spv";
    info.textures.push_back("textures/pop_cat.png");

    myModel->BindMaterial(renderer->CreateMaterial(info));

    // Set Positions
    mainCam->setPosition(glm::vec3(0.0f, 0.0f, 2.0f));
    mainCam->perspectiveMode = true;

    myModel->SetTransform(glm::mat4(1.0f));

    mainCam->lookAt(glm::vec3(0.0f));

    renderer->SubmitMainCamera(mainCam);

    // Main loop
    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        // Submit models to be rendered
        renderer->SumbitModelToFrame(myModel);

        // Render
        renderer->Render();
    }

    // Cleanup
    renderer->WaitToFinish();

    delete myModel;
    delete mainCam;
    delete renderer;

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}