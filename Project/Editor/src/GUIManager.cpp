#include "pch.h"
#include "GUIManager.hpp"
#include "imgui.h"
#include "imgui_internal.h" 
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "ImGuizmo.h"
#include "Graphics/SceneRenderer.hpp"
#include "WindowManager.hpp"
#include "EditorState.hpp"
#include "../../Libraries/IconFontCppHeaders/IconsFontAwesome6.h"
#include "Logging.hpp"

#include "Scene/SceneManager.hpp"
// Include panel headers
#include "Panels/ScenePanel.hpp"
#include "Panels/SceneHierarchyPanel.hpp"
#include "Panels/InspectorPanel.hpp"
#include "Panels/ConsolePanel.hpp"
#include "Panels/GamePanel.hpp"
#include "Panels/PlayControlPanel.hpp"
#include "Panels/PerformancePanel.hpp"
#include "Panels/AssetBrowserPanel.hpp"
#include "Panels/AudioMixerPanel.hpp"
#include <Asset Manager/AssetManager.hpp>
#include "Asset Manager/MetaFilesManager.hpp"


// Static member definitions
std::unique_ptr<PanelManager> GUIManager::panelManager = nullptr;
bool GUIManager::dockspaceInitialized = false;
Entity GUIManager::selectedEntity = static_cast<Entity>(-1);
GUID_128 GUIManager::selectedAsset = GUID_128{0, 0};

void GUIManager::Initialize() {
	GLFWwindow* window = WindowManager::getWindow();
	assert(window != nullptr && "GLFW window must be valid before initializing ImGui");
	
	// Make sure the context is current
	glfwMakeContextCurrent(window);

	// ImGui initialization
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // Enable Docking

	io.IniFilename = "imgui.ini";
	
	CreateEditorTheme();

	// Initialize platform/renderer bindings
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 450");
	
	// Note: Framebuffer creation is delayed until first render call
	// This ensures GLAD is properly initialized first

	// Initialize panel manager and default panels
	panelManager = std::make_unique<PanelManager>();
	assert(panelManager != nullptr && "Failed to create PanelManager");

	SetupDefaultPanels();

	// Set editor to edit mode on startup (engine defaults to play mode for game builds)
	EditorState& editorState = EditorState::GetInstance();
	editorState.SetState(EditorState::State::EDIT_MODE);
	ENGINE_PRINT("[GUIManager] Initialized with panel - based architecture\n");
}


void GUIManager::Render() {
	assert(panelManager != nullptr && "PanelManager must be initialized before rendering");
	
	// Start ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	
	// Initialize ImGuizmo for this frame
	ImGuizmo::BeginFrame();

	// Create main dockspace
	CreateDockspace();

	// Render menu bar
	RenderMenuBar();

	// Render all open panels
	if (panelManager) {
		panelManager->RenderOpenPanels();
	}

	// Handle multi-viewport rendering
	ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		GLFWwindow* backup_current_context = glfwGetCurrentContext();
		assert(backup_current_context != nullptr && "No current GLFW context");
		
		glfwMakeContextCurrent(backup_current_context);
	}

	// Render ImGui on top of the scene
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	// Required for multi-viewport support:
	if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}

	AssetManager::GetInstance().RunEventQueue();
}

void GUIManager::Exit() {
	// SceneRenderer handles framebuffer cleanup
	SceneRenderer::DeleteSceneFramebuffer();
	
	// Clean up panel manager
	panelManager.reset();

	// Clean up ImGui resources
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	ENGINE_PRINT("[GUIManager] Shutdown complete\n");
}

void GUIManager::SetupDefaultPanels() {
	assert(panelManager != nullptr && "PanelManager must be initialized before setting up panels");

	// Register core editor panels
	auto sceneHierarchyPanel = std::make_shared<SceneHierarchyPanel>();
	assert(sceneHierarchyPanel != nullptr && "Failed to create SceneHierarchyPanel");
	panelManager->RegisterPanel(sceneHierarchyPanel);

	auto inspectorPanel = std::make_shared<InspectorPanel>();
	assert(inspectorPanel != nullptr && "Failed to create InspectorPanel");
	panelManager->RegisterPanel(inspectorPanel);

	auto consolePanel = std::make_shared<ConsolePanel>();
	assert(consolePanel != nullptr && "Failed to create ConsolePanel");
	panelManager->RegisterPanel(consolePanel);

	auto scenePanel = std::make_shared<ScenePanel>();
	assert(scenePanel != nullptr && "Failed to create ScenePanel");
	panelManager->RegisterPanel(scenePanel);

	auto gamePanel = std::make_shared<GamePanel>();
	assert(gamePanel != nullptr && "Failed to create GamePanel");
	panelManager->RegisterPanel(gamePanel);

	auto playControlPanel = std::make_shared<PlayControlPanel>();
	assert(playControlPanel != nullptr && "Failed to create PlayControlPanel");
	panelManager->RegisterPanel(playControlPanel);

	auto performancePanel = std::make_shared<PerformancePanel>();
	assert(performancePanel != nullptr && "Failed to create PerformancePanel");
	panelManager->RegisterPanel(performancePanel);

	auto assetBrowserPanel = std::make_shared<AssetBrowserPanel>();
	assert(assetBrowserPanel != nullptr && "Failed to create AssetBrowserPanel");
	panelManager->RegisterPanel(assetBrowserPanel);

	auto audioMixerPanel = std::make_shared<AudioMixerPanel>();
	assert(audioMixerPanel != nullptr && "Failed to create AudioMixerPanel");
	panelManager->RegisterPanel(audioMixerPanel);
	
	ENGINE_PRINT("[GUIManager] Default panels registered\n");
}

void GUIManager::CreateDockspace() {
	// Create a fullscreen dockspace, accounting for the play controls toolbar
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	float menuBarHeight = ImGui::GetFrameHeight();
	float toolbarHeight = ImGui::GetFrameHeight() + 16.0f; // Match toolbar height from PlayControlPanel

	// Position dockspace below menu bar and toolbar
	ImVec2 dockspacePos = ImVec2(viewport->Pos.x, viewport->Pos.y + menuBarHeight + toolbarHeight);
	ImVec2 dockspaceSize = ImVec2(viewport->Size.x, viewport->Size.y - menuBarHeight - toolbarHeight);

	ImGui::SetNextWindowPos(dockspacePos);
	ImGui::SetNextWindowSize(dockspaceSize);
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
	window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
	window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	ImGui::Begin("DockSpace", nullptr, window_flags);
	ImGui::PopStyleVar(3);

	// Create the docking space
	ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
		ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

		// Check if we need to initialize the default layout
		// Only do this if no docking data exists for this dockspace
		if (!dockspaceInitialized) {
			dockspaceInitialized = true;

			// Check if the dockspace already has nodes (from saved layout)
			ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspace_id);
			if (!node || !node->IsSplitNode()) {
				// No saved layout exists, create default layout
				ImGui::DockBuilderRemoveNode(dockspace_id);
				ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
				ImGui::DockBuilderSetNodeSize(dockspace_id, dockspaceSize);

				// Create the splits
				ImGuiID dock_left, dock_right, dock_down;
				ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.2f, &dock_left, &dock_right);
				ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Down, 0.3f, &dock_down, &dock_right);

				// Dock the panels to default positions
				ImGui::DockBuilderDockWindow("Scene Hierarchy", dock_left);
				ImGui::DockBuilderDockWindow("Scene Panel", dock_right);
				ImGui::DockBuilderDockWindow("Game", dock_right);
				ImGui::DockBuilderDockWindow("Inspector", dock_left);
				ImGui::DockBuilderDockWindow("Console", dock_down);
				ImGui::DockBuilderDockWindow("Performance", dock_down);
				ImGui::DockBuilderDockWindow("Asset Browser", dock_down);

				// Ensure all panels are open by default so they get saved to the layout
				if (panelManager) {
					auto gamePanel = panelManager->GetPanel("Game");
					auto scenePanel = panelManager->GetPanel("Scene Panel");
					auto hierarchyPanel = panelManager->GetPanel("Scene Hierarchy");
					auto inspectorPanel = panelManager->GetPanel("Inspector");
					auto consolePanel = panelManager->GetPanel("Console");
					auto performancePanel = panelManager->GetPanel("Performance");
					auto assetBrowserPanel = panelManager->GetPanel("Asset Browser");

					if (gamePanel) gamePanel->SetOpen(true);
					if (scenePanel) scenePanel->SetOpen(true);
					if (hierarchyPanel) hierarchyPanel->SetOpen(true);
					if (inspectorPanel) inspectorPanel->SetOpen(true);
					if (consolePanel) consolePanel->SetOpen(true);
					if (performancePanel) performancePanel->SetOpen(true);
					if (assetBrowserPanel) assetBrowserPanel->SetOpen(true);
				}

				ImGui::DockBuilderFinish(dockspace_id);
			}
		}
	}

	ImGui::End();
}

void GUIManager::RenderMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            // if (ImGui::MenuItem(ICON_FA_FILE_CIRCLE_PLUS " New Scene", "Ctrl+N")) {
            //     // TODO: New scene functionality
            // }
            // if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Open Scene", "Ctrl+O")) {
            //     std::string filepath = "Resources/Scenes/scene.json";
            //     // TEMP
            //     if (!std::filesystem::exists(filepath)) {
            //         std::cerr << "No saved scene yet! Save scene first!" << std::endl;
            //     }
            //     else {
            //         SceneManager::GetInstance().LoadScene(filepath);
            //     }
            // }
            if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " Save Scene", "Ctrl+S")) {
                SceneManager::GetInstance().SaveScene();
            }
            ImGui::Separator();
            if (ImGui::MenuItem(ICON_FA_DESKTOP " Compile Assets for Desktop")) {
                AssetManager::GetInstance().CompileAllAssetsForDesktop();
            }
            if (ImGui::MenuItem(ICON_FA_MOBILE_SCREEN_BUTTON " Compile Assets for Android")) {
                AssetManager::GetInstance().CompileAllAssetsForAndroid();
            }
            ImGui::Separator();
            if (ImGui::MenuItem(ICON_FA_RIGHT_FROM_BRACKET " Exit", "Alt+F4")) {
                // TODO: Exit application
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem(ICON_FA_ROTATE_LEFT " Undo", "Ctrl+Z")) {
                // TODO: Undo functionality
            }
            if (ImGui::MenuItem(ICON_FA_ROTATE_RIGHT " Redo", "Ctrl+Y")) {
                // TODO: Redo functionality
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem(ICON_FA_ROTATE " Reset Layout")) {
                // Reset to default docking layout
                dockspaceInitialized = false;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window")) {
            if (panelManager) {
                // Panel toggles
                for (const auto& panel : panelManager->GetAllPanels()) {
                    bool isOpen = panel->IsOpen();
                    if (ImGui::MenuItem(panel->GetName().c_str(), nullptr, &isOpen)) {
                        panel->SetOpen(isOpen);
                    }
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem(ICON_FA_CIRCLE_INFO " About")) {
                // TODO: About dialog
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void GUIManager::CreateEditorTheme() {
	float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
	// Set updark theme
	ImGuiIO& io = ImGui::GetIO();
	ImGui::StyleColorsDark();
	ImGuiStyle& style = ImGui::GetStyle();
	io.Fonts->Clear();

	// Load main font
	ImFont* mainFont = io.Fonts->AddFontFromFileTTF("Resources/Inter.ttf", 18.0f);

	// Merge FontAwesome icons
	static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
	ImFontConfig icons_config;
	icons_config.MergeMode = true;
	icons_config.PixelSnapH = true;
	icons_config.GlyphMinAdvanceX = 18.0f; // Match main font size
	io.Fonts->AddFontFromFileTTF("Resources/Fonts/fa-solid-900.ttf", 18.0f, &icons_config, icons_ranges);
	style.ScaleAllSizes(main_scale);        // Bake a fixed style scale
	io.ConfigDpiScaleFonts = true;          // This will scale fonts but _NOT_ scale sizes/padding
	io.ConfigDpiScaleViewports = true;

	ImVec4* colors = style.Colors;

	// General Background colors
	colors[ImGuiCol_WindowBg] = ImVec4(0.16f, 0.16f, 0.16f, 1.0f);     // Dark gray
	colors[ImGuiCol_ChildBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);      // Slightly lighter gray
	colors[ImGuiCol_PopupBg] = ImVec4(0.22f, 0.22f, 0.22f, 1.0f);      // Popup background

	// Borders
	colors[ImGuiCol_Border] = ImVec4(0.28f, 0.28f, 0.28f, 1.0f);       // Medium gray border
	colors[ImGuiCol_BorderShadow] = ImVec4(0.10f, 0.10f, 0.10f, 0.5f); // Subtle shadow

	// Text
	colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 1.0f);         // White text
	colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.0f); // Gray text for disabled items

	// Headers
	colors[ImGuiCol_Header] = ImVec4(0.26f, 0.26f, 0.26f, 1.0f);       // Header background
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.0f); // Hovered header
	colors[ImGuiCol_HeaderActive] = ImVec4(0.33f, 0.33f, 0.33f, 1.0f); // Active header

	// Buttons
	colors[ImGuiCol_Button] = ImVec4(0.26f, 0.26f, 0.26f, 1.0f);       // Button background
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.0f); // Hovered button
	colors[ImGuiCol_ButtonActive] = ImVec4(0.33f, 0.33f, 0.33f, 1.0f); // Active button

	// Tabs
	colors[ImGuiCol_Tab] = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);          // Tab background
	colors[ImGuiCol_TabHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.0f);   // Hovered tab
	colors[ImGuiCol_TabActive] = ImVec4(0.26f, 0.26f, 0.26f, 1.0f);    // Active tab
	colors[ImGuiCol_TabUnfocused] = ImVec4(0.18f, 0.18f, 0.18f, 1.0f); // Unfocused tab
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.22f, 0.22f, 0.22f, 1.0f); // Unfocused active tab

	// Title Bar (for windows)
	colors[ImGuiCol_TitleBg] = ImVec4(0.16f, 0.16f, 0.16f, 1.0f);      // Title background
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.20f, 0.20f, 0.20f, 1.0f); // Active title background
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.16f, 0.16f, 0.16f, 1.0f); // Collapsed title

	// Menu Bar
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);    // Menu bar background

	// Scrollbars
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.16f, 0.16f, 0.16f, 1.0f);  // Scrollbar background
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.28f, 0.28f, 0.28f, 1.0f); // Scrollbar grab
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.33f, 0.33f, 0.33f, 1.0f); // Hovered grab
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.36f, 0.36f, 0.36f, 1.0f); // Active grab

	// Slider
	colors[ImGuiCol_SliderGrab] = ImVec4(0.28f, 0.28f, 0.28f, 1.0f);   // Slider grab
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.33f, 0.33f, 0.33f, 1.0f); // Active slider

	// Rounding settings for a polished look
	style.WindowRounding = 4.0f;
	style.FrameRounding = 4.0f;
	style.GrabRounding = 4.0f;
}