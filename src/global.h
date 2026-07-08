#pragma once
#include <memory>
#include <vector>
#include <atomic>

#include "engine/engine.h"
#include "feature/cache.h"
#include "imgui.h"
#include <addons/imgui_addons.h>

namespace global {

	inline sdk::model model;
	inline std::uint64_t GameID;
	inline sdk::render render;
	inline sdk::player LocalPlayer;
	inline sdk::actor actor;
	inline sdk::model workspace;
	inline sdk::light light;
	inline sdk::view view;
	inline sdk::camera camera;

	inline std::vector<sdk::player> Player_Cache;

	namespace overlay {

		inline bool watermark = true;
		inline bool hotkey = true;
		inline bool Notifications = true;
		inline bool ShowKeybinds = true;
		inline bool DamageLogs = true;
		inline ImVec2 Watermark_Pos = { 18.f, 18.f };
		inline ImVec2 Hotkeys_Pos = { 18.f, 72.f };
		inline ImVec2 Notif_Pos   = { 20.f, 200.f };
		inline ImVec2 Media_Pos   = { 0.f,  0.f };  // 0,0 = first-use placement
		inline float AimView_MaxLength = 250.f;
		inline float Spotify = 0.f;

		namespace color {
			inline float Accent[4]{ 0.639f, 0.831f, 0.122f, 1.0f };
			inline float Accent2[4]{ 0.800f, 0.275f, 0.804f, 1.0f };
			inline float panel[4]{ 0.090f, 0.090f, 0.090f, 0.737f };
			inline float text[4]{ 0.804f, 0.804f, 0.804f, 1.0f };
		}
	}

	namespace setting {

		inline bool Team_Check;
		inline bool Client_Check;
		inline bool BotCheck;
		inline bool Streamproof;
		inline int Performance_Mode = 1;
		inline bool Compact_UI = false;
		inline ImGuiKey Menu_Key = ImGuiKey_Insert;

		namespace color {
			inline float Accent[4]{ 0.639f, 0.831f, 0.122f, 1.0f };
			inline float Accent2[4]{ 0.800f, 0.275f, 0.804f, 1.0f };
			inline float Window[4]{ 0.137f, 0.137f, 0.137f, 1.0f };
			inline float card[4]{ 0.090f, 0.090f, 0.090f, 1.0f };
			inline float text[4]{ 0.804f, 0.804f, 0.804f, 1.0f };
		}
	}

	namespace aimbot {

		inline bool Enabled = false;
		inline ImGuiKey Key = ImGuiKey_L;
		inline ImKeyBindMode Mode = ImKeyBindMode_Hold;
		inline bool Legit = false;
		inline float FOV = 180.f;
		inline float Smoothing = 10.f;
		inline float Speed = 5.f;
		inline bool AutoSens = false;
		inline int  SmoothEaseIn  = 1;  // 0=linear 1=ease-out-cubic 2=ease-out-quint
		inline int  SmoothEaseOut = 0;  // 0=linear 1=ease-out-cubic 2=ease-out-quint    // auto-calculate speed from mouse settings
		inline int Hitbox = 0;
		inline bool VisibleCheck = true;
		inline bool DeadCheck = false;
		inline bool TeamCheck = false;
		inline float Jitter = 2.f;
		inline float RandomFOV = 0.f;
		inline int TargetPriority = 0;
		inline bool Prediction = false;
		inline bool AutoShoot = false;
		inline int ReactionMs = 50;
		inline bool Sticky = false;
	}

	namespace aim {

		inline bool TriggerBot = false;
		inline ImGuiKey TriggerBot_Key = ImGuiKey_Q;
		inline ImKeyBindMode TriggerBot_Mode = ImKeyBindMode_Toggle;
		inline bool IntervalToggle = false;
		inline int TriggerDelayMs = 80;
		inline float TriggerRadius = 6.f;
		inline bool VisualiseHitbox = false;
		inline bool VisibleCheck = false;
		inline bool DeadCheck = false;
		inline bool FovCheck = true;
		inline bool DistanceScale = false;
		inline float AutoHeadScale = 1.f;
		inline bool HitPart_Head = true;
		inline bool HitPart_Torso = true;
		inline bool HitPart_UpperTorso = true;
		inline bool HitPart_LowerTorso = true;
		inline bool HitPart_HumanoidRootPart = true;
		inline bool HitPart_LeftArm = true;
		inline bool HitPart_RightArm = true;
		inline bool HitPart_LeftLeg = true;
		inline bool HitPart_RightLeg = true;
		inline float HitPart_Head_Scale = 1.f;
		inline float HitPart_Torso_Scale = 1.f;
		inline float HitPart_UpperTorso_Scale = 1.f;
		inline float HitPart_LowerTorso_Scale = 1.f;
		inline float HitPart_HumanoidRootPart_Scale = 1.f;
		inline float HitPart_LeftArm_Scale = 1.f;
		inline float HitPart_RightArm_Scale = 1.f;
		inline float HitPart_LeftLeg_Scale = 1.f;
    inline float HitPart_RightLeg_Scale = 1.f;
	}

	namespace esp {

		inline bool Enabled;
		inline bool Box;
		inline bool Box_Fill;
		inline bool Local_Player = true;  // show own player in ESP
		inline bool Box_Fill_Gradient;
		inline bool Box_Fill_Gradient_Rotate;
		inline bool Healthbar;
		inline bool Health;
		inline bool name;
		inline bool Distance;
		inline bool Rig_Type;
		inline bool tool;
		inline bool Skeleton;
		inline bool Chams;
		inline bool ChamsFade;
		inline bool Trails;
		inline bool Chinese_Hat;
		inline bool aimline;
		inline bool VisibleCheck = false;
		inline bool Dead_Check = true;   // Hide dead players from ESP
		inline bool Limit_Distance = false;  // Toggle for render distance cap
		inline float Render_Distance = 500.0f;

		inline int ChamsFadeSpeed = 2;
		inline int BoxFillSpeed = 2;
		inline int Healthbar_Type = 1;
		inline int Box_Type = 0;
		inline int Box_Fill_Type = 0;
		inline int Name_Type = 1;
		inline int gap = 2;
		inline int Thickness = 2;

		namespace color {

			inline float Box[4] = { 0.639f, 0.831f, 0.122f, 1.0f };
			inline float BoxFill_Top[4] = { 0.639f, 0.831f, 0.122f, 0.50f };
			inline float BoxFill_Bottom[4] = { 0.f, 0.f, 0.f, 0.50f };
			inline float Healthbar[4] = { 0.639f, 0.831f, 0.122f, 1.0f };
			inline float name[4] = { 0.639f, 0.831f, 0.122f, 1.0f };
			inline float Distance[4] = { 0.800f, 0.275f, 0.804f, 1.0f };
			inline float Rig_Type[4] = { 0.800f, 0.275f, 0.804f, 1.0f };
			inline float tool[4] = { 0.639f, 0.831f, 0.122f, 1.0f };
			inline float Health[4] = { 0.639f, 0.831f, 0.122f, 1.0f };
			inline float Skeleton[4] = { 0.800f, 0.275f, 0.804f, 1.0f };
			inline float Chams[4] = { 0.800f, 0.275f, 0.804f, 1.0f };
			inline float ChamsOutline[4] = { 0.f, 0.f, 0.f, 1.0f };
			inline float Trails[4] = { 0.639f, 0.831f, 0.122f, 1.0f };
			inline float hat[4] = { 0.800f, 0.275f, 0.804f, 1.0f };
			inline float aimline[4] = { 1.0f, 0.313f, 0.407f, 1.0f };
			inline float Visible[4] = { 0.639f, 0.831f, 0.122f, 1.0f };
			inline float NotVisible[4] = { 1.0f, 0.313f, 0.407f, 1.0f };

			inline float Healthbar_Top[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
			inline float Healthbar_Middle[4] = { 1.0f, 1.0f, 0.0f, 1.0f };
			inline float Healthbar_Bottom[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
		}
	}

	namespace misc {
		// Fly (existing)
		inline bool fly = false;
		inline ImGuiKey Fly_Key = ImGuiKey_F;
		inline ImKeyBindMode Fly_Mode = ImKeyBindMode_Toggle;
		inline float Fly_Speed = 50.f;

		// Walkspeed
		inline bool walkspeed = false;
		inline float Walkspeed_Speed = 32.f;

		// Hitbox expand
		inline bool hitbox = false;
		inline float Hitbox_Size_X = 4.f;
		inline float Hitbox_Size_Y = 6.f;
		inline float Hitbox_Size_Z = 4.f;

		// Desync
		inline bool Desync = false;
		inline ImGuiKey Desync_Key = ImGuiKey_None;
		inline ImKeyBindMode Desync_Mode = ImKeyBindMode_Hold;

		// Tickrate
		inline bool  Tickrate         = false;
		inline ImGuiKey Tickrate_Key  = ImGuiKey_None;
		inline ImKeyBindMode Tickrate_Mode = ImKeyBindMode_Hold;
		inline float Tickrate_Value   = 60.f;  // shown in menu; written as *4 internally

		// Animation Changer (18 packs from omc0eg)
		inline bool  AnimChanger      = false;
		inline ImGuiKey AnimKey       = ImGuiKey_None;
		inline ImKeyBindMode AnimMode = ImKeyBindMode_Hold;
		inline int   AnimPack         = 0;     // index into pack list

		// Lighting / Clock Time
		inline bool  Lighting         = false;
		inline bool  ClockTime        = false;
		inline ImGuiKey ClockKey      = ImGuiKey_None;
		inline ImKeyBindMode ClockMode = ImKeyBindMode_Hold;
		inline float ClockTimeValue   = 14.f * 3600.f; // seconds (noon = 14h * 3600)
	}

	namespace crosshair {
		inline bool  Enabled   = false;
		inline int   Type      = 0;    // 0=static, 1=animated, 2=spin
		inline int   Position  = 0;    // 0=follow cursor (replace cursor), 1=screen center
		inline float Size      = 8.f;
		inline float Gap       = 4.f;
		inline float Thickness = 1.5f;
		inline bool  Outline   = true;
		inline float SpinSpeed = 2.f;  // rotations per second for spin type
		inline float Color[4]  = { 1.f, 1.f, 1.f, 1.f };
	}

	namespace tracer {
		inline bool Enabled   = false;
		inline int  Style     = 0;   // 0=straight, 1=curved bezier, 2=dashed
		inline int  Origin    = 3;   // 0=cursor,1=center,2=top,3=bottom,4=head,5=hrp
		inline int  Dest      = 0;   // 0=head, 1=hrp, 2=closest part
		inline float Thickness = 1.f;
		inline bool Outline   = true;
		inline float Color[4] = { 0.639f, 0.831f, 0.122f, 1.f };
	}

	namespace silent {
		inline bool      Enabled         = false;
		inline ImGuiKey  Silent_Key       = ImGuiKey_MouseRight;
		inline ImKeyBindMode Silent_Mode  = ImKeyBindMode_Hold;
		inline bool      VisibleCheck     = false;
		inline bool      KnockedCheck     = true;
		inline bool      UseFov           = true;
		inline float     fov              = 120.f;
		inline bool      GunBasedFov      = false;
		inline float     FovDoubleBarrel  = 60.f;
		inline float     FovTacticalShotgun = 80.f;
		inline float     FovRevolver      = 90.f;
		inline int       AimPart          = 0;   // 0=Head,1=Upper,2=Lower
		inline int       TargetPriority   = 0;   // 0=Crosshair,1=Distance
		inline bool      StickyAim        = false;
		inline bool      Prediction       = false;
		inline bool      AutoPrediction   = true;
		inline float     PredictionX      = 0.05f;
		inline float     PredictionY      = 0.05f;
		inline float     PredictionZ      = 0.05f;
		// Magic bullet
		inline bool      MagicBullet      = false;
	}
}
