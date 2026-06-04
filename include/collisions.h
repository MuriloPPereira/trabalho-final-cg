
#ifndef COLLISIONS_H
#define COLLISIONS_H

#include <glm/vec2.hpp>

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



CollisionResult UpdatePlayerCollision(
    glm::vec2 camera_pos,
    float player_radius,
    const CanonicalCorridorLayout& layout,
    float kCorridorHalfWidth, float kCorridorZ1);

#endif 