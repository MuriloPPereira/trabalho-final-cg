#ifndef RENDERING_WORLDRENDERER_H
#define RENDERING_WORLDRENDERER_H

#include "rendering/Material.h"
#include <vector>

std::vector<PointLight> CreateCorridorLights();
void DrawCorridorTreadmill(const Material &floor_material,
                           const Material &tactile_straight_material,
                           const Material &tactile_dots_material,
                           const Material &ceiling_material,
                           const Material &wall_material,
                           const std::vector<Material> &poster_materials,
                           const Material &no_smoking_sign_material,
                           const Material &doorway_placeholder_material,
                           const std::vector<Material> &exit_sign_materials);

#endif
