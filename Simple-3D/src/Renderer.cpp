#include "Rendering.h"
#include "Animation/AnimationSolver.h"
#include "Debug/DebugSceneHelpers.h"
#include <algorithm>

namespace Diversity
{
    namespace Rendering
    {
        inline ShaderFeature ParseFeature(const std::string& name)
        {
            std::string s = name;
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);

            if (s == "albedo" || s == "basecolor" || s == "diffuse")
                return ShaderFeature::Albedo;

            if (s == "normal" || s == "normalmap")
                return ShaderFeature::Normal;

            if (s == "roughness")
                return ShaderFeature::Roughness;

            if (s == "metallic" || s == "metalness")
                return ShaderFeature::Metallic;

            if (s == "emissive" || s == "emission")
                return ShaderFeature::Emissive;

            if (s == "ao" || s == "ambientocclusion")
                return ShaderFeature::AO;

            if (s == "transparent" || s == "opacity")
                return ShaderFeature::Transparency;

            if (s == "skinned" || s == "skeletal")
                return ShaderFeature::Skinned;

            if (s == "unlit" || s.empty())
                return ShaderFeature::Unlit;


            // Fallback rule:
            // Unknown feature → treat as Albedo-only safe default
            return ShaderFeature::Albedo;
        }

        bool IsInsideAABB(const glm::vec3& point, const AABB& aabb) {
            if (!aabb.valid) return false;

            glm::vec3 diff = glm::abs(point - aabb.center);
            return (diff.x <= aabb.extents.x &&
                diff.y <= aabb.extents.y &&
                diff.z <= aabb.extents.z);
        }

        float GetSqDistanceToCenter(const glm::vec3& point, const AABB& aabb) {
            glm::vec3 diff = point - aabb.center;
            return glm::dot(diff, diff);
        }

        bool IsLightInFrustum(const Light& light, const Frustum& cameraFrustum) {
            if (light.type == LightType::Directional) return true;

            // Simple sphere-frustum test for point lights
            return FrustumIntersectsSphere(cameraFrustum, light.position, light.radius);
        }

        std::vector<glm::vec3> GetFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view) {
            const glm::mat4 inv = glm::inverse(proj * view);
            std::vector<glm::vec3> frustumCorners;
            for (unsigned int x = 0; x < 2; ++x) {
                for (unsigned int y = 0; y < 2; ++y) {
                    for (unsigned int z = 0; z < 2; ++z) {
                        glm::vec4 pt = inv * glm::vec4(2.0f * x - 1.0f, 2.0f * y - 1.0f, 2.0f * z - 1.0f, 1.0f);
                        frustumCorners.push_back(glm::vec3(pt) / pt.w);
                    }
                }
            }
            return frustumCorners;
        }

        Renderer::Renderer(Window& window, std::string ShadersDirectory, Simple3D::RenderTarget* target)
            : window(window), ShadersDirectory(ShadersDirectory), mainTarget(target)
        {
            // Create Simple3D renderer
            renderer = new Simple3D::Renderer(window.GetSdlWindow(), "DIVERSITY_ENGINE", Application::getInstance().name, false);

            renderLoader = new RenderAssetLoader(renderer);
            AssetLoaderRegistry::Instance().RegisterLoader((AssetLoader*)renderLoader);
            AssetWriterRegistry::Instance().RegisterWriter(
                new RenderAssetWriter()
            );

            RegisterComponents();
        }

        Renderer::~Renderer()
        {
            if (m_TexturePool)    m_TexturePool->Shutdown();

            for (auto tex : textures)
                delete tex;

            for (auto target : renderTargets)
                delete target;

            delete renderer;
        }



        // Load in Shaders, configure RenderGraphs, Setup rendertarget if it is null
        void Renderer::Initialize()
        {
            // Setup basic renderTarget if none provided
            if (mainTarget == nullptr) {
                mainTarget = new Simple3D::RenderTarget();
                mainTarget->swapchain = renderer->GetSwapChain();
                mainTarget->depthTexture = renderer->CreateDepth(*mainTarget);
            }

            // Load main pipeline shaders (Shader dir "Resources/Shaders")
            CreatePassShader("Deffered", "Resources/Shaders/postProsess-vert.spv", "Resources/Shaders/Deffered-frag.spv");
            CreatePassShader("Enviroment", "Resources/Shaders/postProsess-vert.spv", "Resources/Shaders/Enviroment-frag.spv");
            CreatePassShader("Shadow", "Resources/Shaders/shadow-vert.spv", "Resources/Shaders/shadow-frag.spv");
            CreatePassShader("DebugDraw", "Resources/Shaders/DebugDraw-vert.spv", "Resources/Shaders/DebugDraw-frag.spv");
            CreatePassShader("GBUFFER", "Resources/Shaders/gbuff-vert.spv", "Resources/Shaders/gbuff-frag.spv");
            CreatePassShader("BILLBOARDGBUFFER", "Resources/Shaders/particleGBUFFER-vert.spv", "Resources/Shaders/gbuff-frag.spv");
            CreatePassShader("Forward", "Resources/Shaders/gbuff-vert.spv", "Resources/Shaders/forward-frag.spv");

            m_TexturePool = std::make_unique<BindlessTexturePool>();
            m_TexturePool->Init(renderer->GetDevice(), 1024);
        }

        void Renderer::CreateShader(std::string name, std::string vertex,
            std::string fragment, std::string features,
            const char* fixedGuid)
        {
            ShaderAsset* asset = new ShaderAsset();

            auto* shaderSet = renderer->CreateShaderSet(name);
            shaderSet->LoadStage(vertex, Simple3D::ShaderStage::Vertex);
            shaderSet->LoadStage(fragment, Simple3D::ShaderStage::Fragment);
            asset->shaderSet = shaderSet;

            ShaderSignature sig;
            sig.features.insert(ParseFeature(features));

            AssetHandle handle = fixedGuid
                ? AssetPool::Instance().RegisterBuiltin(fixedGuid, asset)
                : AssetPool::Instance().Register(asset);

            ShaderRegistry::Instance().RegisterShader(handle, sig);
        }

        void Renderer::CreatePassShader(std::string name, std::string vertex, std::string fragment) {
            auto* shaderSet = renderer->CreateShaderSet(name);

            shaderSet->LoadStage(vertex, Simple3D::ShaderStage::Vertex);
            shaderSet->LoadStage(fragment, Simple3D::ShaderStage::Fragment);

            shaderSets[name] = shaderSet;
        }

        Simple3D::RenderGraph* Renderer::buildFullGraph(
            std::string name,
            Simple3D::RenderTarget* endTarget,
            std::string ResourceLocation)
        {
            Simple3D::RenderGraph* graph = renderer->CreateRenderGraph(name);

            Simple3D::RenderTexture* Base = renderer->CreateRenderTexture(window.GetWidth(), window.GetHeight(), VK_FORMAT_R8G8B8A8_SRGB);
            Simple3D::RenderTexture* Emmision = renderer->CreateRenderTexture(window.GetWidth(), window.GetHeight(), VK_FORMAT_R8G8B8A8_SRGB);
            Simple3D::RenderTexture* Normal = renderer->CreateRenderTexture(window.GetWidth(), window.GetHeight(), VK_FORMAT_R8G8B8A8_UNORM);
            Simple3D::RenderTexture* PBR = renderer->CreateRenderTexture(window.GetWidth(), window.GetHeight(), VK_FORMAT_R8G8B8A8_UNORM);
            Simple3D::RenderTexture* Subsurf = renderer->CreateRenderTexture(window.GetWidth(), window.GetHeight(), VK_FORMAT_R8G8B8A8_UNORM);
            Simple3D::RenderTexture* Motion = renderer->CreateRenderTexture(window.GetWidth(), window.GetHeight(), VK_FORMAT_R8G8B8A8_UNORM);
            Simple3D::RenderTexture* LIT = renderer->CreateRenderTexture(window.GetWidth(), window.GetHeight());

            textures.push_back(Base);
            textures.push_back(Emmision);
            textures.push_back(Normal);
            textures.push_back(PBR);
            textures.push_back(Subsurf);
            textures.push_back(Motion);
            textures.push_back(LIT);

            Simple3D::RenderTarget* GBUFFER = new Simple3D::RenderTarget();
            GBUFFER->AddTexture(Base);
            GBUFFER->AddTexture(Emmision);
            GBUFFER->AddTexture(Normal);
            GBUFFER->AddTexture(PBR);
            GBUFFER->AddTexture(Subsurf);
            GBUFFER->AddTexture(Motion);
            GBUFFER->depthTexture = renderer->CreateDepth(GBUFFER->GetExtent());

            Simple3D::RenderTarget* LitTarget = new Simple3D::RenderTarget();
            LitTarget->AddTexture(LIT);

            Simple3D::RenderTarget* ShadowMap = new Simple3D::RenderTarget();
            ShadowMap->depthTexture = renderer->CreateDepth(shadowResolution);

            renderTargets.push_back(GBUFFER);
            renderTargets.push_back(LitTarget);
            renderTargets.push_back(ShadowMap);

            graph->AddResource("GBUFFER", GBUFFER, false);
            graph->AddResource("LitTarget", LitTarget, false);
            graph->AddResource("ShadowMap", ShadowMap, false);
            graph->AddResource("EndTarget", endTarget, true);   // endTarget is external — not owned here

            GeometryPass* geometryPass = new GeometryPass(ResourceLocation, shaderSets["GBUFFER"], shaderSets["BILLBOARDGBUFFER"]);
            DefferedPass* defferedPass = new DefferedPass(ResourceLocation, GBUFFER, ShadowMap, shaderSets["Deffered"]);
            EnviromentPass* enviromentPass = new EnviromentPass(ResourceLocation, LitTarget, GBUFFER, ShadowMap, shaderSets["Enviroment"]);
            ShadowPass* shadowPass = new ShadowPass(ResourceLocation, shaderSets["Shadow"]);
            ForwardPass* forwardPass = new ForwardPass(ResourceLocation, shaderSets["Forward"]);

            shadowPass->outputResources.push_back("ShadowMap");

            geometryPass->outputResources.push_back("GBUFFER");

            defferedPass->inputResources.push_back("GBUFFER");
            defferedPass->inputResources.push_back("ShadowMap");
            defferedPass->outputResources.push_back("LitTarget");
            enviromentPass->inputResources.push_back("LitTarget");
            enviromentPass->outputResources.push_back("EndTarget");

            forwardPass->inputResources.push_back("EndTarget");
            forwardPass->outputResources.push_back("EndTarget");

            graph->AddPass(shadowPass);
            graph->AddPass(geometryPass);
            graph->AddPass(defferedPass);
            graph->AddPass(enviromentPass);
            graph->AddPass(forwardPass);

            renderer->BuildGraph(graph);
            return graph;
        }


        // ── buildFullDebugGraph ────────────────────────────────────────────
        Simple3D::RenderGraph* Renderer::buildFullDebugGraph(
            std::string name,
            Simple3D::RenderTarget* endTarget,
            std::string ResourceLocation)
        {
            Simple3D::RenderGraph* graph = renderer->CreateRenderGraph(name);

            Simple3D::RenderTexture* Base = renderer->CreateRenderTexture(window.GetWidth(), window.GetHeight(), VK_FORMAT_R8G8B8A8_SRGB);
            Simple3D::RenderTexture* Emmision = renderer->CreateRenderTexture(window.GetWidth(), window.GetHeight(), VK_FORMAT_R8G8B8A8_SRGB);
            Simple3D::RenderTexture* Normal = renderer->CreateRenderTexture(window.GetWidth(), window.GetHeight(), VK_FORMAT_R8G8B8A8_UNORM);
            Simple3D::RenderTexture* PBR = renderer->CreateRenderTexture(window.GetWidth(), window.GetHeight(), VK_FORMAT_R8G8B8A8_UNORM);
            Simple3D::RenderTexture* Subsurf = renderer->CreateRenderTexture(window.GetWidth(), window.GetHeight(), VK_FORMAT_R8G8B8A8_UNORM);
            Simple3D::RenderTexture* Motion = renderer->CreateRenderTexture(window.GetWidth(), window.GetHeight(), VK_FORMAT_R8G8B8A8_UNORM);
            Simple3D::RenderTexture* LIT = renderer->CreateRenderTexture(window.GetWidth(), window.GetHeight());

            textures.push_back(Base);
            textures.push_back(Emmision);
            textures.push_back(Normal);
            textures.push_back(PBR);
            textures.push_back(Subsurf);
            textures.push_back(Motion);
            textures.push_back(LIT);

            Simple3D::RenderTarget* GBUFFER = new Simple3D::RenderTarget();
            GBUFFER->AddTexture(Base);
            GBUFFER->AddTexture(Emmision);
            GBUFFER->AddTexture(Normal);
            GBUFFER->AddTexture(PBR);
            GBUFFER->AddTexture(Subsurf);
            GBUFFER->AddTexture(Motion);
            GBUFFER->depthTexture = renderer->CreateDepth(GBUFFER->GetExtent());

            Simple3D::RenderTarget* LitTarget = new Simple3D::RenderTarget();
            LitTarget->AddTexture(LIT);

            Simple3D::RenderTarget* ShadowMap = new Simple3D::RenderTarget();
            ShadowMap->depthTexture = renderer->CreateDepth(shadowResolution);

            renderTargets.push_back(GBUFFER);
            renderTargets.push_back(LitTarget);
            renderTargets.push_back(ShadowMap);

            graph->AddResource("GBUFFER", GBUFFER, false);
            graph->AddResource("LitTarget", LitTarget, false);
            graph->AddResource("ShadowMap", ShadowMap, false);
            graph->AddResource("EndTarget", endTarget, true);   // endTarget is external — not owned here

            GeometryPass* geometryPass = new GeometryPass(ResourceLocation, shaderSets["GBUFFER"], shaderSets["BILLBOARDGBUFFER"]);
            DefferedPass* defferedPass = new DefferedPass(ResourceLocation, GBUFFER, ShadowMap, shaderSets["Deffered"]);
            EnviromentPass* enviromentPass = new EnviromentPass(ResourceLocation, LitTarget, GBUFFER, ShadowMap, shaderSets["Enviroment"]);
            ShadowPass* shadowPass = new ShadowPass(ResourceLocation, shaderSets["Shadow"]);
            ForwardPass* forwardPass = new ForwardPass(ResourceLocation, shaderSets["Forward"]);
            DebugRenderPass* debugRenderPass = new DebugRenderPass(ResourceLocation, shaderSets["DebugDraw"], GBUFFER);

            shadowPass->outputResources.push_back("ShadowMap");

            geometryPass->outputResources.push_back("GBUFFER");

            defferedPass->inputResources.push_back("GBUFFER");
            defferedPass->inputResources.push_back("ShadowMap");
            defferedPass->outputResources.push_back("LitTarget");

            enviromentPass->inputResources.push_back("LitTarget");
            enviromentPass->outputResources.push_back("EndTarget");

            forwardPass->inputResources.push_back("EndTarget");
            forwardPass->outputResources.push_back("EndTarget");

            debugRenderPass->inputResources.push_back("EndTarget");
            debugRenderPass->inputResources.push_back("GBUFFER");
            debugRenderPass->outputResources.push_back("EndTarget");

            graph->AddPass(shadowPass);
            graph->AddPass(geometryPass);
            graph->AddPass(defferedPass);
            graph->AddPass(enviromentPass);
            graph->AddPass(forwardPass);
            graph->AddPass(debugRenderPass);

            renderer->BuildGraph(graph);
            return graph;
        }

        void Renderer::ApplyRenderSettings(const RenderSettings& next)
        {
            // ── HOT: render scale ─────────────────────────────────────────────
            if (next.renderScale != m_settings.renderScale)
            {
                m_RenderScale = next.renderScale;
            }

            // ── RELOAD_REQUIRED: pipeline rebuild ─────────────────────────────
            if (next.RequiresPipelineReload(m_settings))
            {
                m_PipelineReloadPending = true;
            }

            // ── HOT: shadow atlas size ─────────────────────────────────────────
            shadowResolution = { next.ShadowAtlasSize(), next.ShadowAtlasSize() };

            m_settings = next;
        }

        // ── RebuildPipelines ───────────────────────────────────────────────────
        // Tears down all intermediate render textures and graphs, then rebuilds
        // each view's graph using the same end-target it already owned.
        // The caller's RenderTarget pointers (swapchain, editor RT, etc.) are
        // never touched.
        void Renderer::RebuildPipelines()
        {
            // GPU must be idle before we destroy any Vulkan resources.
            renderer->WaitIdle();

            // ── 1. Destroy intermediate textures owned by old graphs ───────────
            // textures[] holds every RenderTexture created by buildFullGraph /
            // buildFullDebugGraph. The end-targets themselves are NOT in this
            // list (they are registered with external=true and not pushed here).
            for (auto* tex : textures)
                delete tex;
            textures.clear();

            // ── 2. Destroy intermediate render targets ────────────────────────
            // renderTargets[] holds GBUFFER, LitTarget and ShadowMap objects.
            // Again, end-targets are excluded.
            for (auto* rt : renderTargets)
                delete rt;
            renderTargets.clear();

            // ── 3. Destroy old graphs ─────────────────────────────────────────
            // Collect the set of graphs we need to destroy. Use a set so that
            // the shared mainGraph is only destroyed once even if multiple
            // camera views reference it.
            std::unordered_set<Simple3D::RenderGraph*> graphsToDestroy;

            if (mainGraph)
                graphsToDestroy.insert(mainGraph);

            for (RenderView& view : views)
                if (view.Graph) graphsToDestroy.insert(view.Graph);

            for (RenderView& view : NonECSviews)
                if (view.Graph) graphsToDestroy.insert(view.Graph);

            for (auto* g : graphsToDestroy)
                renderer->DestroyRenderGraph(g);

            mainGraph = nullptr;

            // ── 4. Rebuild one graph per unique end-target ────────────────────
            // Map each end-target to a freshly built graph so cameras that share
            // a target also share the new graph (mirrors the original behaviour
            // where the main camera reuses mainGraph).
            std::unordered_map<Simple3D::RenderTarget*, Simple3D::RenderGraph*> targetToNewGraph;

            auto getOrBuild = [&](Simple3D::RenderTarget* endTarget,
                const std::string& resourceLocation,
                bool debug) -> Simple3D::RenderGraph*
                {
                    auto it = targetToNewGraph.find(endTarget);
                    if (it != targetToNewGraph.end())
                        return it->second;

                    Simple3D::RenderGraph* newGraph = debug
                        ? buildFullDebugGraph("RebuildGraph_" + resourceLocation, endTarget, resourceLocation)
                        : buildFullGraph("RebuildGraph_" + resourceLocation, endTarget, resourceLocation);

                    targetToNewGraph[endTarget] = newGraph;
                    return newGraph;
                };

            // Rebuild ECS camera views.
            // We need the scene to know which cameras are in debug mode; fall
            // back to non-debug if the scene is unavailable.
            Scene* scene = SceneManager::GetInstance().GetScene();

            for (RenderView& view : views)
            {
                bool debug = false;
                if (scene && view.ecsEntity != entt::null && scene->registry.valid(view.ecsEntity))
                {
                    // If you have a debug-draw flag on the Camera component, read it here.
                    // e.g. debug = scene->registry.get<Camera>(view.ecsEntity).debugDraw;
                }

                Simple3D::RenderGraph* newGraph =
                    getOrBuild(view.target, view.ResourceLocation, debug);

                view.Graph = newGraph;

                // Sync back to the Camera component so CollectECSViews doesn't
                // immediately rebuild on the next frame.
                if (scene && view.ecsEntity != entt::null && scene->registry.valid(view.ecsEntity))
                {
                    auto& cam = scene->registry.get<Camera>(view.ecsEntity);
                    cam.graph = newGraph;

                    // Promote to mainGraph if this is the main camera.
                    if (cam.isMainCamera && view.target == mainTarget)
                        mainGraph = newGraph;
                }
            }

            // Rebuild non-ECS views.
            for (RenderView& view : NonECSviews)
            {
                view.Graph = getOrBuild(view.target, view.ResourceLocation, false);
            }

            m_PipelineReloadPending = false;
        }

        void Renderer::CollectECSViews(Scene& scene)
        {
            std::vector<GameObject> objects = scene.GetGameObjectsWithComponent<Camera>();

            for (auto obj : objects)
            {
                auto& cam = obj.GetComponent<Camera>();

                RenderView rv{};
                rv.camera = &cam;
                rv.ecsEntity = obj.entity;
                rv.target = cam.CameraTarget ? cam.CameraTarget : mainTarget;

                if (!cam.graph) {
                    if (cam.isMainCamera) {
                        if (mainGraph == nullptr) {
                            // Build Main Render Graph
                            mainGraph = buildFullGraph("MainRenderGraph", mainTarget, "MainResource");
                        }

                        cam.graph = mainGraph;
                        cam.ResourceLocation = "MainResource";
                    }
                    else {
                        cam.graph = buildFullGraph("MainRenderGraph", mainTarget, GenerateResourceName(cam, obj.entity));;
                        cam.ResourceLocation = GenerateResourceName(cam, obj.entity);
                    }
                }

                rv.Graph = cam.graph;
                rv.ResourceLocation = cam.ResourceLocation;

                views.push_back(rv);
            }
        }

        std::string Renderer::GenerateResourceName(Camera& camera, entt::entity cameraObj)
        {
            // If the camera already has a resource name, reuse it
            if (!camera.ResourceLocation.empty())
                return camera.ResourceLocation;

            // Use entity-backed ID if available
            // (Assumes Camera stores its owning entity or a UUID)
            if (cameraObj != entt::null)
            {
                return "CameraResources_" + std::to_string(
                    static_cast<uint32_t>(cameraObj)
                );
            }

            // Fallback for non-ECS cameras
            static uint32_t NonECSCameraCounter = 0;
            return "ExternalCameraResources_" + std::to_string(NonECSCameraCounter++);
        }


        // gather and cull drawdata
        void Renderer::Update()
        {
            Scene* scene = SceneManager::GetInstance().GetScene();
            if (!scene) return;

            AnimationSystem::Update(*scene, TimeManager::getInstance().deltaTime);

            std::vector<ParticleSystem*> particleSystems;
            auto particleView = scene->registry.view<ParticleSystem>();
            particleView.each([&](entt::entity entity, ParticleSystem& system) {
                GameObject obj = GameObject(scene, entity);
                system.WorldPos = obj.GetWorldTransform().GetPosition();

                // Resolve mesh assets for each emitter
                for (Emmiter& e : system.emmiters) {
                    if (e.meshAsset.valid && e.meshData == nullptr) {
                        auto* meshAsset = AssetPool::Instance().GetAssetTyped<MeshAsset>(e.meshAsset);
                        if (meshAsset) {
                            e.meshData = new Simple3D::Model(
                                meshAsset->mesh.vertices,
                                meshAsset->mesh.indices,
                                false
                            );
                        }
                    }
                    else if (!e.meshAsset.valid && e.meshData != nullptr) {
                        delete e.meshData;
                        e.meshData = nullptr;
                    }
                }

                particleSystems.push_back(&system);
                });

            particleContoller.Update(TimeManager::getInstance().deltaTime, particleSystems);
        }


        void Renderer::Render()
        {
            PROFILE_SCOPE("Render");

            // ── Pipeline reload (deferred from ApplyRenderSettings) ───────────
            // Must run before CollectECSViews so the freshly built graphs are
            // handed to the views that are about to be gathered.
            if (m_PipelineReloadPending)
            {
                RebuildPipelines();
                // m_PipelineReloadPending is cleared inside RebuildPipelines().
            }

            views.clear();
            NormalModels.clear();
            TrasparentModels.clear();

            Scene* scene = SceneManager::GetInstance().GetScene();
            if (!scene) return;

            renderLoader->FlushPendingUploads();
            UpdateModels(scene);
            CollectECSViews(*scene);
            m_TexturePool->Update();


            //if (m_MaterialBuffer) m_MaterialBuffer->Flush(preFrameCmd);

            {
                // Render offscreen / editor / RT cameras first
                for (RenderView& view : views)
                {
                    if (view.target != mainTarget)
                        Render(view);
                }

                // Render main camera last
                for (RenderView& view : views)
                {
                    if (view.target == mainTarget)
                        Render(view);
                }


                // Render Non ECS cameras
                for (RenderView& view : NonECSviews)
                {
                    Render(view);
                }
            }

            renderer->Render(frameData);
        }


        void Renderer::Render(RenderView& view) {
            view.Resources.Clear();

            Scene* scene = SceneManager::GetInstance().GetScene();
            if (!scene) return;

            entt::registry& registry = scene->registry;

            frameData.FrameResouces[view.ResourceLocation] = &view.Resources;
            view.Resources.TexturePool = m_TexturePool.get();

            // --- Camera setup ---
            // Switched to World Position and World Rotation
            if (view.ecsEntity != entt::null && registry.valid(view.ecsEntity)) {
                auto transform = GameObject(scene, view.ecsEntity).GetWorldTransform();
                view.camera->camera.setPosition(transform.GetPosition().ToGLM());

                view.camera->camera.setRotationQuat(transform.GetRotationQuat());

            }
            view.Resources.camera = &view.camera->camera;
            if (!view.camera->CameraTarget) view.camera->CameraTarget = view.target;

            glm::mat4 vp = view.camera->camera.getProjectionMatrix(
                view.camera->CameraTarget->GetExtent().width,
                view.camera->CameraTarget->GetExtent().height) *
                view.camera->camera.getViewMatrix();

            glm::mat4 prevVP = PrevViewProjByResource.count(view.ResourceLocation)
                ? PrevViewProjByResource[view.ResourceLocation] : vp;
            PrevViewProjByResource[view.ResourceLocation] = vp;
            view.Resources.prevViewProj = prevVP;

            view.frustum = ExtractFrustum(vp);
            view.camera->CameraFrustum = view.frustum;
            view.camera->FrustumUpToDate = true;
            view.frustumDirty = false;
            view.Resources.viewFrustum = view.frustum;
            float aspect = view.camera->CameraTarget->GetExtent().width / view.camera->CameraTarget->GetExtent().height;

            // --- Skybox Logic ---
            auto skyboxView = registry.view<SkyboxComponent, EntityName>();
            skyboxView.each([&](auto entity, SkyboxComponent& skybox, const EntityName& entityName) {
                aliveCubes.insert(entityName.ID);
                if (skybox.dirty) {
                    auto it = cubeMaps.find(entityName.ID);
                    if (it != cubeMaps.end()) delete it->second;

                    if (skybox.faces.size() == 6) {
                        std::vector<std::string> facePaths;
                        for (auto& face : skybox.faces) {
                            TextureAsset* tex = AssetPool::Instance().GetAssetTyped<TextureAsset>(face.second);
                            if (tex) facePaths.push_back(tex->FilePath);
                        }
                        if (facePaths.size() == 6) {
                            cubeMaps[entityName.ID] = renderer->CreateTextureCube(facePaths);
                            skybox.valid = true;
                        }
                    }
                    else skybox.valid = false;
                    skybox.dirty = false;
                }
                });

            // Best Skybox Selection
            Guid bestSky;
            float minD = std::numeric_limits<float>::max();
            // Camera position is now derived from World Position in the setup block above
            glm::vec3 camPos = view.camera->camera.getPosition();

            skyboxView.each([&](auto entity, SkyboxComponent& skybox, const EntityName& entityName) {
                if (!skybox.valid) return;
                float d = GetSqDistanceToCenter(camPos, skybox.Bounds);
                if (skybox.useBounds) {
                    if (IsInsideAABB(camPos, skybox.Bounds) && d < minD) {
                        minD = d;
                        bestSky = entityName.ID;
                    }
                }
                else if (d < minD) {
                    minD = d;
                    bestSky = entityName.ID;
                }
                });

            view.Resources.SkyboxCube = (bestSky != Guid()) ? cubeMaps[bestSky] : nullptr;

            for (auto it = cubeMaps.begin(); it != cubeMaps.end();) {
                if (aliveCubes.find(it->first) == aliveCubes.end()) {
                    delete it->second;
                    it = cubeMaps.erase(it);
                }
                else ++it;
            }

            // --- Lighting ---
            std::vector<GameObject> objects = scene->GetGameObjectsWithComponent<Light>();
            uint32_t currentShadowIndex = 0;


            int tilesPerSide = std::ceil(std::sqrt(static_cast<float>(m_settings.maxShadowLights)));
            float scale = 1.0f / static_cast<float>(tilesPerSide);

            for (auto obj : objects) {
                Light& light = obj.GetComponent<Light>();
                auto transform = obj.GetWorldTransform();

                glm::vec3 pos = transform.GetPosition();
                glm::vec3 dir = transform.Forward();
                glm::vec3 up = transform.Up();

                light.position = glm::vec4(pos, 1.0f);
                light.Direction = glm::vec4(dir, 0.0f);

                // Determine how many slots this light needs
                int slotsNeeded = (light.type == LightType::Point) ? 6 : 1;

                if (light.castShadows > 0 && (currentShadowIndex + slotsNeeded) <= maxShadowLights) {
                    // Reserve the starting index for the atlas
                    light.shadowIndex = currentShadowIndex;
                    currentShadowIndex += slotsNeeded;

                    // Calculate basic atlas mapping (Fragment shader uses this for the START of the light's region)
                    int row = light.shadowIndex / tilesPerSide;
                    int col = light.shadowIndex % tilesPerSide;
                    light.atlasOffset = glm::vec2(static_cast<float>(col) * scale, static_cast<float>(row) * scale);
                    light.atlasScale = scale;

                    if (light.type == LightType::Point) {
                        // --- POINT LIGHT: 6 Faces ---
                        glm::vec3 targets[6] = {
                            glm::vec3(1, 0, 0), glm::vec3(-1, 0, 0), // +X, -X
                            glm::vec3(0, 1, 0), glm::vec3(0, -1, 0), // +Y, -Y
                            glm::vec3(0, 0, 1), glm::vec3(0, 0, -1)  // +Z, -Z
                        };
                        glm::vec3 ups[6] = {
                            glm::vec3(0, -1, 0), glm::vec3(0, -1, 0),
                            glm::vec3(0, 0, -1), glm::vec3(0, 0, 1),
                            glm::vec3(0, -1, 0), glm::vec3(0, -1, 0)
                        };

                        for (int f = 0; f < 6; ++f) {
                            glm::mat4 view = glm::lookAt(pos, pos + targets[f], ups[f]);
                            glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);
                            proj[1][1] *= -1; // Vulkan Y-Flip
                            light.lightSpaceMatrices[f] = proj * view;
                        }
                    }
                    else {
                        // --- SPOT / DIRECTIONAL: 1 Face ---
                        glm::mat4 lightView = glm::lookAt(pos, pos + dir, up);
                        glm::mat4 lightProj;

                        if (light.type == LightType::Directional) {
                            lightProj = glm::ortho(-20.0f, 20.0f, -20.0f, 20.0f, 1.0f, 100.0f);
                        }
                        else {
                            float fov = glm::acos(light.outerAngle) * 2.0f;
                            lightProj = glm::perspective(fov, 1.0f, 0.1f, 100.0f);
                        }

                        lightProj[1][1] *= -1;
                        light.lightSpaceMatrices[0] = lightProj * lightView;
                    }
                }
                else {
                    light.shadowIndex = -1;
                    light.atlasOffset = glm::vec2(0.0f);
                    light.atlasScale = 0.0f;
                }

                view.Resources.lights.push_back(light);
                view.Resources.LightDataChanged = true;
            }


            auto windView = registry.view<WindSource>();
            if (!windView.empty()) {
                auto entity = windView.front();
                view.Resources.wind = registry.get<WindSource>(entity);
            }
            else {
                view.Resources.wind.windDirection = glm::vec3(1, 0, 0);
                view.Resources.wind.windSpeed = 0.0f;
            }

            auto volumeView = registry.view<VolumeComponent>();
            volumeView.each([&](auto entity, VolumeComponent& volume) {
                auto transform = GameObject(scene, entity).GetWorldTransform();
                VolumeComponent worldVolume = volume;

                // Updated to use World Position
                worldVolume.boundingBox.center = transform.GetPosition().ToGLM() + volume.boundingBox.center;

                glm::vec3 vMin = worldVolume.boundingBox.center - worldVolume.boundingBox.extents;
                glm::vec3 vMax = worldVolume.boundingBox.center + worldVolume.boundingBox.extents;

                if (FrustumIntersectsAABB(view.frustum, worldVolume.boundingBox)) {
                    view.Resources.volumes.push_back(worldVolume);
                }
                });

            // Push Models
            view.Resources.NormalModels = NormalModels;
            view.Resources.TrasparentModels = TrasparentModels;

            view.Resources.SkinnedBonePalettes = SkinnedBonePalettes;
            view.Resources.ModelBonePaletteIndices = ModelBonePaletteIndices;
            view.Resources.PrevTransforms = ModelPrevTransformsThisFrame;


            frameData.FrameResouces[view.ResourceLocation] = &view.Resources;
            // DEBUG DRAW

            // Lights
            for (const Light& light : view.Resources.lights)
            {
                glm::vec3 pos = glm::vec3(light.position);
                glm::vec3 dir = glm::vec3(light.Direction);

                if (light.type == LightType::Spot)
                {
                    DebugDrawSpotlight(pos, dir,
                        light.radius,
                        light.outerAngle,
                        light.innerAngle);
                }
                else if (light.type == LightType::Directional)
                {
                    DebugDrawDirectionalLight(pos, dir);
                }
            }


            // Get particle rendering info
            view.Resources.particles = particleContoller.GetRender();


            // Volumes
            auto dbgVolView = registry.view<VolumeComponent>();
            dbgVolView.each([](auto, const VolumeComponent& vol) {
                DebugDrawVolumeBounds(vol);
                });

            view.Resources.settings = m_settings;
        }

        void Renderer::UpdateModels(Scene* scene) {
            entt::registry& registry = scene->registry;

            // Clear out our runtime dynamic layout frame state tracking cache maps 
            // before re-gathering live entities.
            aliveObjects.clear();
            NormalModels.clear();
            TrasparentModels.clear();
            SkinnedBonePalettes.clear();
            ModelBonePaletteIndices.clear();
            ModelPrevTransformsThisFrame.clear();

            auto modelView = registry.view<Model, Transform, EntityName>();

            modelView.each([&](auto entity, Model& model, Transform& localTransform, const EntityName& entityName) {
                if (!model.ModelAsset.valid) return;

                if (!model.isStatic)
                    aliveObjects.insert(entityName.ID);

                if (ModelChange(model, localTransform)) {
                    UpdateMesh(model, entityName.ID);
                    UpdateMeshMaterial(model);
                }

                // Transform calculation
                glm::mat4 worldMat = localTransform.GetMatrix();
                entt::entity parent = entityName.parent;
                while (parent != entt::null && registry.valid(parent)) {
                    worldMat = registry.get<Transform>(parent).GetMatrix() * worldMat;
                    parent = registry.get<EntityName>(parent).parent;
                }

                // NEW: snapshot last frame's transform before we overwrite it
                glm::mat4 prevTransform = PrevTransforms.count(&model) ? PrevTransforms[&model] : worldMat;
                PrevTransforms[&model] = worldMat;
                ModelPrevTransformsThisFrame[&model] = prevTransform;

                model.Transform = worldMat;
                model.hasRendered = true;

                // ── NEW SKELETAL PALETTE PACKAGING ───────────────────────────────────
                // Check if this renderable mesh entity has an active animation component
                if (registry.all_of<AnimatorComponent>(entity)) {
                    auto& animator = registry.get<AnimatorComponent>(entity);

                    // Record the bone matrices array inside our linear storage block
                    uint32_t paletteIdx = static_cast<uint32_t>(SkinnedBonePalettes.size());
                    SkinnedBonePalettes.push_back(animator.pallette);

                    // Map this model's reference pointer to its corresponding array slot
                    ModelBonePaletteIndices[&model] = paletteIdx;
                }

                if (model.isTransparent)
                    TrasparentModels.push_back(&model);
                else
                    NormalModels.push_back(&model);
                });


            // Model Cleanup
            for (auto it = DynamicModels.begin(); it != DynamicModels.end();) {
                if (aliveObjects.find(it->first) == aliveObjects.end()) {
                    delete it->second;
                    it = DynamicModels.erase(it);
                }
                else ++it;
            }
        }

        void Renderer::UpdateMesh(Model& model, Guid Object) {
            auto* meshAsset = AssetPool::Instance().GetAssetTyped<MeshAsset>(model.ModelAsset);
            if (!meshAsset) return;


            if (model.Dirty || model.meshData == nullptr) {
                model.boundingBox.valid = false;
                if (model.isStatic) {
                    if (AssetModels.find(model.ModelAsset) != AssetModels.end()) {
                        model.meshData = AssetModels[model.ModelAsset];
                    }
                    else {
                        model.meshData = nullptr;

                        Simple3D::Model* newModel = new Simple3D::Model(meshAsset->mesh.vertices, meshAsset->mesh.indices, !model.isStatic);
                        AssetModels[model.ModelAsset] = newModel;
                        model.meshData = newModel;
                    }
                }
                else {
                    model.meshData = nullptr;

                    Simple3D::Model* newModel = new Simple3D::Model(meshAsset->mesh.vertices, meshAsset->mesh.indices, !model.isStatic);
                    DynamicModels[Object] = newModel;
                    model.meshData = newModel;
                }

                model.MaterialDirty = true;
                model.Dirty = false;
                model.DynamicData = meshAsset->mesh;
                model.TexturesDirty = true;
            }

            if (!model.isStatic && model.MeshChanged) {
                model.meshData->UpdateVerticies(model.DynamicData.vertices);
            }
            else if (model.isStatic && !model.boundingBox.valid) {
                model.boundingBox = CreateAABB(model.meshData->Verticies, model.meshData->Indices);
                model.boundingBox.valid = true;
            }
        }

        void Renderer::UpdateMeshMaterial(Model& model)
        {
            if (!model.meshData) return;

            // ── Resolve textures into bindless IDs ────────────────────────────
            if (model.TexturesDirty)
            {
                auto& gm = model.solidMaterial;

                // Helper lambda: look up a texture asset, register it in the
                // bindless pool if not already present, return its pool index.
                auto ResolveTexture = [&](const char* key, bool& resolved) -> uint32_t {
                    auto it = model.textureAssets.find(key);
                    if (it == model.textureAssets.end() || !it->second.valid)
                        return UINT32_MAX;

                    if (!m_TexturePool->HasTexture(it->second))
                        m_TexturePool->AddTexture(it->second);

                    // GetTextureID throws if not registered yet — it won't be until Update() runs
                    // so return UINT32_MAX this frame, it'll resolve next frame
                    if (!m_TexturePool->HasTexture(it->second)) {
                        resolved = false;
                        return UINT32_MAX;
                    }

                    return m_TexturePool->GetTextureID(it->second);
                    };
                bool allResolved = true;

                gm.albedoTex = ResolveTexture("albedoTex", allResolved);
                gm.normalTex = ResolveTexture("normalTex", allResolved);
                gm.roughnessTex = ResolveTexture("roughnessTex", allResolved);
                gm.metallicTex = ResolveTexture("metallicTex", allResolved);
                gm.aoTex = ResolveTexture("aoTex", allResolved);
                gm.sheenTex = ResolveTexture("sheenTex", allResolved);
                gm.sheenTintTex = ResolveTexture("sheenTintTex", allResolved);
                gm.anisotropyTex = ResolveTexture("anisotropyTex", allResolved);
                gm.subsurfTintTex = ResolveTexture("subsurfTintTex", allResolved);
                gm.emissiveTex = ResolveTexture("emissiveTex", allResolved);
                gm.subsurfRadiusTex = ResolveTexture("subsurfRadiusTex", allResolved);
                gm.specStrengthTex = ResolveTexture("specStrengthTex", allResolved);
                gm.clearcoatTex = ResolveTexture("clearcoatTex", allResolved);
                gm.clearcoatGlossTex = ResolveTexture("clearcoatGlossTex", allResolved);
                gm.specTintTex = ResolveTexture("specTintTex", allResolved);
                gm.paralaxOcclusion = ResolveTexture("paralaxOcclusion", allResolved);

                model.TexturesDirty = !allResolved; // retry next frame if any pending
                model.MaterialDirty = true;
            }
        }

        bool Renderer::ModelChange(const Model& model, const Transform& transform) {
            return model.Dirty ||
                model.MaterialDirty ||
                model.TexturesDirty ||
                model.materialIndex == UINT32_MAX ||
                !model.meshData;
        }


        // Change enviroment data on scene change
        void Renderer::OnSceneChange() {
            Scene* scene = SceneManager::GetInstance().GetScene();
        }




        // Update on window resize
        void Renderer::OnWindowResize() {
            renderer->WindoResize();
        }


        void Renderer::ReloadShaders() {
            for (RenderView view : views) {
                view.Graph->Compile();
            }
        }

        // Adds a secondary rendergraph with a corrosponding camera and rendertarget
        void Renderer::AddNonECSCamera(Camera* camera, Simple3D::RenderTarget* target, Simple3D::RenderGraph* graph, std::string ResourceLocation) {
            RenderView newView;

            newView.camera = camera;
            newView.Graph = graph;
            newView.target = target;
            newView.ResourceLocation = ResourceLocation;

            NonECSviews.push_back(newView);
        }

        // render an object once to an image (for thumbnails etc)
        void RenderToImage(Simple3D::RenderTarget* target, Simple3D::Camera* camera, Simple3D::Model model, Simple3D::RenderGraph* graph) {

        }


        // Initalise Imgui (For editor Applications)
        ImGuiContext* Renderer::initImgui(std::function<void(ImGuiStyle&)> styleConfig) {
            renderer->initImgui(styleConfig);
            return ImGui::GetCurrentContext();
        }

        bool Renderer::NewImguiFrame() {
            if (SDL_GetWindowFlags(window.GetSdlWindow()) & SDL_WINDOW_MINIMIZED)
                return false;

            if (!ImGui::GetCurrentContext()) { DIV_ERROR("ContextInit Failed"); return false; }
            if (!renderer->NewImguiframe()) return false;

            return true;
        }


        //      ----------------------------------------------------------
        //      |                 RENDER LAYER                           |
        //      ----------------------------------------------------------


        RenderLayer::RenderLayer(Window& window, std::string ShadersDirectory, Simple3D::RenderTarget* target)
        {
            renderer = new Renderer(window, ShadersDirectory, target);
            renderer->Initialize();
        }

        void RenderLayer::OnAttach()
        {
        }

        void RenderLayer::OnUpdate()
        {
            renderer->Update();
        }

        void RenderLayer::OnEvent(const DiversityEvents::BaseEvent& event)
        {
            if (event.type == DiversityEvents::EventType::Window)
            {
                const DiversityEvents::WindowEvent& windowEvent = static_cast<const DiversityEvents::WindowEvent&>(event);

                if (windowEvent.windowEventType == DiversityEvents::WindowEvent::WindowEventType::Resized) {
                    renderer->OnWindowResize();
                }
            }
        }

        void RenderLayer::OnNewScene() {
            renderer->OnSceneChange();
        }

        void RenderLayer::OnRender()
        {
            renderer->Render();
        }

        void RenderLayer::OnDetach()
        {
            delete renderer;
        }

        Renderer& RenderLayer::GetRenderer()
        {
            return *renderer;
        }
    }
}