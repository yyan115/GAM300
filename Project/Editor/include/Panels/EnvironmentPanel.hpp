#pragma once
#include "EditorPanel.hpp"

class EnvironmentPanel : public EditorPanel {
public:
    EnvironmentPanel();
    virtual ~EnvironmentPanel() = default;

    void OnImGuiRender() override;
};
