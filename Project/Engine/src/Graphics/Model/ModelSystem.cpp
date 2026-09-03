#include "pch.h"
#include "Graphics/Model/ModelSystem.hpp"
#include "ECS/ECSRegistry.hpp"
#include "ECS/ActiveComponent.hpp"
#include <Graphics/Model/ModelRenderComponent.hpp>
#include "WindowManager.hpp"
#include "Graphics/GraphicsManager.hpp"
#include <Transform/TransformComponent.hpp>
#include "Asset Manager/AssetManager.hpp"
#include "Asset Manager/ResourceManager.hpp"
#include "Logging.hpp"
#include "ECS/LayerComponent.hpp"
#include "Graphics/PostProcessing/PostProcessingManager.hpp"
#include "Graphics/BloomComponent.hpp"
#include "ECS/TagComponent.hpp"
#include "ECS/TagManager.hpp"
#include "Graphics/Camera/Camera.hpp"
#include "Graphics/Camera/CameraComponent.hpp"
#include "Graphics/Camera/CameraSystem.hpp"
#include "Graphics/Lights/LightingSystem.hpp"

#ifdef ANDROID
#include <android/log.h>
#endif
#include <Graphics/Model/ModelFactory.hpp>
#include <Graphics/Instancing/InstancingManager.hpp>

namespace {
	constexpr std::size_t kMaxShaderBones = 100;

	void InitialiseBindPoseMatrices(ModelRenderComponent& modelComponent)
	{
		if (!modelComponent.model || modelComponent.model->mBoneInfoMap.empty()) {
			modelComponent.mFinalBoneMatrices.clear();
			return;
		}

		std::size_t matrixCount = 0;
		for (const auto& [name, boneInfo] : modelComponent.model->mBoneInfoMap) {
			(void)name;
			if (boneInfo.id >= 0) {
				matrixCount = std::max(
					matrixCount,
					static_cast<std::size_t>(boneInfo.id) + 1);
			}
		}

		matrixCount = std::min(matrixCount, kMaxShaderBones);
		modelComponent.mFinalBoneMatrices.assign(matrixCount, glm::mat4(1.0f));
	}
}

bool ModelSystem::Initialise() 
{
    // DISABLE COLOR AND DEPTH WRITES BEFORE DUMMY DRAWING!
    // This stops exploding vertices from rendering to the editor viewport.
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glDepthMask(GL_FALSE);

	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
#ifdef ANDROID
	const std::string defaultShaderPath =
		ResourceManager::GetPlatformShaderPath("default");
	const std::shared_ptr<Shader> defaultShader =
		ResourceManager::GetInstance().GetResource<Shader>(defaultShaderPath);
	const std::string opaqueShaderPath =
		ResourceManager::GetPlatformShaderPath("defaultopaque");
	const std::shared_ptr<Shader> opaqueShader =
		ResourceManager::GetInstance().GetResource<Shader>(opaqueShaderPath);
#endif
	for (const auto& entity : entities) {
        auto& modelComp = ecsManager.GetComponent<ModelRenderComponent>(entity);
        ENGINE_LOG_DEBUG("Loading model");
        std::string modelPath = AssetManager::GetInstance().GetAssetPathFromGUID(modelComp.modelGUID);
        if (!modelPath.empty())
            modelComp.model = ResourceManager::GetInstance().GetResourceFromGUID<Model>(modelComp.modelGUID, modelPath);
#ifndef ANDROID
        std::string shaderPath = AssetManager::GetInstance().GetAssetPathFromGUID(modelComp.shaderGUID);
        if (!shaderPath.empty())
            modelComp.shader = ResourceManager::GetInstance().GetResourceFromGUID<Shader>(modelComp.shaderGUID, shaderPath);
#else
		ENGINE_LOG_DEBUG("Loading shader");
		modelComp.shader = defaultShader;
#endif
        ENGINE_LOG_DEBUG("Loading material");
        std::string materialPath = AssetManager::GetInstance().GetAssetPathFromGUID(modelComp.materialGUID);
        if (!materialPath.empty()) {
            modelComp.material = ResourceManager::GetInstance().GetResourceFromGUID<Material>(modelComp.materialGUID, materialPath);
        }

        if (modelComp.model) {
			// Static models need no skeletal storage. Allocate only the matrices
			// required by models that actually contain bones.
			InitialiseBindPoseMatrices(modelComp);
            ModelFactory::PopulateBoneNameToEntityMap(entity, modelComp.boneNameToEntityMap, *modelComp.model, true);
            modelComp.childBonesSaved = true;

            // Force shader compilation / activation
            modelComp.shader->Activate();

            // Force textures to page into VRAM
			if (modelComp.material) {
				modelComp.material->ApplyToShader(*modelComp.shader);
			}

#ifdef ANDROID
			// A shader containing discard can inhibit opaque early-depth writes even
			// when sampled alpha is always one. Most mobile diffuse KTX files are
			// ETC2 RGB, so render them with a compile-time no-discard permutation.
			bool requiresDiffuseAlphaTest = false;
			if (modelComp.material) {
				requiresDiffuseAlphaTest =
					modelComp.material->RequiresDiffuseAlphaTest();
			}
			else {
				for (const Mesh& mesh : modelComp.model->meshes) {
					if (mesh.material &&
						mesh.material->RequiresDiffuseAlphaTest()) {
						requiresDiffuseAlphaTest = true;
						break;
					}
				}
			}

			if (!requiresDiffuseAlphaTest && opaqueShader) {
				modelComp.shader = opaqueShader;
				modelComp.shader->Activate();
				if (modelComp.material) {
					modelComp.material->ApplyToShader(*modelComp.shader);
				}
			}
#endif

			for (auto& mesh : modelComp.model->meshes)
            {
                // This calls your setupMesh() and sets vaoSetup = true
                // while the loading screen is still up!
                mesh.Prewarm();

                // THE DUMMY DRAW: Force the driver to execute the pipeline!
                // We only draw 3 indices (1 triangle) to make it lightning fast.
                mesh.DrawPrewarmTriangle();
                mesh.vao.Unbind();
#ifdef ANDROID
				mesh.ReleaseCPUVertexData();
#endif
            }
        }
    }

    // Restore normal graphics state for gameplay
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE); // Or whatever your default is

    ENGINE_PRINT("[ModelSystem] Dummy Draw Prewarm complete! Driver is fully warmed up.\n");

    ENGINE_PRINT("[ModelSystem] Initialized\n");
    return true;
}

void ModelSystem::Update()
{
    PROFILE_FUNCTION(); // Will automatically show as "Model" in profiler UI

    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    GraphicsManager& gfxManager = GraphicsManager::GetInstance();
    InstancingManager& instancing = InstancingManager::GetInstance();

    // Get current view mode and check if rendering for editor
    bool isRenderingForEditor = gfxManager.IsRenderingForEditor();
    bool is3DMode = gfxManager.Is3DMode();
    Camera* currentCamera = gfxManager.GetCurrentCamera();

    float cameraFadeNear = 3.0f;
    float cameraFadeFar = 5.0f;
    if (!isRenderingForEditor && currentCamera && ecsManager.cameraSystem) {
        const Entity activeCameraEntity = ecsManager.cameraSystem->GetActiveCameraEntity();
        if (auto cameraComponent = ecsManager.TryGetComponent<CameraComponent>(activeCameraEntity)) {
            cameraFadeNear = cameraComponent->get().fadeNear;
            cameraFadeFar = cameraComponent->get().fadeFar;
        }
    }

    static const int tagPlayer = TagManager::GetInstance().GetTagIndex("Player");
    static const int tagNPC = TagManager::GetInstance().GetTagIndex("NPC");
    static const int tagCollectible = TagManager::GetInstance().GetTagIndex("Collectible");
    static const int tagNoCameraCollision = TagManager::GetInstance().GetTagIndex("NoCameraCollision");

    // Get frustum for culling
    const Frustum& frustum = gfxManager.GetFrustum();
    bool enableCulling = gfxManager.IsFrustumCullingEnabled() && !isRenderingForEditor;
    // Reset stats each frame
    cullingStats.Reset();
    const uint32_t excludedLayerMask = PostProcessingManager::GetInstance().GetExcludedLayerMask();
    renderSnapshots.reserve(entities.size());
    std::size_t renderSnapshotCount = 0;


    // Submit all visible models to the graphics manager
    for (const auto& entity : entities)
    {
        // Skip all 3D models in 2D mode ONLY when rendering for editor
        // Game window should always show all models
        if (isRenderingForEditor && !is3DMode) {
            continue;
        }

        // Skip entities that are inactive in hierarchy (checks parents too)
        if (!ecsManager.IsEntityActiveInHierarchy(entity)) {
            continue;
        }

        auto& modelComponent = ecsManager.GetComponent<ModelRenderComponent>(entity);

        if (!modelComponent.isVisible || !modelComponent.model || !modelComponent.shader)
        {
            continue;
        }

        const Transform& entityTransform = ecsManager.GetComponent<Transform>(entity);
        const glm::mat4 glmWorldMatrix = entityTransform.worldMatrix.ConvertToGLM();
        auto bloomComponent = ecsManager.TryGetComponent<BloomComponent>(entity);

		if (modelComponent.cachedBoundsModel != modelComponent.model.get() ||
			modelComponent.cachedBoundsWorldRevision != entityTransform.worldRevision) {
			modelComponent.cachedWorldBounds =
				modelComponent.model->GetBoundingBox().Transform(glmWorldMatrix);
			modelComponent.cachedBoundsModel = modelComponent.model.get();
			modelComponent.cachedBoundsWorldRevision = entityTransform.worldRevision;
		}
		const AABB& staticWorldBounds = modelComponent.cachedWorldBounds;

#ifdef __ANDROID__
		auto getStaticLightMask = [&]() -> std::uint32_t {
			if (!ecsManager.lightingSystem) {
				return 0xFFFFFFFFu;
			}

			const std::uint64_t lightingRevision =
				ecsManager.lightingSystem->GetLightingRevision();
			if (modelComponent.cachedLightMaskModel != modelComponent.model.get() ||
				modelComponent.cachedLightMaskWorldRevision != entityTransform.worldRevision ||
				modelComponent.cachedLightingRevision != lightingRevision) {
				modelComponent.cachedLightMask =
					ecsManager.lightingSystem->GetLightMask(staticWorldBounds);
				modelComponent.cachedLightMaskModel = modelComponent.model.get();
				modelComponent.cachedLightMaskWorldRevision = entityTransform.worldRevision;
				modelComponent.cachedLightingRevision = lightingRevision;
			}
			return modelComponent.cachedLightMask;
		};
#endif

       
        if (instancing.IsEnabled())
        {
            // Gather per-entity bloom data for instancing
            glm::vec3 entityBloomColor(0.0f);
            float entityBloomIntensity = 0.0f;
            if (bloomComponent) {
                const auto& bloom = bloomComponent->get();
                if (bloom.enabled) {
                    entityBloomColor = bloom.bloomColor;
                    entityBloomIntensity = bloom.bloomIntensity;
                }
            }

			std::uint32_t instanceLightMask = 0xFFFFFFFFu;
#ifdef __ANDROID__
			if (!modelComponent.HasAnimation() &&
				modelComponent.model->mBoneInfoMap.empty()) {
				instanceLightMask = getStaticLightMask();
			}
#endif

            const InstanceSubmissionResult instanceResult =
                instancing.TryAddInstance(
                    modelComponent,
                    glmWorldMatrix,
					staticWorldBounds,
                    entityBloomColor,
					entityBloomIntensity,
					instanceLightMask);

            if (instanceResult != InstanceSubmissionResult::NotInstanced)
            {
                if (instanceResult == InstanceSubmissionResult::Added)
                {
                    cullingStats.renderedObjects++;
                    if (entityBloomIntensity > 0.01f) {
                        gfxManager.NotifyBloomUsedThisFrame();
                    }
                }
                else
                {
                    cullingStats.culledObjects++;
                }
                continue;
            }
        }

        // =====================================================================
        // Fallback: Not instanceable, render individually
        // =====================================================================

        // Tags only affect the non-instanced player/fade path. Avoid probing
        // component storage for the hundreds of static instances above.
        auto tagComponent = ecsManager.TryGetComponent<TagComponent>(entity);

        // Frustum culling for non-instanced objects. Android also reuses this
        // transformed bound for finite point-light masking below.
#ifdef __ANDROID__
        AABB localBounds = modelComponent.model->GetBoundingBox();
        bool hasConservativeSkinnedBounds = false;
        if (modelComponent.HasAnimation() &&
            !modelComponent.model->mBoneInfoMap.empty()) {
			if (modelComponent.cachedSkinnedBoundsModel !=
					modelComponent.model.get() ||
				modelComponent.cachedSkinnedBoundsPoseRevision !=
					modelComponent.bonePoseRevision) {
				modelComponent.cachedSkinnedBoundsValid =
					modelComponent.model->TryGetSkinnedBoundingBox(
						modelComponent.mFinalBoneMatrices,
						modelComponent.cachedSkinnedLocalBounds);
				modelComponent.cachedSkinnedBoundsModel =
					modelComponent.model.get();
				modelComponent.cachedSkinnedBoundsPoseRevision =
					modelComponent.bonePoseRevision;
			}
			hasConservativeSkinnedBounds =
				modelComponent.cachedSkinnedBoundsValid;
			if (hasConservativeSkinnedBounds) {
				localBounds = modelComponent.cachedSkinnedLocalBounds;
			}
        }
		const AABB worldBounds = hasConservativeSkinnedBounds
			? localBounds.Transform(glmWorldMatrix)
			: staticWorldBounds;
        if (enableCulling && !frustum.IsBoxVisible(worldBounds))
#else
        if (enableCulling &&
			!frustum.IsBoxVisible(staticWorldBounds))
#endif
        {
            cullingStats.culledObjects++;
            continue;
        }

        // Passed culling test, create and submit render item
        ModelRenderComponent* modelRenderItem = nullptr;
        if (renderSnapshotCount == renderSnapshots.size()) {
            renderSnapshots.emplace_back(modelComponent, ModelRenderComponent::RenderSnapshotTag{});
        }
        else {
            renderSnapshots[renderSnapshotCount].UpdateRenderSnapshot(modelComponent);
        }
        modelRenderItem = &renderSnapshots[renderSnapshotCount++];
        modelRenderItem->transform = entityTransform.worldMatrix;

#ifdef __ANDROID__
        // Animated bounds are conservatively reconstructed from current bone
        // matrices. Manual bone editing still falls back to all packed lights
        // because those matrices are updated later in this system.
        modelRenderItem->lightMask = 0xFFFFFFFFu;
        if ((modelRenderItem->model->mBoneInfoMap.empty() ||
            hasConservativeSkinnedBounds) &&
            ecsManager.lightingSystem) {
			modelRenderItem->lightMask = hasConservativeSkinnedBounds
				? ecsManager.lightingSystem->GetLightMask(worldBounds)
				: getStaticLightMask();
        }
#endif

        // If model doesn't have an animation controller, allow manual manipulation of bone entities.
        if (!modelRenderItem->HasAnimation() && !modelRenderItem->model->mBoneInfoMap.empty()) {
            glm::mat4 rootInverse = glm::inverse(entityTransform.worldMatrix.ConvertToGLM());
            for (const auto& [name, boneInfo] : modelRenderItem->model->mBoneInfoMap)
            {
                if (boneInfo.id < 0 || static_cast<size_t>(boneInfo.id) >= modelRenderItem->mFinalBoneMatrices.size()) {
                    continue;
                }

                // Get the child entity representing this bone.
			    auto boneEntityIt = modelComponent.boneNameToEntityMap.find(name);
			    if (boneEntityIt == modelComponent.boneNameToEntityMap.end()) {
			        continue;
			    }
			    Entity boneEntity = boneEntityIt->second;
                auto boneTransform = ecsManager.TryGetComponent<Transform>(boneEntity);
			    if (!boneTransform) {
			        continue;
			    }

			    // Get the transform of the bone entity.
                glm::mat4 currentWorld = boneTransform->get().worldMatrix.ConvertToGLM();

			    // Write to the final bone matrices.
                modelRenderItem->mFinalBoneMatrices[boneInfo.id] =
                    rootInverse * currentWorld * boneInfo.offset;
		    }
        }

        // Per-entity bloom emission
        if (bloomComponent) {
            const auto& bloom = bloomComponent->get();
            if (bloom.enabled) {
                modelRenderItem->bloomColor = bloom.bloomColor;
                modelRenderItem->bloomIntensity = bloom.bloomIntensity;
            }
        }

        // Brighten the player model so it pops against the environment
        if (tagComponent && tagComponent->get().tagIndex == tagPlayer) {
            modelRenderItem->brightnessBoost = 1.35f;
        }

        // Tag items on excluded layers for deferred rendering
        if (excludedLayerMask != 0) {
            int layerIdx = GetEffectiveLayerIndex(entity, ecsManager);
            if (layerIdx >= 0 && layerIdx < 32 && (excludedLayerMask & (1u << layerIdx)))
                modelRenderItem->excludeFromPostProcess = true;
        }
        if (!modelRenderItem->excludeFromPostProcess &&
            modelRenderItem->bloomIntensity > 0.01f) {
            gfxManager.NotifyBloomUsedThisFrame();
        }

        // Distance-based camera fade: only fade entities with specific tags
        if (!isRenderingForEditor && currentCamera && tagComponent) {
            const int tagIndex = tagComponent->get().tagIndex;
            const bool shouldFade =
                tagIndex == tagNPC ||
                tagIndex == tagCollectible ||
                tagIndex == tagNoCameraCollision;

            if (shouldFade) {
                const glm::vec3 entityPosition = glm::vec3(glmWorldMatrix[3]);
                const float distance = glm::length(entityPosition - currentCamera->Position);

                float fade = 0.0f;
                if (cameraFadeFar > cameraFadeNear) {
                    fade = glm::clamp(
                        (distance - cameraFadeNear) / (cameraFadeFar - cameraFadeNear),
                        0.0f, 1.0f);
                }
                modelRenderItem->distanceFadeOpacity = fade;
            }
        }

    }
    gfxManager.SubmitBatch(renderSnapshots, renderSnapshotCount);
#ifdef ANDROID
    //__android_log_print(ANDROID_LOG_INFO, "GAM300", "ModelSystem::Update() completed");
#endif

    // Optional: Print stats every frame (remove in production!)
#ifdef _DEBUG
 //std::cout << "Culling: " << cullingStats.culledObjects << "/" 
 //          << cullingStats.totalObjects << " (" 
 //          << cullingStats.GetCulledPercentage() << "%)\n";
#endif
}

void ModelSystem::Shutdown() 
{
    ENGINE_PRINT("[ModelSystem] Shutdown\n");
}
