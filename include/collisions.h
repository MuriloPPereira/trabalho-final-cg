
#ifndef COLLISIONS_H
#define COLLISIONS_H

#include <array>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

constexpr int kCorridorWalkableSectionCount = 5;

struct CanonicalCorridorLayout
{
    float connector_length;
    float corridor2_offset_x;
    float second_corridor_z_offset;
    glm::vec2 block_offset;
    float turn_z0;
    float turn_z1;
    float connector_center_z;
    float connector_start_x;
    float connector_end_x;
    float exit_turn_x;
};

struct WalkableBox2D
{
    float min_x;
    float max_x;
    float min_z;
    float max_z;
};

struct CollisionResult {
    glm::vec2 p_world;
    int block_index;
    bool inside_straight_corridor;
    bool inside_shared_connector;
    bool inside_entry_turn;
    bool inside_exit_turn;
    bool inside_connector_turn;
    float connector_progress;
};


std::array<WalkableBox2D, kCorridorWalkableSectionCount>
GetCorridorWalkableSections(const CanonicalCorridorLayout &layout,
                            float corridor_half_width, float corridor_z1,
                            float entity_radius);

CollisionResult UpdatePlayerCollision(
    glm::vec2 camera_pos,
    float player_radius,
    const CanonicalCorridorLayout& layout,
    float kCorridorHalfWidth, float kCorridorZ1);

bool IsPointInsideWalkableBox(const WalkableBox2D &box, const glm::vec2 &p);

bool IsPointInsideStaticWalkable(const glm::vec2 &world_point, float radius,
                                 const CanonicalCorridorLayout &layout);

glm::vec2 ClampPointToStaticWalkable(const glm::vec2 &world_point, float radius,
                                     const CanonicalCorridorLayout &layout);

float FindVisibleThirdPersonBoomFraction(const glm::vec2 &target_ground,
                                         const glm::vec2 &desired_ground,
                                         float radius,
                                         const CanonicalCorridorLayout &layout);

bool ArePointsInSameWalkableSection(const glm::vec2 &a, const glm::vec2 &b,
                                    const CanonicalCorridorLayout &layout);

struct CamouflagedPursuerState;
bool HasCamouflagedPursuerCaughtPlayer(const CamouflagedPursuerState &pursuer,
                                       const glm::vec3 &player_position);

#endif
