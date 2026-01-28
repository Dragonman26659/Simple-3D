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

class CameraController {
private:
    Simple3D::Camera& camera;
    const float sensitivity = 10.0f;
    const float moveSpeed = 5.0f;

public:
    glm::vec2 lastMousePos = { 0.0f, 0.0f };
    bool mouseLocked = false;

    CameraController(Simple3D::Camera& cam) : camera(cam) {}

    void handleInput(float deltatime) {
        if (!mouseLocked) return;

        const Uint8* key_state = SDL_GetKeyboardState(nullptr);

        glm::vec3 pos = camera.getPosition();

        // Use camera’s true direction vectors
        glm::vec3 forward = camera.getForward();
        glm::vec3 right = camera.getRight();
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

        // Movement controls (standard FPS convention)
        if (key_state[SDL_SCANCODE_W])
            pos += forward * moveSpeed * deltatime;
        if (key_state[SDL_SCANCODE_S])
            pos -= forward * moveSpeed * deltatime;
        if (key_state[SDL_SCANCODE_A])
            pos -= right * moveSpeed * deltatime;
        if (key_state[SDL_SCANCODE_D])
            pos += right * moveSpeed * deltatime;
        if (key_state[SDL_SCANCODE_SPACE])
            pos -= up * moveSpeed * deltatime;
        if (key_state[SDL_SCANCODE_LSHIFT])
            pos += up * moveSpeed * deltatime;

        camera.setPosition(pos);
    }

    void handleMouseEvent(const SDL_Event& event, float dt) {
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
            float yaw = -mouseDelta.x * sensitivity * dt;
            float pitch = mouseDelta.y * sensitivity * dt;

            // Get current rotation
            glm::vec3 rot = camera.getRotationEuler();
            rot.y += yaw;
            rot.x += pitch;

            // Clamp pitch to prevent flipping
            rot.x = glm::clamp(rot.x, -89.0f, 89.0f);

            // Update rotation
            camera.setRotationEuler(rot.x, rot.y, 0.0f);
        }

        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
            SDL_SetRelativeMouseMode(SDL_FALSE);
            mouseLocked = false;
        }
    }

    void handleEvent(const SDL_Event& event, float deltatime) {

        handleMouseEvent(event, deltatime);
        handleInput(deltatime);

    }
};

Simple3D::Model* loadModel(std::string MODEL_PATH) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, MODEL_PATH.c_str())) {
        throw std::runtime_error(err);
    }

    // Helper key to uniquely identify a vertex by its indices (tri-safe, avoids float-key issues)
    struct IndexKey {
        int vi, ni, ti;
        bool operator==(IndexKey const& o) const noexcept {
            return vi == o.vi && ni == o.ni && ti == o.ti;
        }
    };
    struct IndexKeyHash {
        size_t operator()(IndexKey const& k) const noexcept {
            uint64_t a = static_cast<uint64_t>(k.vi + 0x9e3779b9);
            uint64_t b = static_cast<uint64_t>(k.ni + 0x9e3779b9);
            uint64_t c = static_cast<uint64_t>(k.ti + 0x9e3779b9);
            uint64_t h = a;
            h ^= b + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
            h ^= c + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
            return static_cast<size_t>(h);
        }
    };

    std::unordered_map<IndexKey, uint32_t, IndexKeyHash> uniqueVertices;

    // Build vertices/indices using index-key deduping (no float hashing)
    for (const auto& shape : shapes) {
        for (const auto& idx : shape.mesh.indices) {
            IndexKey key{ idx.vertex_index, idx.normal_index, idx.texcoord_index };

            auto it = uniqueVertices.find(key);
            if (it != uniqueVertices.end()) {
                indices.push_back(it->second);
                continue;
            }

            Vertex vertex{}; // default-initialized; ensure Vertex ctor zeroes tangent if needed

            // Position (guard against missing or out-of-range indices)
            if (key.vi >= 0 && (size_t)(3 * key.vi + 2) < attrib.vertices.size()) {
                vertex.pos = {
                    attrib.vertices[3 * key.vi + 0],
                    attrib.vertices[3 * key.vi + 1],
                    attrib.vertices[3 * key.vi + 2]
                };
            }
            else {
                vertex.pos = glm::vec3(0.0f);
            }

            // Normal
            if (key.ni >= 0 && (size_t)(3 * key.ni + 2) < attrib.normals.size()) {
                vertex.normal = {
                    attrib.normals[3 * key.ni + 0],
                    attrib.normals[3 * key.ni + 1],
                    attrib.normals[3 * key.ni + 2]
                };
            }
            else {
                vertex.normal = glm::vec3(0.0f, 0.0f, 1.0f);
            }

            // TexCoord (flip V)
            if (key.ti >= 0 && (size_t)(2 * key.ti + 1) < attrib.texcoords.size()) {
                vertex.texCoord = {
                    attrib.texcoords[2 * key.ti + 0],
                    1.0f - attrib.texcoords[2 * key.ti + 1]
                };
            }
            else {
                vertex.texCoord = glm::vec2(0.0f, 0.0f);
            }

            // Color fallback (keep your original behavior)
            vertex.color = glm::vec3(1.0f, 1.0f, 1.0f);

            uint32_t newIndex = static_cast<uint32_t>(vertices.size());
            vertices.push_back(vertex);
            uniqueVertices.emplace(key, newIndex);
            indices.push_back(newIndex);
        }
    }

    // Prepare accumulators for tangents & bitangents (we cannot change Vertex layout)
    std::vector<glm::vec3> tangentAccum(vertices.size(), glm::vec3(0.0f));
    std::vector<glm::vec3> bitangentAccum(vertices.size(), glm::vec3(0.0f));

    // Calculate tangents and bitangents per triangle
    const float EPSILON = 1e-8f;
    const float EPSILON_SQR = EPSILON * EPSILON;
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        uint32_t i0 = indices[i + 0];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];

        const Vertex& v0 = vertices[i0];
        const Vertex& v1 = vertices[i1];
        const Vertex& v2 = vertices[i2];

        glm::vec3 edge1 = v1.pos - v0.pos;
        glm::vec3 edge2 = v2.pos - v0.pos;
        glm::vec2 deltaUV1 = v1.texCoord - v0.texCoord;
        glm::vec2 deltaUV2 = v2.texCoord - v0.texCoord;

        float det = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;
        if (std::fabs(det) < EPSILON) {
            // Degenerate UVs for this triangle — skip accumulation for this tri.
            continue;
        }

        float f = 1.0f / det;

        glm::vec3 tangent = f * (deltaUV2.y * edge1 - deltaUV1.y * edge2);
        glm::vec3 bitangent = f * (-deltaUV2.x * edge1 + deltaUV1.x * edge2);

        // Accumulate (do not normalize yet)
        tangentAccum[i0] += tangent;
        tangentAccum[i1] += tangent;
        tangentAccum[i2] += tangent;

        bitangentAccum[i0] += bitangent;
        bitangentAccum[i1] += bitangent;
        bitangentAccum[i2] += bitangent;
    }

    // Normalize, orthogonalize and compute handedness per-vertex, write into vertex.tangent (vec4)
    for (size_t vi = 0; vi < vertices.size(); ++vi) {
        Vertex& vert = vertices[vi];

        glm::vec3 N = glm::normalize(vert.normal);
        glm::vec3 T = tangentAccum[vi];
        glm::vec3 B = bitangentAccum[vi];

        // If the accumulated tangent is nearly zero (no valid tri or degenerate), build a fallback
        if (glm::dot(T, T) < EPSILON_SQR) {
            // produce some tangent orthogonal to N
            glm::vec3 up = glm::abs(N.y) < 0.999f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
            T = glm::normalize(glm::cross(up, N));
            // No reliable bitangent available; compute from cross
            B = glm::normalize(glm::cross(N, T));
        }
        else {
            // Orthogonalize T against N and normalize
            T = T - N * glm::dot(N, T);
            if (glm::dot(T, T) < EPSILON_SQR) {
                // fallback if orthogonalization collapsed
                glm::vec3 up = glm::abs(N.y) < 0.999f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
                T = glm::normalize(glm::cross(up, N));
            }
            else {
                T = glm::normalize(T);
            }

            // If no reliable accumulated bitangent, recompute approximation
            if (glm::dot(B, B) < EPSILON_SQR) {
                B = glm::normalize(glm::cross(N, T));
            }
            else {
                B = glm::normalize(B);
            }
        }

        // Compute handedness: +1 or -1
        float handedness = (glm::dot(glm::cross(N, T), B) < 0.0f) ? -1.0f : 1.0f;

        // Store tangent.xyz = T and tangent.w = handedness (float)
        vert.tangent = glm::vec4(T, handedness);
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


    // Setup camera
    Simple3D::Camera* mainCam = new Simple3D::Camera();
    CameraController cameraController(*mainCam);

#ifdef USEIMGUI
    renderer->initImgui();

    Simple3D::RenderTexture* ImguiTexture;
    Simple3D::TextureBinding* ImguiBinding;
    VkDescriptorSet ImguiDiscriptor;


    // Test render to texture
    ImguiTexture = renderer->CreateRenderTexture(640, 480);



    ImguiBinding = ImguiTexture->getBinding();

    ImguiDiscriptor = ImGui_ImplVulkan_AddTexture(
        ImguiBinding->sampler,
        ImguiBinding->view,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
#endif // USEIMGUI


    // LOAD SHADERS
    Simple3D::ShaderSet* shader = renderer->CreateShaderSet("PBR_Shaders");
    shader->LoadStage("shaders/lit_PBR.spv", Simple3D::ShaderStage::Fragment);
    shader->LoadStage("shaders/vert.spv", Simple3D::ShaderStage::Vertex);


    // Light setup
    Simple3D::Light* myLight = new Simple3D::Light();
    myLight->type = Simple3D::point;
    myLight->castShadows = false;

    // Color setup
    myLight->Color = glm::vec3(1.0f, 1.0f, 1.0f); // subtle ambient tint

    // Intensity and position
    myLight->intensity = 0.3f;
    myLight->position = glm::vec3(0.0f, 0.0f, 2.0f);

    // No direction needed for point lights
    myLight->direction = glm::vec3(0.0f);

    // Shadows off
    myLight->shadowBias = 0.0f;
    myLight->shadowIntensity = 0.0f;

    // Spotlight fields — unused for point light
    myLight->cutoffAngle = 0.0f;
    myLight->outerCutoffAngle = 0.0f;


    // Load model
    Simple3D::Model* myModel = loadModel("meteor.obj");

    // Set up model material
    Simple3D::MaterialInfo info;
    info.textures["Albedo"] = ("textures/5382.jpg");
    info.textures["Normal"] = ("textures/asteroid_normal.jpg");
    info.textures["Roughness"] = ("textures/asteroid_rough.jpg");
    info.textures["Metalic"] = ("textures/black.jpeg");
    info.textures["Emissive"] = ("textures/black.jpeg");
    info.textures["AO"] = ("textures/white.jpg");
    info.shaders = shader;


    // Bind material to model and set its postion
    Simple3D::Material* material = renderer->CreateMaterial(info);
    myModel->BindMaterial(material);
    myModel->SetTransform(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)));
    myModel->SetTransform(glm::rotate(myModel->GetTransform(), glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f)));


    // Set camera position
    mainCam->setPosition(glm::vec3(0.0f, 0.5f, 2.0f));
    if (!mainCam->isPerspectiveMode())
        mainCam->toggleProjectionMode();

    mainCam->lookAt(glm::vec3(0.0f));


    // Create RenderGraphs
    Simple3D::RenderGraph* mainGraph = renderer->CreateRenderGraph("Main");
    Simple3D::RenderTarget mainTarget = Simple3D::RenderTarget();
    mainTarget.swapchain = renderer->GetSwapChain();
    mainTarget.depthTexture = renderer->CreateDepth(mainTarget);
    
    mainGraph->AddResource("Swapchain", &mainTarget, true);

    Simple3D::RenderPass* GeometryPass = new Simple3D::ForwardPass(mainCam);
    GeometryPass->outputResources.push_back("Swapchain");
    mainGraph->AddPass(GeometryPass);



#ifdef USEIMGUI
    
    Simple3D::RenderGraph* imguiGraph = renderer->CreateRenderGraph("Main");
    Simple3D::RenderTarget imguiTarget = Simple3D::RenderTarget();
    imguiTarget.
        
        
        
        ture(ImguiTexture);
    imguiTarget.depthTexture = renderer->CreateDepth(imguiTarget);

    imguiGraph->AddResource("ImguiTexture", &imguiTarget);

    Simple3D::RenderPass* ImguiGPass = new Simple3D::ForwardPass(mainCam);
    ImguiGPass->outputResources.push_back("ImguiTexture");
    imguiGraph->AddPass(ImguiGPass);


    // Display the texture as a thumbnail
    ImTextureID texID = (ImTextureID)ImGui_ImplVulkan_AddTexture(
        ImguiTexture->getBinding()->sampler,
        ImguiTexture->getBinding()->view,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

#endif


    renderer->BuildRenderGraphs();








    Simple3D::RenderData RenderData;

    RenderData.models.push_back(myModel);
    RenderData.lights.push_back(*myLight);



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


        // Only bother rendering imgui if the window is actuale able to draw
        if (renderer->NewImguiframe()) {
            ImGui::Begin("Light Editor");

            // --- Light Type ---
            const char* lightTypes[] = { "None", "Point", "Spot", "Directional" };
            int currentType = myLight->type;
            if (ImGui::Combo("Light Type", &currentType, lightTypes, IM_ARRAYSIZE(lightTypes))) {
                myLight->type = static_cast<Simple3D::LightType>(currentType);
            }

            // --- Base lighting info ---
            ImGui::SeparatorText("Base information");
            ImGui::ColorEdit3("Color", (float*)&myLight->Color);
            ImGui::SliderFloat("Intensity", &myLight->intensity, 0.0f, 10.0f, "%.2f");

            // --- Position & Direction ---
            ImGui::SeparatorText("Transform");
            ImGui::DragFloat3("Position", (float*)&myLight->position, 0.1f);
            ImGui::DragFloat3("Direction", (float*)&myLight->direction, 0.1f);

            // --- Spot Angles (if spotlight) ---
            if (myLight->type == Simple3D::spot) {
                ImGui::SeparatorText("Spotlight Angles");
                float cutoffDeg = glm::degrees(acos(myLight->cutoffAngle));
                float outerDeg = glm::degrees(acos(myLight->outerCutoffAngle));

                if (ImGui::SliderFloat("Inner Angle", &cutoffDeg, 0.0f, 90.0f, "%.1f°"))
                    myLight->cutoffAngle = glm::cos(glm::radians(cutoffDeg));

                if (ImGui::SliderFloat("Outer Angle", &outerDeg, 0.0f, 90.0f, "%.1f°"))
                    myLight->outerCutoffAngle = glm::cos(glm::radians(outerDeg));
            }

            // --- Shadow Options ---
            ImGui::SeparatorText("Shadows");
            ImGui::Checkbox("Cast Shadows", &myLight->castShadows);
            ImGui::SliderFloat("Shadow Bias", &myLight->shadowBias, 0.0f, 0.05f, "%.4f");
            ImGui::SliderFloat("Shadow Intensity", &myLight->shadowIntensity, 0.0f, 1.0f, "%.2f");

            ImGui::Separator();
            ImGui::Text("Position: (%.2f, %.2f, %.2f)", myLight->position.x, myLight->position.y, myLight->position.z);
            ImGui::Text("Direction: (%.2f, %.2f, %.2f)", myLight->direction.x, myLight->direction.y, myLight->direction.z);

            ImGui::End();


// ===========================
// Material Texture Viewer
// ===========================
            ImGui::Begin("Material Textures");

            if (material) {
                // Iterate all textures in the material
                for (const auto& texPair : material->textures) {
                    const std::string& texName = texPair.first;
                    const Simple3D::TextureBinding& texBinding = texPair.second;

                    ImGui::Text("%s", texName.c_str());

                    // Register this texture for ImGui if not already
                    static std::unordered_map<VkImageView, ImTextureID> imguiTextureIDs;
                    if (imguiTextureIDs.find(texBinding.view) == imguiTextureIDs.end()) {
                        ImTextureID id = (ImTextureID)ImGui_ImplVulkan_AddTexture(
                            texBinding.sampler,
                            texBinding.view,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                        );
                        imguiTextureIDs[texBinding.view] = id;
                    }

                    // Display the texture as a thumbnail
                    ImTextureID texID = imguiTextureIDs[texBinding.view];
                    float thumbSize = 96.0f;
                    ImGui::Image(texID, ImVec2(thumbSize, thumbSize));

                    // Optional tooltip preview
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::Text("Size: %dx%d", texBinding.width, texBinding.height);
                        ImGui::Image(texID, ImVec2(thumbSize * 2, thumbSize * 2));
                        ImGui::EndTooltip();
                    }

                    ImGui::Separator();
                }
            }
            else {
                ImGui::Text("No material loaded.");
            }

            ImGui::End();



            ImGui::Begin("RTT View");

           ImGui::Image(texID, ImVec2(ImguiTexture->getExtent().width, ImguiTexture->getExtent().height));



            ImGui::End();

            ImGui::Render();
        }

#endif // USEIMGUI





        renderer->Render(RenderData);

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

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

#endif