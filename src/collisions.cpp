#include "../include/collisions.h"
#include "entities/NPC.h"
#include <algorithm>
#include <glm/glm.hpp>

CollisionResult UpdatePlayerCollision(glm::vec2 camera_pos, float player_radius, const CanonicalCorridorLayout& corridor_layout, float kCorridorHalfWidth, float kCorridorZ1) {
    const float first_x_limit = kCorridorHalfWidth - player_radius;
    const float first_z_max = 0.8f;

    const glm::vec2 block_offset = corridor_layout.block_offset;
    const float connector_length = corridor_layout.connector_length;
    const float connector_half_width = kCorridorHalfWidth - player_radius;
    const float joint_overlap = player_radius * 2.0f;
    const float turn_z0 = corridor_layout.turn_z0;
    const float turn_z1 = corridor_layout.turn_z1;
    const float connector_center_z = corridor_layout.connector_center_z;
    const float connector_start_x = corridor_layout.connector_start_x;
    const float connector_end_x = corridor_layout.connector_end_x;
    const float exit_turn_x = corridor_layout.exit_turn_x;

    // Todas as caixas abaixo estão no espaço local do "Bloco 0" (tile central).
    const WalkableBox2D corridor1 = {-first_x_limit, +first_x_limit, kCorridorZ1 - joint_overlap, first_z_max};
    const WalkableBox2D corner_left_1 = {-kCorridorHalfWidth, +kCorridorHalfWidth,
                                         turn_z1 + player_radius, turn_z0 + joint_overlap};
    const WalkableBox2D connector_corridor = {connector_end_x - joint_overlap, connector_start_x + joint_overlap,
                                              connector_center_z - connector_half_width,
                                              connector_center_z + connector_half_width};
    const WalkableBox2D corner_right_1 = {exit_turn_x - kCorridorHalfWidth, exit_turn_x + kCorridorHalfWidth,
                                          turn_z1 - joint_overlap, turn_z0 + joint_overlap};
    const WalkableBox2D walkable_boxes[] = {
        corridor1,
        corner_left_1,
        connector_corridor,
        corner_right_1
    };

    auto clampf = [](float value, float min_value, float max_value) {
        return std::max(min_value, std::min(max_value, value));
    };

    auto inside_box = [](const WalkableBox2D& box, float x, float z) {
        return x >= box.min_x && x <= box.max_x && z >= box.min_z && z <= box.max_z;
    };

    auto closest_point = [&](const WalkableBox2D& box, glm::vec2 p) {
        return glm::vec2(clampf(p.x, box.min_x, box.max_x),
                         clampf(p.y, box.min_z, box.max_z));
    };

    glm::vec2 p = camera_pos;

    // Collision wrapping: mapeia a posição do mundo para o espaço local do Bloco 0
    int block_index = 0;
    const float block_length = glm::length(block_offset);
    const glm::vec2 block_dir = block_offset / block_length;
    const float wrap_forward_progress = glm::dot(block_offset + glm::vec2(0.0f, player_radius), block_dir);
    const float wrap_backward_progress = glm::dot(glm::vec2(0.0f, first_z_max), block_dir);

    while (glm::dot(p, block_dir) > wrap_forward_progress) {
        p -= block_offset;
        ++block_index;
    }
    while (glm::dot(p, block_dir) < wrap_backward_progress) {
        p += block_offset;
        --block_index;
    }

    bool inside_any_box = false;
    for (const WalkableBox2D& box : walkable_boxes) {
        if (inside_box(box, p.x, p.y)) {
            inside_any_box = true;
            break;
        }
    }

    if (!inside_any_box) {
        glm::vec2 best = closest_point(walkable_boxes[0], p);
        float best_dist2 = (best.x - p.x) * (best.x - p.x) + (best.y - p.y) * (best.y - p.y);

        for (const WalkableBox2D& box : walkable_boxes) {
            glm::vec2 candidate = closest_point(box, p);
            float dist2 = (candidate.x - p.x) * (candidate.x - p.x) + (candidate.y - p.y) * (candidate.y - p.y);
            if (dist2 < best_dist2) {
                best = candidate;
                best_dist2 = dist2;
            }
        }
        p = best;
    }

    glm::vec2 p_world = p + (float)block_index * block_offset;

    // Colisão dinâmica com o NPC
    if (g_SalarymanNPC.active) {
        glm::vec2 npc_pos(g_SalarymanNPC.position.x, g_SalarymanNPC.position.z);
        glm::vec2 diff = p_world - npc_pos;
        float dist = glm::length(diff);
        float npc_radius = g_SalarymanNPC.isGiant ? 0.6f : 0.4f;
        float min_dist = player_radius + npc_radius;
        
        if (dist < min_dist && dist > 0.0001f) {
            p_world = npc_pos + glm::normalize(diff) * min_dist;
            
            // Re-clampar contra as paredes estáticas para evitar que o NPC empurre o jogador para fora do cenário
            glm::vec2 p_local = p_world - (float)block_index * block_offset;
            bool inside_any_box_again = false;
            for (const WalkableBox2D& box : walkable_boxes) {
                if (inside_box(box, p_local.x, p_local.y)) {
                    inside_any_box_again = true;
                    break;
                }
            }
            if (!inside_any_box_again) {
                glm::vec2 best = closest_point(walkable_boxes[0], p_local);
                float best_dist2 = (best.x - p_local.x) * (best.x - p_local.x) + (best.y - p_local.y) * (best.y - p_local.y);

                for (const WalkableBox2D& box : walkable_boxes) {
                    glm::vec2 candidate = closest_point(box, p_local);
                    float dist2 = (candidate.x - p_local.x) * (candidate.x - p_local.x) + (candidate.y - p_local.y) * (candidate.y - p_local.y);
                    if (dist2 < best_dist2) {
                        best = candidate;
                        best_dist2 = dist2;
                    }
                }
                p_local = best;
            }
            p_world = p_local + (float)block_index * block_offset;
            p = p_local; // Atualizar p para computar as flags de resultado corretamente abaixo
        }
    }

    // Popula e retorna a estrutura com todos os resultados necessários para o main
    CollisionResult result;
    result.p_world = p_world;
    result.block_index = block_index;
    
    result.inside_straight_corridor = inside_box(corridor1, p.x, p.y);
    result.inside_shared_connector = inside_box(connector_corridor, p.x, p.y);
    result.inside_entry_turn = inside_box(corner_left_1, p.x, p.y);
    result.inside_exit_turn = inside_box(corner_right_1, p.x, p.y);
    
    result.inside_connector_turn = false;
    for (size_t i = 1; i < sizeof(walkable_boxes) / sizeof(walkable_boxes[0]); ++i) {
        if (inside_box(walkable_boxes[i], p.x, p.y)) {
            result.inside_connector_turn = true;
            break;
        }
    }

    result.connector_progress = 0.0f;
    if (result.inside_shared_connector)
        result.connector_progress = clampf((connector_start_x - p.x) / connector_length, 0.0f, 1.0f);
    else if (result.inside_exit_turn)
        result.connector_progress = 1.0f;
    else if (result.inside_entry_turn)
        result.connector_progress = 0.0f;

    return result;
}
