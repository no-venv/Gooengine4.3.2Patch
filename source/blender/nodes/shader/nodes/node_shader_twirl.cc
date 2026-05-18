#include "node_shader_util.hh"

namespace blender::nodes::node_shader_twirl_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>("Vector").default_value({0.0f, 0.0f, 0.0f});
  b.add_input<decl::Vector>("Center").default_value({0.5f, 0.5f, 0.0f}).min(0.0f).max(1.0f);
  b.add_input<decl::Float>("Amount").default_value(0.0f).min(-100.0f).max(100.0f);
  b.add_output<decl::Vector>("Vector");
}

static int gpu_shader_twirl(GPUMaterial *mat,
                            bNode *node,
                            bNodeExecData * /*execdata*/,
                            GPUNodeStack *in,
                            GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "node_twirl", in, out);
}

} // namespace blender::nodes::node_shader_twirl_cc

void register_node_type_sh_twirl(void)
{
  namespace file_ns = blender::nodes::node_shader_twirl_cc;
  
  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_TWIRL, "Twirl", NODE_CLASS_OP_VECTOR);
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::gpu_shader_twirl;
  
  blender::bke::node_register_type(&ntype);
}