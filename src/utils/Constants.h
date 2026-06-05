#ifndef UTILS_CONSTANTS_H
#define UTILS_CONSTANTS_H

#ifndef ENABLE_CORRIDOR_DEBUG_LOGS
#define ENABLE_CORRIDOR_DEBUG_LOGS 0
#endif

inline constexpr bool kCorridorDebugLogsEnabled = (ENABLE_CORRIDOR_DEBUG_LOGS != 0);

inline constexpr int kPosterCount = 4;
inline constexpr int kMaxLights = 30;
inline constexpr int kMaxSalarymanBones = 100;

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

inline const char *const kPosterNames[kPosterCount] = {"poster_0", "poster_1", "poster_2", "poster_3"};

#endif
