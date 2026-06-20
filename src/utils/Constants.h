#ifndef UTILS_CONSTANTS_H
#define UTILS_CONSTANTS_H

#ifndef ENABLE_CORRIDOR_DEBUG_LOGS
#define ENABLE_CORRIDOR_DEBUG_LOGS 0
#endif

inline constexpr bool kCorridorDebugLogsEnabled = (ENABLE_CORRIDOR_DEBUG_LOGS != 0);

inline constexpr int kPosterCount = 4;
inline constexpr int kScaryPosterSlot = 2;
inline constexpr int kScaryPosterTextureIndex = kPosterCount;
inline constexpr int kMaxLights = 30;
inline constexpr int kMaxSalarymanBones = 100;
inline constexpr float kThirdPersonShiftSprintMultiplier = 2.0f;
inline constexpr float kCamouflagedPursuerSprintSpeedRatio = 0.70f;

inline constexpr float kCorridorHalfWidth = 2.0f;
inline constexpr float kCorridorHeight = 3.0f;
inline constexpr float kCorridorLength = 40.0f;
inline constexpr float kCorridorZ0 = 0.0f;
inline constexpr float kCorridorZ1 = -kCorridorLength;
inline constexpr float kCornerLength = 4.0f;
inline constexpr float kConnectorLength = kCorridorLength * 0.5f;
inline constexpr float kFloorTileSize = 0.5f;
inline constexpr float kCeilingTileSize = 2.5f;
inline constexpr float kWallTextureTileSize = 2.0f;
inline constexpr int kDoorwayCount = 3;
inline constexpr float kDoorwayDistanceFractions[kDoorwayCount] = {0.50f,
                                                                   0.70f,
                                                                   0.76f};
inline constexpr float kDoorwayOpeningWidth = 1.35f;
inline constexpr float kDoorwayOpeningHeight = 2.20f;
inline constexpr float kDoorwayRecessDepth = 0.16f;
inline constexpr float kDoorwayPanelInset = 0.01f;
inline constexpr float kNoSmokingSignWidth = 0.62f;
inline constexpr float kNoSmokingSignHeight = 0.62f;
inline constexpr float kNoSmokingSignCenterY = 1.55f;
inline constexpr float kNoSmokingSignStartDistance = 3.0f;
inline constexpr float kNoSmokingSignWallOffset = 0.022f;
inline constexpr float kNoSmokingSignLayerOffset = 0.00012f;
inline constexpr int kNoSmokingAnomalySignCount = 260;
inline constexpr int kExitSignCount = 9;
inline constexpr float kExitSignWidth = 0.90f;
inline constexpr float kExitSignHeight = 1.50f;
inline constexpr float kExitSignBottomY = 0.70f;
inline constexpr float kExitSignEntranceDistance = 1.10f;
inline constexpr float kExitSignWallOffset = 0.024f;
inline constexpr float kCamouflagedPursuerEndWallClearance = 0.02f;
inline constexpr float kCamouflagedPursuerIdleWallEmbedDepth = 0.20f;
inline constexpr float kCamouflagedPursuerTriggerRadius = 8.0f;
inline constexpr float kCamouflagedPursuerStopDistance = 0.75f;
inline const char *const kCamouflagedPursuerFbxPath =
    "assets/perseguidor_run.fbx";

inline const char *const kPosterNames[kPosterCount] = {"poster_0", "poster_1", "poster_2", "poster_3"};

#endif
