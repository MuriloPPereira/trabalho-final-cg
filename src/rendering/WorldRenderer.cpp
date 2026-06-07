#include "rendering/WorldRenderer.h"

#include "engine/Renderer.h"
#include "engine/Shader.h"
#include "matrices.h"
#include "utils/Constants.h"
#include "world/Corridor.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <string>

std::vector<PointLight> CreateCorridorLights() {
  const CanonicalCorridorLayout corridor_layout = GetCanonicalCorridorLayout();
  const float connector_length = corridor_layout.connector_length;
  const glm::vec2 block_offset = corridor_layout.block_offset;
  const float turn_z0 = corridor_layout.turn_z0;
  const float connector_center_z = corridor_layout.connector_center_z;
  const float connector_start_x = corridor_layout.connector_start_x;
  const float exit_turn_x = corridor_layout.exit_turn_x;

  std::vector<PointLight> corridor_lights;
  corridor_lights.reserve(kMaxLights);

  auto make_light = [&](const glm::vec3 &position) {
    PointLight light;
    light.position = position;
    light.color = glm::vec3(1.0f, 0.98f, 0.92f);
    light.ambient_strength = 0.03f;
    light.diffuse_strength = 1.0f;
    light.specular_strength = 1.0f;
    light.constant = 1.0f;
    light.linear = 0.14f;
    light.quadratic = 0.07f;
    return light;
  };

  auto make_block_light = [&](const glm::mat4 &block_transform,
                              const glm::vec3 &local_position) {
    glm::vec4 world_position =
        block_transform *
        glm::vec4(local_position.x, local_position.y, local_position.z, 1.0f);
    return make_light(
        glm::vec3(world_position.x, world_position.y, world_position.z));
  };

  // Lambda to generate the lights for one complete modular block.
  auto add_block_lights = [&](const glm::mat4 &block_transform) {
    const float straight_spacing = kCorridorLength / 5.0f;
    for (int i = 0; i < 4; ++i) {
      corridor_lights.push_back(make_block_light(
          block_transform, glm::vec3(0.0f, kCorridorHeight - 0.15f,
                                     -(i + 1) * straight_spacing)));
    }

    corridor_lights.push_back(make_block_light(
        block_transform, glm::vec3(0.0f, kCorridorHeight - 0.15f,
                                   turn_z0 - 0.5f * kCornerLength)));
    corridor_lights.push_back(make_block_light(
        block_transform,
        glm::vec3(connector_start_x - 0.5f * connector_length,
                  kCorridorHeight - 0.15f, connector_center_z)));
    corridor_lights.push_back(make_block_light(
        block_transform, glm::vec3(exit_turn_x, kCorridorHeight - 0.15f,
                                   turn_z0 - 0.5f * kCornerLength)));
  };

  // Apply lights to the three physical treadmill tiles.
  add_block_lights(Matrix_Translate(
      -block_offset.x, 0.0f, -block_offset.y)); // Previous-side candidate
  add_block_lights(Matrix_Identity());          // Current physical block
  add_block_lights(Matrix_Translate(block_offset.x, 0.0f,
                                    block_offset.y)); // Next-side candidate


  return corridor_lights;
}

void DrawCorridorTreadmill(const Material &floor_material,
                           const Material &ceiling_material,
                           const Material &wall_material,
                           const std::vector<Material> &poster_materials,
                           const Material &doorway_placeholder_material) {
  const CanonicalCorridorLayout corridor_layout = GetCanonicalCorridorLayout();
  const float connector_length = corridor_layout.connector_length;
  const glm::vec2 block_offset = corridor_layout.block_offset;
  const float turn_z0 = corridor_layout.turn_z0;
  const float connector_center_z = corridor_layout.connector_center_z;
  const float connector_start_x = corridor_layout.connector_start_x;
  const float exit_turn_x = corridor_layout.exit_turn_x;

    auto draw_straight_corridor =
        [&](const CorridorRenderTransform &corridor_render,
            const glm::mat4 &content_placement, bool draw_posters,
            const CorridorInstance &corridor_instance,
            const Material &floor_mat, const Material &ceiling_mat,
            const Material &wall_mat, float segment_start_distance,
            float segment_length_scale) {
          const glm::mat4 &corridor_model = corridor_render.geometryFromLocal;
          glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE,
                             glm::value_ptr(corridor_model));

          Material floor_instance = floor_mat;
          floor_instance.uv_scale.y *= segment_length_scale;
          floor_instance.uv_offset =
              glm::vec2(0.0f, segment_start_distance / kFloorTileSize);
          ApplyMaterial(floor_instance);
          DrawVirtualObject("corridor_floor");

          Material ceiling_instance = ceiling_mat;
          ceiling_instance.uv_scale.y *= segment_length_scale;
          ceiling_instance.uv_offset =
              glm::vec2(0.0f, segment_start_distance / kCeilingTileSize);
          ApplyMaterial(ceiling_instance);
          DrawVirtualObject("corridor_ceiling");

          Material wall_instance = wall_mat;
          wall_instance.uv_scale.x *= segment_length_scale;
          wall_instance.uv_offset =
              glm::vec2(segment_start_distance / kWallTextureTileSize, 0.0f);
          ApplyMaterial(wall_instance);

          bool draw_doorways = draw_posters;
          bool doorways_on_positive_x_wall = true;
          if (draw_doorways) {
            const glm::vec3 content_right =
                corridor_instance.content.frame.contentRight;
            doorways_on_positive_x_wall = (content_right.x >= 0.0f);
          }

          auto draw_left_wall_with_doorways = []() {
            for (int span = 0; span <= kDoorwayCount; ++span) {
              const std::string name = "corridor_wall_left_doorway_span_" +
                                       std::to_string(span);
              DrawVirtualObject(name.c_str());
            }
            for (int slot = 0; slot < kDoorwayCount; ++slot) {
              const std::string suffix = "_" + std::to_string(slot);
              DrawVirtualObject(
                  ("corridor_wall_left_doorway_top" + suffix).c_str());
              DrawVirtualObject(
                  ("corridor_wall_left_doorway_reveal_low" + suffix).c_str());
              DrawVirtualObject(
                  ("corridor_wall_left_doorway_reveal_high" + suffix).c_str());
              DrawVirtualObject(
                  ("corridor_wall_left_doorway_reveal_top" + suffix).c_str());
            }
          };

          auto draw_right_wall_with_doorways = []() {
            for (int span = 0; span <= kDoorwayCount; ++span) {
              const std::string name = "corridor_wall_right_doorway_span_" +
                                       std::to_string(span);
              DrawVirtualObject(name.c_str());
            }
            for (int slot = 0; slot < kDoorwayCount; ++slot) {
              const std::string suffix = "_" + std::to_string(slot);
              DrawVirtualObject(
                  ("corridor_wall_right_doorway_top" + suffix).c_str());
              DrawVirtualObject(
                  ("corridor_wall_right_doorway_reveal_low" + suffix).c_str());
              DrawVirtualObject(
                  ("corridor_wall_right_doorway_reveal_high" + suffix).c_str());
              DrawVirtualObject(
                  ("corridor_wall_right_doorway_reveal_top" + suffix).c_str());
            }
          };

          if (draw_doorways && !doorways_on_positive_x_wall)
            draw_left_wall_with_doorways();
          else
            DrawVirtualObject("corridor_wall_left");

          if (draw_doorways && doorways_on_positive_x_wall)
            draw_right_wall_with_doorways();
          else
            DrawVirtualObject("corridor_wall_right");

          if (draw_posters) {
            const CorridorContent &content = corridor_instance.content;

            for (const PosterSlotLayout &poster : content.posters) {
              glm::mat4 poster_basis =
                  Matrix(poster.normal.x, poster.up.x, poster.widthAxis.x,
                         poster.position.x, poster.normal.y, poster.up.y,
                         poster.widthAxis.y, poster.position.y, poster.normal.z,
                         poster.up.z, poster.widthAxis.z, poster.position.z,
                         0.0f, 0.0f, 0.0f, 1.0f);
              glm::mat4 poster_model = content_placement * poster_basis;
              glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE,
                                 glm::value_ptr(poster_model));

              ApplyMaterial(poster_materials[poster.textureIndex]);
              DrawVirtualObject(kPosterNames[poster.slot]);
            }
          }

          if (draw_doorways) {
            ApplyMaterial(doorway_placeholder_material);
            for (const DoorInstance &doorway :
                 corridor_instance.content.doorways) {
              const glm::vec3 up_axis = doorway.up * doorway.height;
              const glm::vec3 width_axis =
                  doorway.widthAxis * doorway.width;
              const glm::mat4 doorway_basis = Matrix(
                  doorway.normal.x, up_axis.x, width_axis.x,
                  doorway.position.x, doorway.normal.y, up_axis.y,
                  width_axis.y, doorway.position.y, doorway.normal.z,
                  up_axis.z, width_axis.z, doorway.position.z, 0.0f, 0.0f,
                  0.0f, 1.0f);
              const glm::mat4 doorway_model =
                  content_placement * doorway_basis;
              glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE,
                                 glm::value_ptr(doorway_model));
              DrawVirtualObject("doorway_placeholder_panel");
            }
          }
        };

    Material corner_floor_material = floor_material;
    Material corner_ceiling_material = ceiling_material;
    Material corner_wall_material = wall_material;

    auto make_length_offset_material = [&](const Material &base_material,
                                           float length_offset, bool offset_u,
                                           float tile_size) {
      Material instance = base_material;
      if (offset_u)
        instance.uv_offset.x = length_offset / tile_size;
      else
        instance.uv_offset.y = length_offset / tile_size;
      return instance;
    };

    auto draw_corner = [&](const glm::mat4 &corner_model,
                           const char *floor_name, const char *ceiling_name,
                           const char *wall_a_name, const char *wall_b_name,
                           float length_offset, bool wall_a_uses_length_axis,
                           bool wall_b_uses_length_axis) {
      glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE,
                         glm::value_ptr(corner_model));

      ApplyMaterial(make_length_offset_material(
          corner_floor_material, length_offset, false, kFloorTileSize));
      DrawVirtualObject(floor_name);

      ApplyMaterial(make_length_offset_material(
          corner_ceiling_material, length_offset, false, kCeilingTileSize));
      DrawVirtualObject(ceiling_name);

      ApplyMaterial(make_length_offset_material(
          corner_wall_material, wall_a_uses_length_axis ? length_offset : 0.0f,
          true, kWallTextureTileSize));
      DrawVirtualObject(wall_a_name);

      ApplyMaterial(make_length_offset_material(
          corner_wall_material, wall_b_uses_length_axis ? length_offset : 0.0f,
          true, kWallTextureTileSize));
      DrawVirtualObject(wall_b_name);
    };

    // (1) Modular block (tile): corredor reto + quina esquerda + conector +
    // quina direita. O próximo tile começa em (corridor2_offset_x,
    // second_corridor_z_offset) relativo ao tile atual.
    auto draw_modular_block = [&](const CorridorRenderTransform &block_render,
                                  const glm::mat4 &content_placement,
                                  const CorridorInstance &corridor_instance) {
      const glm::mat4 &base_transform = block_render.geometryFromLocal;
      const float straight_start = 0.0f;
      const float corner1_start = kCorridorLength;
      const float connector1_start = corner1_start + kCornerLength;
      const float corner2_start = connector1_start + connector_length;
      const float full_segment_scale = 1.0f;
      const float connector_segment_scale = connector_length / kCorridorLength;

      // Corredor reto principal (eixo -Z) deste tile.
      draw_straight_corridor(block_render, content_placement, true,
                             corridor_instance, floor_material,
                             ceiling_material, wall_material, straight_start,
                             full_segment_scale);

      // Quina esquerda no final do corredor: base_transform *
      // T(0,0,kCorridorZ1).
      glm::mat4 m = base_transform * Matrix_Translate(0.0f, 0.0f, turn_z0);
      draw_corner(m, "corner_left_floor", "corner_left_ceiling",
                  "corner_left_wall_back", "corner_left_wall_right",
                  corner1_start, false, true);

      // Conector curto (eixo -X): base_transform * T(...) * R_y(+90°) *
      // S(...).
      m = base_transform *
          Matrix_Translate(connector_start_x, 0.0f, connector_center_z) *
          Matrix_Rotate_Y(3.141592f / 2.0f) *
          Matrix_Scale(1.0f, 1.0f, connector_length / kCorridorLength);
      draw_straight_corridor(MakeCorridorRenderTransform(m), content_placement,
                             false, corridor_instance, floor_material,
                             ceiling_material, wall_material, connector1_start,
                             connector_segment_scale);

      // Quina direita no fim do conector: base_transform *
      // T(corridor2_offset_x,0,kCorridorZ1).
      m = base_transform * Matrix_Translate(exit_turn_x, 0.0f, turn_z0);
      draw_corner(m, "corner_right_floor", "corner_right_ceiling",
                  "corner_right_wall_front", "corner_right_wall_left",
                  corner2_start, false, true);
    };

    // (2) 3-Tile Treadmill: side blocks are the canonical previous/next
    // candidates.
    const glm::mat4 negative_slot_transform =
        Matrix_Translate(-block_offset.x, 0.0f, -block_offset.y);
    const glm::mat4 current_slot_transform = Matrix_Identity();
    const glm::mat4 positive_slot_transform =
        Matrix_Translate(block_offset.x, 0.0f, block_offset.y);

    draw_modular_block(MakeCorridorRenderTransform(negative_slot_transform),
                       negative_slot_transform,
                       g_NegativeCandidateCorridorInstance);
    draw_modular_block(MakeCorridorRenderTransform(current_slot_transform),
                       current_slot_transform, g_CurrentCorridorInstance);
    draw_modular_block(MakeCorridorRenderTransform(positive_slot_transform),
                       positive_slot_transform,
                       g_PositiveCandidateCorridorInstance);
}
