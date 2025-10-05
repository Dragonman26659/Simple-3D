#ifdef SDL_WINDOW
#define TINYOBJLOADER_IMPLEMENTATION

#include "Simple3D.h"
#include <tiny_obj_loader.h>
#include <SDL.h>
#include <stdio.h>

#ifdef USEIMGUI
#include "imgui.h"

// Backend implementations
#include "backends/imgui_impl_vulkan.cpp"
#include "backends/imgui_impl_sdl2.cpp"
#endif // USEIMGUI



#define main main

class FPSCameraController {
private:
    Simple3D::Camera& camera;
    const float sensitivity = 0.1f;
    const float moveSpeed = 1.0f;

public:
    glm::vec2 lastMousePos = { 0.0f, 0.0f };
    bool mouseLocked = false;

    FPSCameraController(Simple3D::Camera& cam) : camera(cam) {}

    void handleInput(float deltatime) {

        if (!mouseLocked) return;

        const Uint8* key_state = SDL_GetKeyboardState(nullptr);

        // Get current position and rotation
        glm::vec3 pos = camera.getPosition();
        glm::vec3 rot = camera.getRotation();

        // Movement controls
        if (key_state[SDL_SCANCODE_W]) {
            // Move forward along current facing direction
            float yawRad = glm::radians(rot.y);
            pos.z += std::cos(yawRad) * moveSpeed * deltatime;
            pos.x -= std::sin(yawRad) * moveSpeed * deltatime;
        }
        if (key_state[SDL_SCANCODE_S]) {
            // Move backward along current facing direction
            float yawRad = glm::radians(rot.y);
            pos.z -= std::cos(yawRad) * moveSpeed * deltatime;
            pos.x += std::sin(yawRad) * moveSpeed * deltatime;
        }
        if (key_state[SDL_SCANCODE_A]) {
            // Move left perpendicular to facing direction
            float yawRad = glm::radians(rot.y);
            pos.x += std::cos(yawRad) * moveSpeed * deltatime;
            pos.z += std::sin(yawRad) * moveSpeed * deltatime;
        }
        if (key_state[SDL_SCANCODE_D]) {
            // Move right perpendicular to facing direction
            float yawRad = glm::radians(rot.y);
            pos.x -= std::cos(yawRad) * moveSpeed * deltatime;
            pos.z -= std::sin(yawRad) * moveSpeed * deltatime;
        }
        if (key_state[SDL_SCANCODE_SPACE]) {
            pos.y += moveSpeed * deltatime;
        }
        if (key_state[SDL_SCANCODE_LSHIFT]) {
            pos.y -= moveSpeed * deltatime;
        }

        // Update position
        camera.setPosition(pos);
    }

    void handleMouseEvent(const SDL_Event& event) {
        if (event.type == SDL_MOUSEMOTION) {
            int mouseX, mouseY;
            SDL_GetMouseState(&mouseX, &mouseY);


            // Check if right click pressed - if so then enable mouseLocked
            if (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(SDL_BUTTON_RIGHT)) {
                mouseLocked = true;
                SDL_SetRelativeMouseMode(SDL_TRUE);
            }
            else {
                mouseLocked = false;
                SDL_SetRelativeMouseMode(SDL_FALSE);
                return;
            }

            glm::vec2 currentMousePos(mouseX, mouseY);
            glm::vec2 mouseDelta = currentMousePos - lastMousePos;

            // Calculate new rotation
            float yaw = mouseDelta.x * sensitivity;
            float pitch = -mouseDelta.y * sensitivity;

            // Get current rotation
            glm::vec3 rot = camera.getRotation();
            rot.y += yaw;
            rot.x += pitch;

            // Clamp pitch to prevent flipping
            rot.x = glm::clamp(rot.x, -89.0f, 89.0f);

            // Update rotation
            camera.setRotation(rot.x, rot.y, 0.0f);
        }

        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
            SDL_SetRelativeMouseMode(SDL_FALSE);
            mouseLocked = false;
        }
    }

    void handleEvent(const SDL_Event& event, float deltatime) {
        handleMouseEvent(event);
        handleInput(deltatime);
    }
};



Simple3D::Model* loadModel(std::string MODEL_PATH) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::unordered_map<Vertex, uint32_t> uniqueVertices;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, MODEL_PATH.c_str())) {
        throw std::runtime_error(err);
    }

    // Initialize vertices without tangents first
    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            // Position
            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            // Normal
            if (index.normal_index >= 0) {
                vertex.normal = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                };
            }
            else {
                vertex.normal = glm::vec3(0.0f, 0.0f, 1.0f);
            }

            // Texture coordinates
            if (index.texcoord_index >= 0) {
                vertex.texCoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };
            }
            else {
                vertex.texCoord = glm::vec2(0.0f, 0.0f);
            }

            // Color: use vertex color if present
            vertex.color = glm::vec3(1.0f, 1.0f, 1.0f);

            // Push vertex
            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }
            indices.push_back(uniqueVertices[vertex]);
        }
    }

    // Calculate tangents per triangle
    for (size_t i = 0; i < indices.size(); i += 3) {
        Vertex& v0 = vertices[indices[i + 0]];
        Vertex& v1 = vertices[indices[i + 1]];
        Vertex& v2 = vertices[indices[i + 2]];

        glm::vec3 edge1 = v1.pos - v0.pos;
        glm::vec3 edge2 = v2.pos - v0.pos;
        glm::vec2 deltaUV1 = v1.texCoord - v0.texCoord;
        glm::vec2 deltaUV2 = v2.texCoord - v0.texCoord;

        float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

        glm::vec3 tangent = f * (deltaUV2.y * edge1 - deltaUV1.y * edge2);
        tangent = glm::normalize(tangent);

        // Add the same tangent to all three vertices
        v0.tangent += tangent;
        v1.tangent += tangent;
        v2.tangent += tangent;
    }

    // Normalize all tangents
    for (auto& vertex : vertices) {
        vertex.tangent = glm::normalize(vertex.tangent);
    }

    return new Simple3D::Model(vertices, indices);
}


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

    Simple3D::RenderInstance* mainInstance = renderer->CreateRenderInstance();    


    // Setup camera
    Simple3D::Camera* mainCam = new Simple3D::Camera();
    FPSCameraController cameraController(*mainCam);

#ifdef USEIMGUI
    renderer->initImgui();

    Simple3D::RenderTexture* ImguiTexture;
    Simple3D::TextureBinding* ImguiBinding;
    VkDescriptorSet ImguiDiscriptor;
    Simple3D::RenderInstance* ImguiInstance;


    // Test render to texture
    ImguiTexture = renderer->CreateRenderTexture(640, 480);

    // Setup the instances
    ImguiInstance = renderer->CreateRenderInstance(ImguiTexture);
    ImguiInstance->SetCamera(mainCam);



    ImguiBinding = ImguiTexture->getBinding();

    ImguiDiscriptor = ImGui_ImplVulkan_AddTexture(
        ImguiBinding->sampler,
        ImguiBinding->view,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
#endif // USEIMGUI

    Simple3D::Model* myModel = loadModel("meteor.obj");
    Simple3D::Light* myLight = new Simple3D::Light();


    myLight->type = Simple3D::directional;
    myLight->castShadows = false;
    myLight->diffuseColor = glm::vec3(1.0f, 1.0f, 1.0f);
    myLight->ambientColor = glm::vec3(1.0f, 1.0f, 1.0f);
    myLight->position = glm::vec3(0.0f, 0.0f, 2.0f);

    // Setup Materials for all models
    Simple3D::MaterialInfo info;
    info.FragmentSource = "shaders/lit_PBR.spv";
    info.vertexSource = "shaders/vert.spv";
    info.textures["Albeado"] = ("textures/5382.jpg");
    info.textures["Normal"] = ("textures/asteroid_normal.jpg");
    info.textures["Roughness"] = ("textures/asteroid_rough.jpg");
    info.isLit = true;


    // Bind material to model and set its postion
    Simple3D::Material* material = renderer->CreateMaterial(info);
    myModel->BindMaterial(material);
    myModel->SetTransform(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)));
    myModel->SetTransform(glm::rotate(myModel->GetTransform(), glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f)));

    // Set Positions
    mainCam->setPosition(glm::vec3(0.0f, 0.5f, 2.0f));
    mainCam->perspectiveMode = true;
    mainCam->lookAt(glm::vec3(0.0f));

    renderer->SubmitMainCamera(mainCam, mainInstance);





    // Main loop
    bool running = true;
    SDL_Event event;


    // Framerate
    const float targetFrameTime = 1.0f / 60.0f;
    float lastFrameTime = 0.0f;
    lastFrameTime = SDL_GetTicks() / 1000.0f;

    while (running) {
        float currentTime = SDL_GetTicks() / 1000.0f;
        float deltaTime = currentTime - lastFrameTime;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            cameraController.handleEvent(event, deltaTime);
#ifdef USEIMGUI
            ImGui_ImplSDL2_ProcessEvent(&event);
#endif // USEIMGUI
        }

#ifdef USEIMGUI
        renderer->SumbitModelToFrame(myModel, ImguiInstance);
        renderer->SubmitLightToFrame(*myLight, ImguiInstance);



        // Only bother rendering imgui if the window is actuale able to draw
        if (renderer->NewImguiframe()) {
            ImGui::Begin("Test view");

            // Display the texture
            ImGui::Image((ImTextureID)ImguiDiscriptor, { (float)ImguiTexture->width, (float)ImguiTexture->height});



            ImGui::End();

            ImGui::Render();
        }
#endif // USEIMGUI




        renderer->SumbitModelToFrame(myModel, mainInstance);
        renderer->SubmitLightToFrame(*myLight, mainInstance);
        renderer->Render();

        lastFrameTime = currentTime;
        float frameTime = SDL_GetTicks() / 1000.0f - currentTime;
        if (frameTime < targetFrameTime) {
            SDL_Delay((targetFrameTime - frameTime) * 1000.0f);
        }


        if (cameraController.mouseLocked) {
            int height, width;
            SDL_GetWindowSize(window, &width, &height);
            SDL_WarpMouseInWindow(window, width / 2, height / 2);
            cameraController.lastMousePos = glm::vec2(width / 2, height / 2);
        }
    }

    // Cleanup
    renderer->WaitToFinish();

    delete myModel;
    delete mainCam;
    delete myLight;
    delete renderer;
    delete material;

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

#endif