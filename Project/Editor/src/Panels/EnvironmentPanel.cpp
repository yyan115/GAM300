#include "pch.h"
#include "Panels/EnvironmentPanel.hpp"
#include "ECS/ECSRegistry.hpp"
#include "Graphics/Lights/LightingSystem.hpp"
#include <imgui.h>

EnvironmentPanel::EnvironmentPanel()
    : EditorPanel("Environment", false) {
}

void EnvironmentPanel::OnImGuiRender() {
    if (!isOpen) return;

    ImGui::Begin("Environment", &isOpen);

    auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
    auto lightingSystem = ecs.GetSystem<LightingSystem>();

    if (!lightingSystem) {
        ImGui::TextDisabled("Lighting system unavailable.");
        ImGui::End();
        return;
    }

    // -------------------------------------------------------------------------
    // Ambient Lighting
    // -------------------------------------------------------------------------
    ImGui::SeparatorText("Ambient Lighting");

    // Mode dropdown
    const char* modeNames[] = { "Color", "Gradient", "Skybox" };
    int currentMode = static_cast<int>(lightingSystem->ambientMode);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo("##AmbientMode", &currentMode, modeNames, 3)) {
        lightingSystem->SetAmbientMode(static_cast<LightingSystem::AmbientMode>(currentMode));
    }
    ImGui::Spacing();

    const float labelW = 90.0f;

    if (lightingSystem->ambientMode == LightingSystem::AmbientMode::Color) {
        // Single color — use ambientSky as the overall ambient color
        ImGui::Text("Color");
        ImGui::SameLine(labelW);
        ImGui::SetNextItemWidth(-1);
        float col[3] = { lightingSystem->ambientSky.r, lightingSystem->ambientSky.g, lightingSystem->ambientSky.b };
        if (ImGui::ColorEdit3("##AmbientColor", col)) {
            lightingSystem->SetAmbientSky({ col[0], col[1], col[2] });
            lightingSystem->SetAmbientEquator({ col[0] * 0.6f, col[1] * 0.6f, col[2] * 0.6f });
            lightingSystem->SetAmbientGround({ col[0] * 0.3f, col[1] * 0.3f, col[2] * 0.3f });
        }
    }
    else if (lightingSystem->ambientMode == LightingSystem::AmbientMode::Gradient) {
        // Three separate color pickers
        ImGui::Text("Sky");
        ImGui::SameLine(labelW);
        ImGui::SetNextItemWidth(-1);
        float sky[3] = { lightingSystem->ambientSky.r, lightingSystem->ambientSky.g, lightingSystem->ambientSky.b };
        if (ImGui::ColorEdit3("##AmbientSky", sky)) {
            lightingSystem->SetAmbientSky({ sky[0], sky[1], sky[2] });
        }

        ImGui::Text("Equator");
        ImGui::SameLine(labelW);
        ImGui::SetNextItemWidth(-1);
        float eq[3] = { lightingSystem->ambientEquator.r, lightingSystem->ambientEquator.g, lightingSystem->ambientEquator.b };
        if (ImGui::ColorEdit3("##AmbientEquator", eq)) {
            lightingSystem->SetAmbientEquator({ eq[0], eq[1], eq[2] });
        }

        ImGui::Text("Ground");
        ImGui::SameLine(labelW);
        ImGui::SetNextItemWidth(-1);
        float gnd[3] = { lightingSystem->ambientGround.r, lightingSystem->ambientGround.g, lightingSystem->ambientGround.b };
        if (ImGui::ColorEdit3("##AmbientGround", gnd)) {
            lightingSystem->SetAmbientGround({ gnd[0], gnd[1], gnd[2] });
        }
    }
    else {
        ImGui::TextDisabled("Ambient is driven by the skybox.");
    }

    ImGui::Spacing();
    ImGui::Text("Intensity");
    ImGui::SameLine(labelW);
    ImGui::SetNextItemWidth(-1);
    ImGui::SliderFloat("##AmbientIntensity", &lightingSystem->ambientIntensity, 0.0f, 4.0f);

    UpdateFocusState();
    ImGui::End();
}
