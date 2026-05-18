/* SPDX-FileCopyrightText: 2025 Goo Engine Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 */

#include "DNA_light_types.h"
#include "DNA_object_types.h"

#include "BKE_context.hh"
#include "BKE_node.hh"
#include "BKE_object.hh"

#include "GPU_material.hh"

#include "BLI_math_vector.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"

#include "DEG_depsgraph_query.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_light_info_cc {

static void sh_node_light_info_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Color>("Light Color");
  b.add_output<decl::Float>("Light Power");
  b.add_output<decl::Float>("Perceptual Power");
}

static void node_shader_draw_light_info(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  /* Show light object selector */
  uiItemR(layout, ptr, "light_object", UI_ITEM_R_SPLIT_EMPTY_NAME, "Light", ICON_LIGHT);
}

/* This is the function called during GPU node graph building (before compilation). */
static int node_shader_gpu_light_info(GPUMaterial *mat,
                                      bNode *node,
                                      bNodeExecData * /*execdata*/,
                                      GPUNodeStack *in,
                                      GPUNodeStack *out)
{
  /* Capture light properties NOW (during graph building on main thread)
   * and store them as GPU uniforms. This avoids accessing the Light object
   * from the background compilation thread. */
  Object *ob = (Object *)node->id;
  
  /* Always use safe defaults. */
  static float default_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  static float default_power = 1.0f;
  static float default_perceptual_power = 1.0f;
  
  GPUNodeLink *color_link;
  GPUNodeLink *power_link;
  GPUNodeLink *perceptual_power_link;
  
  /* Try to access light properties if object is valid. */
  if (ob && ob->type == OB_LAMP && ob->data != nullptr) {
    Light *light = (Light *)ob->data;
    
    /* Capture the light properties as GPU uniforms (thread-safe). */
    float color[4] = {light->r, light->g, light->b, 1.0f};
    float power = light->energy;
    float perceptual_power = light->energy;
    
    /* Use GPU_uniform to capture these values safely for later compilation. */
    color_link = GPU_uniform(color);
    power_link = GPU_uniform(&power);
    perceptual_power_link = GPU_uniform(&perceptual_power);
  }
  else {
    /* Use default values */
    color_link = GPU_uniform(default_color);
    power_link = GPU_uniform(&default_power);
    perceptual_power_link = GPU_uniform(&default_perceptual_power);
  }
  
  /* Pass the captured uniforms to the GPU stack. */
  return GPU_stack_link(mat, node, "node_light_info_simple", in, out,
                        color_link,              /* Light Color */
                        power_link,              /* Light Power */
                        perceptual_power_link);  /* Perceptual Power */
}

}  // namespace blender::nodes::node_shader_light_info_cc

/* node type definition */
void register_node_type_sh_light_info(void)
{
  namespace file_ns = blender::nodes::node_shader_light_info_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_LIGHT_INFO, "Light Info", NODE_CLASS_INPUT);
  ntype.declare = file_ns::sh_node_light_info_declare;
  ntype.draw_buttons = file_ns::node_shader_draw_light_info;
  ntype.gpu_fn = file_ns::node_shader_gpu_light_info;

  blender::bke::node_register_type(&ntype);
}
