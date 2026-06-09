#include "Renderer.h"
#include "Internal/RTcontext.h"
#include "Internal/RT/RTModel.h"

namespace Simple3D {

    // ── EnableRayTracing ──────────────────────────────────────────────────────────
    bool Renderer::EnableRayTracing()
    {
        if (m_RTEnabled) return m_RTCtx.IsAvailable();

        m_RTCtx.Init(instance,
            RenderDevice->getPhysicalDevice(),
            RenderDevice->getLogicalDevice());

        m_RTEnabled = true;
        return m_RTCtx.IsAvailable();
    }

    // ── BuildModelBLAS ────────────────────────────────────────────────────────────
    BLAS Renderer::BuildModelBLAS(Model& model, bool allowUpdate)
    {
        if (!m_RTCtx.IsAvailable()) return {};

        return BuildBLAS(model, m_RTCtx, &commandPool, allowUpdate);
    }

    // ── RefitModelBLAS ────────────────────────────────────────────────────────────
    void Renderer::RefitModelBLAS(BLAS& blas, Model& model)
    {
        if (!m_RTCtx.IsAvailable() || !blas.IsValid()) return;

        RefitBLAS(blas, model, m_RTCtx, &commandPool);
    }

    // ── DestroyModelBLAS ──────────────────────────────────────────────────────────
    void Renderer::DestroyModelBLAS(BLAS& blas)
    {
        DestroyBLAS(blas, m_RTCtx);
    }

    // ── BuildSceneTLAS ────────────────────────────────────────────────────────────
    void Renderer::BuildSceneTLAS(TLAS& tlas,
        const std::vector<TLASInstance>& instances)
    {
        if (!m_RTCtx.IsAvailable()) return;

        BuildTLAS(tlas, instances, m_RTCtx, &commandPool);
    }

    // ── DestroySceneTLAS ──────────────────────────────────────────────────────────
    void Renderer::DestroySceneTLAS(TLAS& tlas)
    {
        DestroyTLAS(tlas, m_RTCtx);
    }

} // namespace Simple3D