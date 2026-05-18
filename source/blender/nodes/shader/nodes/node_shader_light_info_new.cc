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

#include "GPU_material.hh"

#include "BLI_math_vector.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "node_shader_util.hh"
#include "../../intern/node_util.hh"

namespace blender::nodes::node_shader_light_info_cc {

static void sh_node_light_info_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_output<decl::Color>("Light Color");
  b.add_output<decl::Float>("Light Power");
  b.add_output<decl::Vector>("Light Position");
  b.add_output<decl::Vector>("Light Rotation");
  b.add_output<decl::Float>("Light Distance");
}

static void node_shader_init_light_info(bNodeTree * /*ntree*/, bNode *node)
{
  NodeShaderLightInfo *data = MEM_cnew<NodeShaderLightInfo>(__func__);
  
  /* Node starts with no light object selected */
  data->light_object = nullptr;
  
  node->storage = data;
}

static void node_shader_draw_light_info(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  /* Show light object selector */
  uiItemR(layout, ptr, "light_object", UI_ITEM_NONE, "Light", ICON_LIGHT);
}

static int node_shader_gpu_light_info(GPUMaterial *mat,
                                      bNode *node,
                                      bNodeExecData * /*execdata*/,
                                      GPUNodeStack *in,
                                      GPUNodeStack *out)
{
  NodeShaderLightInfo *data = (NodeShaderLightInfo *)node->storage;
  
  if (data->light_object != nullptr && data->light_object->type == OB_LAMP) {
    Object *light_ob = data->light_object;
    Light *light = (Light *)light_ob->data;
    
    /* Get light color */
    float light_color[4] = {light->r, light->g, light->b, 1.0f};
    
    /* Get light power/energy */
    float light_power = light->energy;
    
    /* Get light position (world space) - use location directly */
    float light_position[3];
    copy_v3_v3(light_position, light_ob->loc);
    
    /* For rotation, use object's rotation directly */
    float light_rotation[3];
    copy_v3_v3(light_rotation, light_ob->rot);
    
    /* For now, return simplified version without distance calculation */
    return GPU_stack_link(mat, node, "node_light_info_simple", in, out,
                          GPU_constant(light_color),      /* Light Color */
                          GPU_constant(&light_power),     /* Light Power */  
                          GPU_constant(light_position),   /* Light Position */
                          GPU_constant(light_rotation));  /* Light Rotation */
  }
  else {
    /* No light selected - output defaults */
    float default_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float default_power = 1.0f;
    float default_position[3] = {0.0f, 0.0f, 0.0f};
    float default_rotation[3] = {0.0f, 0.0f, 0.0f};
    
    return GPU_stack_link(mat, node, "node_light_info_default", in, out,
                          GPU_constant(default_color),
                          GPU_constant(&default_power),
                          GPU_constant(default_position),
                          GPU_constant(default_rotation));
  }
}

}  // namespace blender::nodes::node_shader_light_info_cc

/* node type definition */
void register_node_type_sh_light_info(void)
{
  namespace file_ns = blender::nodes::node_shader_light_info_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_LIGHT_INFO, "Light Info", NODE_CLASS_INPUT);
  ntype.declare = file_ns::sh_node_light_info_declare;
  node_type_storage(&ntype, "NodeShaderLightInfo", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_shader_gpu_light_info;
  ntype.initfunc = file_ns::node_shader_init_light_info;
  ntype.draw_buttons = file_ns::node_shader_draw_light_info;

  blender::bke::node_register_type(&ntype);
}
