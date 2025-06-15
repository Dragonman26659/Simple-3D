Simple3D Engine
Simple3D is a lightweight 3D rendering API built on top of Vulkan with SDL2 integration and optional ImGui support. It abstracts away low-level Vulkan complexity, enabling quick prototyping of 3D applications with camera controls, lighting, materials, and real-time rendering.

🚀 Features
  Vulkan-based renderer

  SDL2 window/input support

  First-person camera control

  Model rendering with materials

  Basic lighting (directional, shadow toggle)

  Optional ImGui integration

  Shader and texture binding system

🛠️ Basic Example
This is a minimal example of how to use the Simple3D API.

```
// Create a Vulkan-backed renderer with SDL
Simple3D::Renderer* renderer = new Simple3D::Renderer(window, "Engine Name", "App Name");

// Create a render instance (scene)
Simple3D::RenderInstance* instance = renderer->CreateRenderInstance();

// Create and configure a camera
Simple3D::Camera* camera = new Simple3D::Camera();
camera->setPosition(glm::vec3(0.0f, 0.5f, 2.0f));
camera->lookAt(glm::vec3(0.0f));
camera->perspectiveMode = true;

// Attach the camera to the renderer
renderer->SubmitMainCamera(camera, instance);

// Create a directional light
Simple3D::Light* light = new Simple3D::Light();
light->type = Simple3D::directional;
light->diffuseColor = glm::vec3(1.0f);
light->ambientColor = glm::vec3(1.0f);
light->position = glm::vec3(0.0f, 0.0f, 2.0f);
renderer->SubmitLightToFrame(*light, instance);

// Create a material and bind it to a model
Simple3D::MaterialInfo matInfo;
matInfo.FragmentSource = "shaders/frag.spv";
matInfo.vertexSource = "shaders/vert.spv";
matInfo.textures["Albeado"] = "textures/albeado_viking_room.png";

Simple3D::Material* material = renderer->CreateMaterial(matInfo);
model->BindMaterial(material);

// Submit model to render frame
renderer->SumbitModelToFrame(model, instance);
```
🔧 Note: In the above example, model is assumed to be an instance of Simple3D::Model. How it’s loaded (e.g., via tinyobjloader) is up to the application developer and not part of the Simple3D API itself.

🎮 Camera Controller
  While not part of the Simple3D API, the example project includes a basic FPS-style camera controller built using SDL keyboard and mouse input:

```
FPSCameraController cameraController(*camera);
cameraController.handleEvent(event, deltaTime);
```
  This manages WASD movement, mouse look, and jump/crouch.

🧱 API Components
✅ Renderer
  Manages Vulkan initialization, window drawing, and scene submission.

✅ Camera
  Supports perspective and orthographic modes, position, rotation, and lookAt() behavior.

✅ Model
  User-defined geometry, optionally textured and shaded.

✅ Material
  Binds SPIR-V shaders and texture maps to models.

✅ Light
  Supports directional lighting, color, intensity, and shadow flags.

📦 Dependencies
  Vulkan SDK

  SDL2

  GLM

  (Optional) ImGui

📁 Example Project
  An example usage file (SDL-Example/src/main.cpp) is provided in the repository to demonstrate:

  Initializing SDL + Vulkan

  Loading an OBJ file (via custom utility)

  Using FPS-style input (via a custom class)

  Submitting camera, light, and model to renderer

  📝 Note: loadModel() and similar helpers are part of the example, not the API.

🧑‍💻 License
  MIT (shown in License file)

🤝 Acknowledgements
  SDL2

  Vulkan

  GLM

  Dear ImGui
