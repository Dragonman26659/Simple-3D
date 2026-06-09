#pragma once
#include "Component/Renderable/Model.h"
#include "Internal/RTcontext.h"

namespace Simple3D {

    // ── BuildBLAS ─────────────────────────────────────────────────────────────────
    // Builds a Bottom-Level Acceleration Structure for a Model's triangle geometry.
    //
    // Rules:
    //   • ctx.IsAvailable() must be true — call is a no-op otherwise (returns {}).
    //   • The model's vertex + index buffers must already exist (CreateBuffers called).
    //   • The returned BLAS owns its memory; call DestroyBLAS when the mesh is unloaded.
    //   • Static meshes: call once, reuse forever.
    //   • Dynamic/skinned meshes: rebuild every frame — pass the same BLAS in to
    //     refit cheaply using PREFER_FAST_BUILD + UPDATE flags.
    //
    BLAS BuildBLAS(Model& model,
        RayTracingContext& ctx,
        VkCommandPool* pool,
        bool allowUpdate = false);

    // Destroys the backing buffer and AS handle.
    void DestroyBLAS(BLAS& blas, RayTracingContext& ctx);

    // Refit (update in place) a BLAS after vertex positions changed.
    // Much cheaper than a full rebuild; only valid if allowUpdate was true at build time.
    void RefitBLAS(BLAS& blas,
        Model& model,
        RayTracingContext& ctx,
        VkCommandPool* pool);

} // namespace Simple3D