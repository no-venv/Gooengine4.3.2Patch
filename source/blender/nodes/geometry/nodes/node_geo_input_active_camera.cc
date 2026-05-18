/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DEG_depsgraph_query.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_active_camera_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Object>("Active Camera")
      .description("The camera used for rendering the scene");
  b.add_input<decl::Bool>("Ignore Scene Override")
      .default_value(true)
      .description(
          "Ignores the Goo Engine geometry nodes override scene setting and uses the original active camera");
}

static void node_exec(GeoNodeExecParams params)
{
  const Scene *scene = DEG_get_evaluated_scene(params.depsgraph());

  if (!(scene->gn_camera_override == nullptr) && !(params.get_input<bool>("Ignore Scene Override"))) {
    Object *camera = DEG_get_evaluated_object(params.depsgraph(), scene->gn_camera_override);
    params.set_output("Active Camera", camera);
  } else {
    Object *camera = DEG_get_evaluated_object(params.depsgraph(), scene->camera);
    params.set_output("Active Camera", camera);
  }
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_INPUT_ACTIVE_CAMERA, "Active Camera", NODE_CLASS_INPUT);
  ntype.geometry_node_execute = node_exec;
  ntype.declare = node_declare;
  blender::bke::node_register_type(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_active_camera_cc
