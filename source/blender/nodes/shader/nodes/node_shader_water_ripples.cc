// Modified node_shader_water_ripples.cc
#include "node_util.hh"
#include "UI_interface.hh"
#include "UI_resources.hh"
#include "node_shader_util.hh"

namespace blender::nodes::node_shader_water_ripples_cc {

static void node_shader_init_water_ripples(bNodeTree * /*ntree*/, bNode *node)
{
  NodeWaterRipples *storage = MEM_cnew<NodeWaterRipples>("NodeWaterRipples");
  storage->mode = NODE_WATER_RIPPLES_DROPS;
  node->storage = storage;
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "mode", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

static void node_declare(NodeDeclarationBuilder &b)
{
    // Essential inputs
    b.add_input<decl::Vector>("Vector").default_value({0.0f, 0.0f, 0.0f});
    b.add_input<decl::Float>("Time").default_value(0.0f);
    
    // ADD MODE AS INPUT SOCKET - this will solve the caching issue
    b.add_input<decl::Float>("Mode").default_value(0.0f).min(0.0f).max(3.0f).available(false);
    
    // Main controls
    b.add_input<decl::Float>("Scale").default_value(1.0f).min(-10.0f).max(10.0f);
    b.add_input<decl::Float>("Intensity").default_value(1.0f).min(-10.0f).max(10.0f);
    b.add_input<decl::Float>("Speed").default_value(1.0f).min(-5.0f).max(5.0f);
    b.add_input<decl::Float>("Detail").default_value(0.5f).min(0.0f).max(1.0f);
    b.add_input<decl::Float>("Bias").default_value(0.6f).min(0.01f).max(0.99f);
    
    // Outputs
    b.add_output<decl::Vector>("Distorted Vector");
    b.add_output<decl::Float>("Mask");
}

static int gpu_shader_water_ripples(GPUMaterial *mat,
                                   bNode *node,
                                   bNodeExecData * /*execdata*/,
                                   GPUNodeStack *in,
                                   GPUNodeStack *out)
{
    // Don't pass mode as constant anymore - it's now an input socket
    return GPU_stack_link(mat, node, "node_water_ripples", in, out);
}

static void node_update(bNodeTree * /*tree*/, bNode *node)
{
    NodeWaterRipples *storage = (NodeWaterRipples *)node->storage;
    if (!storage) return;
    
    // Set the default value of the Mode socket based on the enum
    bNodeSocket *mode_socket = bke::node_find_socket(node, SOCK_IN, "Mode");
    if (mode_socket && mode_socket->default_value) {
        bNodeSocketValueFloat *mode_val = (bNodeSocketValueFloat *)mode_socket->default_value;
        mode_val->value = (float)storage->mode;
    }
}

} // namespace blender::nodes::node_shader_water_ripples_cc

void register_node_type_sh_water_ripples()
{
    namespace file_ns = blender::nodes::node_shader_water_ripples_cc;
    
    static blender::bke::bNodeType ntype;

    sh_node_type_base(&ntype, SH_NODE_WATER_RIPPLES, "Water Ripples", NODE_CLASS_TEXTURE);
    ntype.declare = file_ns::node_declare;
    ntype.gpu_fn = file_ns::gpu_shader_water_ripples;
    ntype.draw_buttons = file_ns::node_layout;
    ntype.initfunc = file_ns::node_shader_init_water_ripples;
    ntype.updatefunc = file_ns::node_update;
    
    node_type_storage(
        &ntype,
        "NodeWaterRipples",
        node_free_standard_storage,
        node_copy_standard_storage);

  blender::bke::node_register_type(&ntype);
}