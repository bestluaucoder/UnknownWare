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
		inline bool DamageLogs = true;
		inline ImVec2 Watermark_Pos = { 18.f, 18.f };
		inline ImVec2 Hotkeys_Pos = { 18.f, 72.f };
		inline float AimView_MaxLength = 250.f;

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

		inline float Render_Distance = 200.0f;

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
}
