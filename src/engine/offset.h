#pragma once

#include <cstdint>
#include <string>
namespace offset {
    inline std::string ClientVersion = "version-5cf2272675e145f5";

    namespace air {
         inline constexpr uintptr_t AirDensity = 0x18;
         inline constexpr uintptr_t GlobalWind = 0x3c;
    }

    namespace animation {
         inline constexpr uintptr_t Animation = 0xb8;
         inline constexpr uintptr_t animator = 0x100;
         inline constexpr uintptr_t IsPlaying = 0xa40;
         inline constexpr uintptr_t Looped = 0xdd;
         inline constexpr uintptr_t Speed = 0xcc;
         inline constexpr uintptr_t TimePosition = 0xd0;
    }

    namespace animator {
         inline constexpr uintptr_t ActiveAnimations = 0x9d0;
    }

    namespace atmosphere {
         inline constexpr uintptr_t tocolor = 0xb8;
         inline constexpr uintptr_t Decay = 0xc4;
         inline constexpr uintptr_t Density = 0xd0;
         inline constexpr uintptr_t Glare = 0xd4;
         inline constexpr uintptr_t Haze = 0xd8;
         inline constexpr uintptr_t Offset = 0xdc;
    }

    namespace attachment {
         inline constexpr uintptr_t Position = 0xc4;
    }

    namespace basepart {
         inline constexpr uintptr_t CastShadow = 0xd5;
         inline constexpr uintptr_t Color3 = 0x148;
         inline constexpr uintptr_t Locked = 0xd6;
         inline constexpr uintptr_t Massless = 0xd7;
         inline constexpr uintptr_t primitive = 0x128;
         inline constexpr uintptr_t Reflectance = 0xcc;
         inline constexpr uintptr_t Shape = 0x159;
         inline constexpr uintptr_t Transparency = 0xd0;
    }

    namespace beam {
         inline constexpr uintptr_t Attachment0 = 0x160;
         inline constexpr uintptr_t Attachment1 = 0x170;
         inline constexpr uintptr_t Brightness = 0x180;
         inline constexpr uintptr_t CurveSize0 = 0x184;
         inline constexpr uintptr_t CurveSize1 = 0x188;
         inline constexpr uintptr_t LightEmission = 0x18c;
         inline constexpr uintptr_t LightInfluence = 0x190;
         inline constexpr uintptr_t Texture = 0x140;
         inline constexpr uintptr_t TextureLength = 0x19c;
         inline constexpr uintptr_t TextureSpeed = 0x1a4;
         inline constexpr uintptr_t Width0 = 0x1a8;
         inline constexpr uintptr_t Width1 = 0x1ac;
         inline constexpr uintptr_t ZOffset = 0x1b0;
    }

    namespace bloom {
         inline constexpr uintptr_t Enabled = 0xb0;
         inline constexpr uintptr_t Intensity = 0xb8;
         inline constexpr uintptr_t Size = 0xbc;
         inline constexpr uintptr_t Threshold = 0xc0;
    }

    namespace blur {
         inline constexpr uintptr_t Enabled = 0xb0;
         inline constexpr uintptr_t Size = 0xb8;
    }

    namespace bytecode {
         inline constexpr uintptr_t Pointer = 0x10;
         inline constexpr uintptr_t Size = 0x20;
    }

    namespace camera {
         inline constexpr uintptr_t CameraSubject = 0xc8;
         inline constexpr uintptr_t CameraType = 0x138;
         inline constexpr uintptr_t FieldOfView = 0x140;
         inline constexpr uintptr_t ImagePlaneDepth = 0x2d0;
         inline constexpr uintptr_t Position = 0xfc;
         inline constexpr uintptr_t Rotation = 0xd8;
         inline constexpr uintptr_t Viewport = 0x28c;
         inline constexpr uintptr_t ViewportSize = 0x2c8;
    }

    namespace charmesh {
         inline constexpr uintptr_t BaseTextureId = 0xc8;
         inline constexpr uintptr_t BodyPart = 0x148;
         inline constexpr uintptr_t MeshId = 0xf8;
         inline constexpr uintptr_t OverlayTextureId = 0x128;
    }

    namespace click {
         inline constexpr uintptr_t MaxActivationDistance = 0xe8;
         inline constexpr uintptr_t MouseIcon = 0xc8;
    }

    namespace clothing {
         inline constexpr uintptr_t Color3 = 0x120;
         inline constexpr uintptr_t Template = 0x100;
    }

    namespace correction {
         inline constexpr uintptr_t Brightness = 0xc4;
         inline constexpr uintptr_t Contrast = 0xc8;
         inline constexpr uintptr_t Enabled = 0xb0;
         inline constexpr uintptr_t TintColor = 0xb8;
    }

    namespace grading {
         inline constexpr uintptr_t Enabled = 0xb0;
         inline constexpr uintptr_t TonemapperPreset = 0xb8;
    }

    namespace datamodel {
         inline constexpr uintptr_t CreatorId = 0x180;
         inline constexpr uintptr_t GameId = 0x188;
         inline constexpr uintptr_t GameLoaded = 0x658;
         inline constexpr uintptr_t JobId = 0x120;
         inline constexpr uintptr_t PlaceId = 0x190;
         inline constexpr uintptr_t PlaceVersion = 0x1ac;
         inline constexpr uintptr_t PrimitiveCount = 0x488;
         inline constexpr uintptr_t scriptcontext = 0x440;
         inline constexpr uintptr_t ServerIP = 0x640;
         inline constexpr uintptr_t ToRenderView1 = 0x1c8;
         inline constexpr uintptr_t ToRenderView2 = 0x8;
         inline constexpr uintptr_t ToRenderView3 = 0x28;
         inline constexpr uintptr_t workspace = 0x160;
    }

    namespace depth {
         inline constexpr uintptr_t Enabled = 0xb0;
         inline constexpr uintptr_t FarIntensity = 0xb8;
         inline constexpr uintptr_t FocusDistance = 0xbc;
         inline constexpr uintptr_t InFocusRadius = 0xc0;
         inline constexpr uintptr_t NearIntensity = 0xc4;
    }

    namespace drag {
         inline constexpr uintptr_t ActivatedCursorIcon = 0x1c0;
         inline constexpr uintptr_t CursorIcon = 0xc8;
         inline constexpr uintptr_t MaxActivationDistance = 0xe8;
         inline constexpr uintptr_t MaxDragAngle = 0x2a8;
         inline constexpr uintptr_t MaxDragTranslation = 0x26c;
         inline constexpr uintptr_t MaxForce = 0x2ac;
         inline constexpr uintptr_t MaxTorque = 0x2b0;
         inline constexpr uintptr_t MinDragAngle = 0x2b4;
         inline constexpr uintptr_t MinDragTranslation = 0x278;
         inline constexpr uintptr_t ReferenceInstance = 0x1f0;
         inline constexpr uintptr_t Responsiveness = 0x2c0;
    }

    namespace fakemodel {
         inline constexpr uintptr_t Pointer = 0x7c3d2e8;
         inline constexpr uintptr_t RealDataModel = 0x1d0;
    }

    namespace gui2d {
         inline constexpr uintptr_t AbsolutePosition = 0xf8;
         inline constexpr uintptr_t AbsoluteRotation = 0x178;
         inline constexpr uintptr_t AbsoluteSize = 0x100;
    }

    namespace gui {
         inline constexpr uintptr_t BackgroundColor3 = 0x540;
         inline constexpr uintptr_t BackgroundTransparency = 0x54c;
         inline constexpr uintptr_t BorderColor3 = 0x54c;
         inline constexpr uintptr_t Image = 0x988;
         inline constexpr uintptr_t LayoutOrder = 0x580;
         inline constexpr uintptr_t Position = 0x510;
         inline constexpr uintptr_t RichText = 0xb50;
         inline constexpr uintptr_t Rotation = 0x178;
         inline constexpr uintptr_t ScreenGui_Enabled = 0x4c4;
         inline constexpr uintptr_t Size = 0x530;
         inline constexpr uintptr_t text = 0xda0;
         inline constexpr uintptr_t TextColor3 = 0xe50;
         inline constexpr uintptr_t Visible = 0x5ad;
         inline constexpr uintptr_t ZIndex = 0x5a4;
    }

    namespace humanoid {
         inline constexpr uintptr_t AutoJumpEnabled = 0x1d4;
         inline constexpr uintptr_t AutoRotate = 0x1d5;
         inline constexpr uintptr_t AutomaticScalingEnabled = 0x1d6;
         inline constexpr uintptr_t BreakJointsOnDeath = 0x1d7;
         inline constexpr uintptr_t CameraOffset = 0x128;
         inline constexpr uintptr_t DisplayDistanceType = 0x180;
         inline constexpr uintptr_t DisplayName = 0xb8;
         inline constexpr uintptr_t EvaluateStateMachine = 0x1d8;
         inline constexpr uintptr_t FloorMaterial = 0x184;
         inline constexpr uintptr_t Health = 0x188;
         inline constexpr uintptr_t HealthDisplayDistance = 0x18c;
         inline constexpr uintptr_t HealthDisplayType = 0x190;
         inline constexpr uintptr_t HipHeight = 0x194;
         inline constexpr uintptr_t HumanoidRootPart = 0x478;
         inline constexpr uintptr_t HumanoidState = 0x898;
         inline constexpr uintptr_t HumanoidStateID = 0x20;
         inline constexpr uintptr_t IsWalking = 0x917;
         inline constexpr uintptr_t Jump = 0x1da;
         inline constexpr uintptr_t JumpHeight = 0x1a0;
         inline constexpr uintptr_t JumpPower = 0x1a4;
         inline constexpr uintptr_t MaxHealth = 0x1a8;
         inline constexpr uintptr_t MaxSlopeAngle = 0x1ac;
         inline constexpr uintptr_t MoveDirection = 0x140;
         inline constexpr uintptr_t MoveToPart = 0x118;
         inline constexpr uintptr_t MoveToPoint = 0x164;
         inline constexpr uintptr_t NameDisplayDistance = 0x1b0;
         inline constexpr uintptr_t NameOcclusion = 0x1b4;
         inline constexpr uintptr_t PlatformStand = 0x1dc;
         inline constexpr uintptr_t RequiresNeck = 0x1dd;
         inline constexpr uintptr_t RigType = 0x1c0;
         inline constexpr uintptr_t SeatPart = 0x108;
         inline constexpr uintptr_t Sit = 0x1dd;
         inline constexpr uintptr_t TargetPoint = 0x14c;
         inline constexpr uintptr_t UseJumpPower = 0x1e0;
         inline constexpr uintptr_t WalkTimer = 0x408;
         inline constexpr uintptr_t walkspeed = 0x1d0;
         inline constexpr uintptr_t WalkspeedCheck = 0x3bc;
    }

    namespace instance {
         inline constexpr uintptr_t ChildrenEnd = 0x8;
         inline constexpr uintptr_t ChildrenStart = 0x70;
         inline constexpr uintptr_t ClassBase = 0x230;
         inline constexpr uintptr_t ClassDescriptor = 0x18;
         inline constexpr uintptr_t ClassName = 0x8;
         inline constexpr uintptr_t name = 0x98;
         inline constexpr uintptr_t parent = 0x68;
         inline constexpr uintptr_t This = 0x8;
    }

    namespace light {
         inline constexpr uintptr_t Ambient = 0xc8;
         inline constexpr uintptr_t Brightness = 0x110;
         inline constexpr uintptr_t ClockTime = 0x1a8;
         inline constexpr uintptr_t ColorShift_Bottom = 0xe0;
         inline constexpr uintptr_t ColorShift_Top = 0xd4;
         inline constexpr uintptr_t EnvironmentDiffuseScale = 0x114;
         inline constexpr uintptr_t EnvironmentSpecularScale = 0x118;
         inline constexpr uintptr_t ExposureCompensation = 0x11c;
         inline constexpr uintptr_t FogColor = 0xec;
         inline constexpr uintptr_t FogEnd = 0x124;
         inline constexpr uintptr_t FogStart = 0x128;
         inline constexpr uintptr_t GeographicLatitude = 0x180;
         inline constexpr uintptr_t GlobalShadows = 0x138;
         inline constexpr uintptr_t GradientBottom = 0x184;
         inline constexpr uintptr_t GradientTop = 0x140;
         inline constexpr uintptr_t LightColor = 0x14c;
         inline constexpr uintptr_t LightDirection = 0x158;
         inline constexpr uintptr_t MoonPosition = 0x174;
         inline constexpr uintptr_t OutdoorAmbient = 0xf8;
         inline constexpr uintptr_t sky = 0x1c8;
         inline constexpr uintptr_t Source = 0x164;
         inline constexpr uintptr_t SunPosition = 0x168;
    }

    namespace localscript {
         inline constexpr uintptr_t bytecode = 0x190;
         inline constexpr uintptr_t GUID = 0xd0;
         inline constexpr uintptr_t Hash = 0x1a0;
    }

    namespace material {
         inline constexpr uintptr_t Asphalt = 0x30;
         inline constexpr uintptr_t Basalt = 0x27;
         inline constexpr uintptr_t Brick = 0xf;
         inline constexpr uintptr_t Cobblestone = 0x33;
         inline constexpr uintptr_t Concrete = 0xc;
         inline constexpr uintptr_t CrackedLava = 0x2d;
         inline constexpr uintptr_t Glacier = 0x1b;
         inline constexpr uintptr_t Grass = 0x6;
         inline constexpr uintptr_t Ground = 0x2a;
         inline constexpr uintptr_t Ice = 0x36;
         inline constexpr uintptr_t LeafyGrass = 0x39;
         inline constexpr uintptr_t Limestone = 0x3f;
         inline constexpr uintptr_t Mud = 0x24;
         inline constexpr uintptr_t Pavement = 0x42;
         inline constexpr uintptr_t Rock = 0x18;
         inline constexpr uintptr_t Salt = 0x3c;
         inline constexpr uintptr_t Sand = 0x12;
         inline constexpr uintptr_t Sandstone = 0x21;
         inline constexpr uintptr_t Slate = 0x9;
         inline constexpr uintptr_t Snow = 0x1e;
         inline constexpr uintptr_t WoodPlanks = 0x15;
    }

    namespace meshprovider {
         inline constexpr uintptr_t AssetID = 0x10;
         inline constexpr uintptr_t cache = 0xf0;
         inline constexpr uintptr_t LRUCache = 0x20;
         inline constexpr uintptr_t meshdata = 0x40;
         inline constexpr uintptr_t ToMeshData = 0x40;
    }

    namespace meshdata {
         inline constexpr uintptr_t FaceEnd = 0x38;
         inline constexpr uintptr_t FaceStart = 0x30;
         inline constexpr uintptr_t VertexEnd = 0x8;
         inline constexpr uintptr_t VertexStart = 0x0;
    }

    namespace meshpart {
         inline constexpr uintptr_t MeshId = 0x290;
         inline constexpr uintptr_t Texture = 0x2c0;
    }

    namespace misc {
         inline constexpr uintptr_t Adornee = 0xf0;
         inline constexpr uintptr_t AnimationId = 0xc0;
         inline constexpr uintptr_t StringLength = 0x10;
         inline constexpr uintptr_t Value = 0xb8;
    }

    namespace model {
         inline constexpr uintptr_t PrimaryPart = 0x258;
         inline constexpr uintptr_t Scale = 0x144;
    }

    namespace modulescript {
         inline constexpr uintptr_t bytecode = 0x138;
         inline constexpr uintptr_t GUID = 0xd0;
         inline constexpr uintptr_t Hash = 0x148;
         inline constexpr uintptr_t IsCoreScript = 0x0;
    }

    namespace mouseservice {
         inline constexpr uintptr_t InputObject = 0xf0;
         inline constexpr uintptr_t InputObject2 = 0x100;
         inline constexpr uintptr_t MousePosition = 0xd4;
         inline constexpr uintptr_t SensitivityPointer = 0x7dfd318;
    }

    namespace particle {
         inline constexpr uintptr_t Acceleration = 0x1e0;
         inline constexpr uintptr_t Brightness = 0x21c;
         inline constexpr uintptr_t Drag = 0x220;
         inline constexpr uintptr_t Lifetime = 0x1f4;
         inline constexpr uintptr_t LightEmission = 0x238;
         inline constexpr uintptr_t LightInfluence = 0x23c;
         inline constexpr uintptr_t Rate = 0x248;
         inline constexpr uintptr_t RotSpeed = 0x1fc;
         inline constexpr uintptr_t Rotation = 0x204;
         inline constexpr uintptr_t Speed = 0x20c;
         inline constexpr uintptr_t SpreadAngle = 0x214;
         inline constexpr uintptr_t Texture = 0x1c0;
         inline constexpr uintptr_t TimeScale = 0x25c;
         inline constexpr uintptr_t VelocityInheritance = 0x260;
         inline constexpr uintptr_t ZOffset = 0x264;
    }

    namespace player {
         inline constexpr uintptr_t AccountAge = 0x34c;
         inline constexpr uintptr_t CameraMode = 0x360;
         inline constexpr uintptr_t DisplayName = 0x138;
         inline constexpr uintptr_t HealthDisplayDistance = 0x380;
         inline constexpr uintptr_t LocalPlayer = 0x130;
         inline constexpr uintptr_t LocaleId = 0x118;
         inline constexpr uintptr_t MaxZoomDistance = 0x358;
         inline constexpr uintptr_t MinZoomDistance = 0x35c;
         inline constexpr uintptr_t ModelInstance = 0x298;
         inline constexpr uintptr_t mouse = 0x11c8;
         inline constexpr uintptr_t NameDisplayDistance = 0x390;
         inline constexpr uintptr_t team = 0x2c8;
         inline constexpr uintptr_t TeamColor = 0x39c;
         inline constexpr uintptr_t UserId = 0x2f0;
    }

    namespace playerconfig {
         inline constexpr uintptr_t Pointer = 0x0;
    }

    namespace playermouse {
         inline constexpr uintptr_t icon = 0xc8;
         inline constexpr uintptr_t workspace = 0x150;
    }

    namespace primitive {
         inline constexpr uintptr_t AssemblyAngularVelocity = 0x104;
         inline constexpr uintptr_t AssemblyLinearVelocity = 0xf8;
         inline constexpr uintptr_t Flags = 0x1b6;
         inline constexpr uintptr_t Material = 0x0;
         inline constexpr uintptr_t Owner = 0x208;
         inline constexpr uintptr_t Position = 0xec;
         inline constexpr uintptr_t Rotation = 0xc8;
         inline constexpr uintptr_t Size = 0x1b8;
         inline constexpr uintptr_t Validate = 0x6;
    }

    namespace primitiveflag {
         inline constexpr uintptr_t Anchored = 0x2;
         inline constexpr uintptr_t CanCollide = 0x8;
         inline constexpr uintptr_t CanQuery = 0x20;
         inline constexpr uintptr_t CanTouch = 0x10;
    }

    namespace prompt {
         inline constexpr uintptr_t ActionText = 0xb0;
         inline constexpr uintptr_t Enabled = 0x136;
         inline constexpr uintptr_t GamepadKeyCode = 0x11c;
         inline constexpr uintptr_t HoldDuration = 0x120;
         inline constexpr uintptr_t KeyCode = 0x124;
         inline constexpr uintptr_t MaxActivationDistance = 0x128;
         inline constexpr uintptr_t ObjectText = 0xd0;
         inline constexpr uintptr_t RequiresLineOfSight = 0x137;
    }

    namespace job {
         inline constexpr uintptr_t fakemodel = 0x38;
         inline constexpr uintptr_t RealDataModel = 0x1c8;
         inline constexpr uintptr_t view = 0x1d0;
    }

    namespace view {
         inline constexpr uintptr_t DeviceD3D11 = 0x8;
         inline constexpr uintptr_t LightingValid = 0x150;
         inline constexpr uintptr_t SkyValid = 0x28d;
         inline constexpr uintptr_t render = 0x10;
    }

    namespace run {
         inline constexpr uintptr_t HeartbeatFPS = 0xbc;
         inline constexpr uintptr_t HeartbeatTask = 0x1b0;
    }

    namespace script {
         inline constexpr uintptr_t bytecode = 0x190;
         inline constexpr uintptr_t GUID = 0xd0;
         inline constexpr uintptr_t Hash = 0x1a0;
    }

    namespace scriptcontext {
         inline constexpr uintptr_t RequireBypass = 0x0;
    }

    namespace seat {
         inline constexpr uintptr_t Occupant = 0x1b0;
    }

    namespace sky {
         inline constexpr uintptr_t MoonAngularSize = 0x244;
         inline constexpr uintptr_t MoonTextureId = 0xc8;
         inline constexpr uintptr_t SkyboxBk = 0xf8;
         inline constexpr uintptr_t SkyboxDn = 0x128;
         inline constexpr uintptr_t SkyboxFt = 0x158;
         inline constexpr uintptr_t SkyboxLf = 0x188;
         inline constexpr uintptr_t SkyboxOrientation = 0x238;
         inline constexpr uintptr_t SkyboxRt = 0x1b8;
         inline constexpr uintptr_t SkyboxUp = 0x1e8;
         inline constexpr uintptr_t StarCount = 0x248;
         inline constexpr uintptr_t SunAngularSize = 0x23c;
         inline constexpr uintptr_t SunTextureId = 0x218;
    }

    namespace sound {
         inline constexpr uintptr_t Looped = 0x13d;
         inline constexpr uintptr_t PlaybackSpeed = 0x11c;
         inline constexpr uintptr_t RollOffMaxDistance = 0x120;
         inline constexpr uintptr_t RollOffMinDistance = 0x124;
         inline constexpr uintptr_t SoundGroup = 0xe8;
         inline constexpr uintptr_t SoundId = 0xc8;
         inline constexpr uintptr_t Volume = 0x130;
    }

    namespace spawn {
         inline constexpr uintptr_t AllowTeamChangeOnTouch = 0x188;
         inline constexpr uintptr_t Enabled = 0x189;
         inline constexpr uintptr_t ForcefieldDuration = 0x180;
         inline constexpr uintptr_t Neutral = 0x18a;
         inline constexpr uintptr_t TeamColor = 0x184;
    }

    namespace specialmesh {
         inline constexpr uintptr_t MeshId = 0xf8;
         inline constexpr uintptr_t Scale = 0xc4;
    }

    namespace stat {
         inline constexpr uintptr_t Value = 0xc8;
    }

    namespace sunray {
         inline constexpr uintptr_t Enabled = 0xb0;
         inline constexpr uintptr_t Intensity = 0xb8;
         inline constexpr uintptr_t Spread = 0xbc;
    }

    namespace surface {
         inline constexpr uintptr_t AlphaMode = 0x288;
         inline constexpr uintptr_t tocolor = 0x270;
         inline constexpr uintptr_t ColorMap = 0xc8;
         inline constexpr uintptr_t EmissiveMaskContent = 0xf8;
         inline constexpr uintptr_t EmissiveStrength = 0x28c;
         inline constexpr uintptr_t EmissiveTint = 0x27c;
         inline constexpr uintptr_t MetalnessMap = 0x128;
         inline constexpr uintptr_t NormalMap = 0x158;
         inline constexpr uintptr_t RoughnessMap = 0x188;
    }

    namespace task {
         inline constexpr uintptr_t JobEnd = 0xd0;
         inline constexpr uintptr_t JobName = 0x18;
         inline constexpr uintptr_t JobStart = 0xc8;
         inline constexpr uintptr_t MaxFPS = 0xb0;
         inline constexpr uintptr_t Pointer = 0x81cc868;
    }

    namespace team {
         inline constexpr uintptr_t BrickColor = 0xb8;
    }

    namespace terrain {
         inline constexpr uintptr_t GrassLength = 0x188;
         inline constexpr uintptr_t material = 0x438;
         inline constexpr uintptr_t WaterColor = 0x178;
         inline constexpr uintptr_t WaterReflectance = 0x190;
         inline constexpr uintptr_t WaterTransparency = 0x194;
         inline constexpr uintptr_t WaterWaveSize = 0x198;
         inline constexpr uintptr_t WaterWaveSpeed = 0x19c;
    }

    namespace texture {
         inline constexpr uintptr_t Decal_Texture = 0x180;
         inline constexpr uintptr_t Texture_Texture = 0x180;
    }

    namespace tool {
         inline constexpr uintptr_t CanBeDropped = 0x4b8;
         inline constexpr uintptr_t Enabled = 0x4b9;
         inline constexpr uintptr_t Grip = 0x4ac;
         inline constexpr uintptr_t ManualActivationOnly = 0x4ba;
         inline constexpr uintptr_t RequiresHandle = 0x4bb;
         inline constexpr uintptr_t TextureId = 0x360;
         inline constexpr uintptr_t Tooltip = 0x468;
    }

    namespace unionop {
         inline constexpr uintptr_t AssetId = 0x288;
    }

    namespace input {
         inline constexpr uintptr_t inputstate = 0x2c0;
    }

    namespace vehicle {
         inline constexpr uintptr_t MaxSpeed = 0x1c8;
         inline constexpr uintptr_t SteerFloat = 0x1d0;
         inline constexpr uintptr_t ThrottleFloat = 0x1d8;
         inline constexpr uintptr_t Torque = 0x1dc;
         inline constexpr uintptr_t TurnSpeed = 0x1e0;
    }

    namespace render {
         inline constexpr uintptr_t Dimensions = 0xab0;
         inline constexpr uintptr_t fakemodel = 0xa90;
         inline constexpr uintptr_t Pointer = 0x835a548;
         inline constexpr uintptr_t view = 0xbb8;
         inline constexpr uintptr_t ViewMatrix = 0x150;
    }

    namespace weld {
         inline constexpr uintptr_t Part0 = 0x118;
         inline constexpr uintptr_t Part1 = 0x128;
    }

    namespace weldconstraint {
         inline constexpr uintptr_t Part0 = 0xb8;
         inline constexpr uintptr_t Part1 = 0xc8;
    }

    namespace inputstate {
         inline constexpr uintptr_t CapsLock = 0x40;
         inline constexpr uintptr_t CurrentTextBox = 0x48;
    }

    namespace workspace {
         inline constexpr uintptr_t CurrentCamera = 0x4a0;
         inline constexpr uintptr_t DistributedGameTime = 0x4c0;
         inline constexpr uintptr_t ReadOnlyGravity = 0x9b0;
         inline constexpr uintptr_t world = 0x3f8;
    }

    namespace world {
         inline constexpr uintptr_t air = 0x218;
         inline constexpr uintptr_t FallenPartsDestroyHeight = 0x208;
         inline constexpr uintptr_t Gravity = 0x210;
         inline constexpr uintptr_t Primitives = 0x288;
         inline constexpr uintptr_t worldStepsPerSec = 0x680;
    }

    namespace silent {
         inline constexpr uintptr_t FramePositionOffsetX = 0x524;
         inline constexpr uintptr_t FramePositionOffsetY = 0x52C;
         inline constexpr uintptr_t FramePositionX = 0x520;
         inline constexpr uintptr_t FramePositionY = 0x528;
         inline constexpr uintptr_t FrameRotation = 0x178;
         inline constexpr uintptr_t FrameSizeOffsetX = 0x540;
         inline constexpr uintptr_t FrameSizeOffsetY = 0x544;
         inline constexpr uintptr_t FrameSizeX = 0x538;
         inline constexpr uintptr_t FrameSizeY = 0x53C;
    }

    namespace flag {
         inline constexpr uintptr_t List = 0x7ce33d8;
         inline constexpr uintptr_t ToValueGetSet = 0x30;
    }

    namespace value {
         inline constexpr uintptr_t ToValue = 0xc0;
    }

}
