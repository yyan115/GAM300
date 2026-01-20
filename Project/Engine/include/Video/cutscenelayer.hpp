///////////////////////////////////////////////////////////////////////////////
//// WLVERSE [https://wlverse.web.app]
//// cutscenelayer.h / .cpp
//// 
//// Declares the CutsceneLayer class, which handles the logic for displaying
//// cutscenes with dialogue, transitions, and UI interaction.
////
//// This base layer manages cutscene playback, including image sequencing,
//// animated text dialogue, input response, autoplay logic, and transition effects.
////
//// AUTHORS
//// [100%] Soh Wei Jie (weijie.soh\@digipen.edu)
////   - Designed and implemented full cutscene system including UI, timing,
////     image transitions, and text animation.
////
//// Copyright (c) 2025 DigiPen, All rights reserved.
///////////////////////////////////////////////////////////////////////////////
//#pragma once
//
//#include <FlexEngine.h>
//using namespace FlexEngine;
//
//namespace Game
//{
//    // Defines the current transition phase in the cutscene.
//    enum TransitionPhase
//    {
//        None,          // No transition is occurring; normal image display.
//        PreTransition, // Pre-transition phase (e.g., fade-out before swap).
//        PostTransition // Post-transition phase (e.g., fade-in after swap).
//    };
//
//    class CutsceneLayer : public FlexEngine::Layer
//    {
//        // Asset references.
//        FlexECS::Scene::StringIndex  m_currDialogueFile = 0;
//        FlexECS::Scene::StringIndex  m_currCutsceneFile = 0;
//
//        // Containers for cutscene images and dialogue.
//        std::vector<FlexECS::Scene::StringIndex> m_CutsceneImages;
//        std::vector<std::vector<FlexECS::Scene::StringIndex>> m_CutsceneDialogue;
//        std::vector<std::string> m_dialogue_scenes; //each element is a key to cutsceneasset.cutscenes
//        //std::unordered_map<std::string, Asset::VideoCutsceneInfo> m_cutscene_info;
//
//#pragma region Variables
//
//        FlexECS::Entity m_videoplayer;
//
//        // Index management for sections, frames, and dialogue lines.
//        size_t m_currSectionIndex = 0;   // Which cutscene section is active.
//        size_t m_currFrameIndex = 0;     // Which frame in the current section.
//        size_t m_currDialogueIndex = 0;  // Which dialogue line is active.
//
//        // Entities used for rendering.
//        //FlexECS::Entity m_currShot;
//        //FlexECS::Entity m_nextShot;
//        FlexECS::Entity m_dialoguebox;
//        FlexECS::Entity m_shadowdialoguebox;
//        FlexECS::Entity m_dialoguearrow;
//
//        FlexECS::Entity m_autoplayText;
//        FlexECS::Entity m_autoplayBtn;
//        FlexECS::Entity m_autoplaySymbolAuto;     // To show that auto playing isn't active
//        FlexECS::Entity m_autoplaySymbolPlaying;  // To show that auto playing is active
//
//        FlexECS::Entity m_skiptext;
//        FlexECS::Entity m_instructiontxt;
//        FlexECS::Entity m_instructiontxtopacityblk; //Have to do this due to text no opacity
//        FlexECS::Entity m_skipwheel;     // To show that auto playing isn't active
//
//        // Timing and phase management.
//        float m_ElapsedTime = 0.0f;      // Time in the normal (non-transition) phase.
//        float m_ImageDuration = 0.6f;    // Duration for displaying each image.
//        float m_PerFrameDuration = 0.0f; // Duration for each frame in the section.
//        int m_frameCount = 1;
//
//        float m_dialogueTimer = 0.0f;    // Accumulates time for text animation.
//        float m_dialogueHoldTimer = 0.0f;// Time after text is fully revealed.
//        float m_dialogueTextRate = 30.0f;// Characters per second.
//        float m_dialogueHoldDuration = 1.0f; // Hold duration after full text reveal.
//        bool is_autoplay = false;         // Determines if dialogue advances automatically.
//        bool enable_clickingreaction = true;   // Determines if cutscene should be clickable
//
//        // Transition phase variables.
//        TransitionPhase m_TransitionPhase = TransitionPhase::None;
//        float m_TransitionElapsedTime = 0.0f;
//        float m_PreTransitionDuration = 1.0f;
//        float m_PostTransitionDuration = 1.0f;
//
//        // UI timers
//        bool m_instructionActive = true;
//        float m_instructionTimer = 0.0f;
//        float m_instructionDuration = 2.0f;
//        bool m_dialogueIsWaitingForInput = false;
//        float m_arrowTimer = 0.0f;
//        float m_arrowOscillationFrequency = 3.0f;
//        float m_arrowOscillationAmplitude = 3.0f;
//        float m_skipTimer = 0.0f;
//        float m_skipFadeDuration = 2.0f;
//        float m_skipTextRate = 20.0f;
//        float m_baseRotationSpeed = 1200.0f;
//        float m_skipHoldThreshold = 3.0f;
//
//        // Overall cutscene activation flag.
//        bool m_CutsceneActive = false;
//        bool m_messageSent = false;
//#pragma endregion
//
//#pragma region Helper Func
//    private:
//        // Handles global input actions like skipping or restarting the cutscene.
//        void processGlobalInput();
//
//        // Updates the animated display of dialogue characters over time.
//        void updateDialogueText(float dt);
//
//        // Advances to the next dialogue line or section, if applicable.
//        void advanceDialogue();
//
//        // Immediately jumps to the last frame in the current section.
//        //void skipRemainingFrames();
//
//        // Automatically progresses dialogue when autoplay is enabled.
//        void updateDialogueAuto(float dt);
//
//        // Handles manual input-based progression of dialogue.
//        void updateDialogueManual(float dt);
//
//        // Handles The changing of video if needed
//        void updateVideoPlayback();
//
//        // Updates image frames when the section includes multiple image frames.
//        //void updateImageFrames(float dt);
//
//        // Controls transition effects such as fade-out and fade-in between shots.
//        void updateTransitionPhase(float dt);
//
//        // Swaps the current cutscene shot entity with the next shot.
//        //void SwapShots();
//
//        // Updates timing values and per-frame duration when transitioning between sections.
//        void UpdateTimings(bool toNextSection = false);
//#pragma endregion
//
//#pragma region UI Animation
//        // Updates the animation for the instructional "Click to continue" text.
//        void updateInstructionAnimation(float dt);
//
//        // Animates the dialogue arrow to indicate input is expected.
//        void updateDialogueArrow(float dt);
//
//        // Updates skip button UI effects such as fading, spinning, and text visibility.
//        void updateSkipUI(float dt);
//#pragma endregion
//
//    public:
//        // Constructs the cutscene layer with a default layer name.
//        CutsceneLayer() : Layer("Cutscene Layer") {}
//
//        // Default destructor.
//        ~CutsceneLayer() = default;
//
//        // Called when the layer is attached to the application.
//        virtual void OnAttach() override;
//
//        // Called when the layer is removed from the application.
//        virtual void OnDetach() override;
//
//        // Called every frame to update cutscene logic, visuals, and UI.
//        virtual void Update() override;
//
//        // Loads a new cutscene using provided dialogue and cutscene files.
//        void loadCutscene(FlexECS::Scene::StringIndex dialogue_file, FlexECS::Scene::StringIndex cutscene_file);
//
//        // Starts playback of the cutscene from the beginning.
//        void StartCutscene();
//
//        // Ends and deactivates the currently playing cutscene.
//        void StopCutscene();
//
//        // Fully restarts the cutscene and resets its state.
//        void RestartCutscene();
//    };
//
//}
