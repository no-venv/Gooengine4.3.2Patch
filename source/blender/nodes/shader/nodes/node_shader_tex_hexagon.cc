/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"
#include "node_util.hh"

#include "BKE_texture.h"

#include "BLI_hash.hh"
#include "BLI_math_vector.hh"
#include "BLI_noise.hh"

#include "NOD_multi_function.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "node_shader_util.hh"

#include "NOD_math_functions.hh"

namespace blender::nodes::node_shader_tex_hexagon_cc {

NODE_STORAGE_FUNCS(NodeTexHexagon)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>("Vector").hide_value().implicit_field(implicit_field_inputs::position);
  b.add_input<decl::Float>("Scale").min(-1000.0f).max(1000.0f).default_value(5.0f);
  b.add_input<decl::Float>("Size").min(0.0f).max(16.0f).default_value(1.0f);
  b.add_input<decl::Float>("Radius").min(0.0f).max(1000.0f).default_value(0.0f);
  b.add_input<decl::Float>("Roundness").min(0.0f).max(1.0f).subtype(PROP_FACTOR);
  b.add_output<decl::Float>("Value").no_muted_links();
  b.add_output<decl::Color>("Color").no_muted_links();
  b.add_output<decl::Vector>("Hex Coords").no_muted_links();
  b.add_output<decl::Vector>("Position").no_muted_links();
  b.add_output<decl::Vector>("Cell UV").no_muted_links();
  b.add_output<decl::Vector>("Cell ID").no_muted_links();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "value_mode", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  uiItemR(col, ptr, "direction", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  uiItemR(col, ptr, "coord_mode", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "use_clamp", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

static void node_init(bNodeTree * /*ntree*/, bNode *node)
{
  NodeTexHexagon *tex = MEM_cnew<NodeTexHexagon>(__func__);
  BKE_texture_mapping_default(&tex->base.tex_mapping, TEXMAP_TYPE_POINT);
  BKE_texture_colormapping_default(&tex->base.color_mapping);
  node->storage = tex;
}

static int node_shader_gpu_tex_hexagon(GPUMaterial *mat,
                                       bNode *node,
                                       bNodeExecData * /*execdata*/,
                                       GPUNodeStack *in,
                                       GPUNodeStack *out)
{
  node_shader_gpu_default_tex_coord(mat, node, &in[0].link);
  node_shader_gpu_tex_mapping(mat, node, in, out);
  NodeTexHexagon *tex = (NodeTexHexagon *)node->storage;
  float coord_mode = tex->coord_mode;
  float value_mode = tex->value_mode;
  float direction = tex->direction;
  int ret = GPU_stack_link(mat,
                           node,
                           "node_tex_hexagon",
                           in,
                           out,
                           GPU_constant(&coord_mode),
                           GPU_constant(&value_mode),
                           GPU_constant(&direction));
  if (ret && tex->use_clamp) {
    float min[3] = {0.0f, 0.0f, 0.0f};
    float max[3] = {1.0f, 1.0f, 1.0f};
    GPU_link(mat, "clamp_value", out[0].link, GPU_constant(min), GPU_constant(max), &out[0].link);
  }
  return ret;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *sockRadius = bke::node_find_socket(node, SOCK_IN, "Radius");
  bNodeSocket *sockRoundness = bke::node_find_socket(node, SOCK_IN, "Roundness");

  NodeTexHexagon *tex = (NodeTexHexagon *)node->storage;
  bke::node_set_socket_availability(ntree, sockRadius, !ELEM(tex->value_mode, SHD_HEXAGON_VALUE_DOT));
  bke::node_set_socket_availability(
      ntree, sockRoundness, ELEM(tex->value_mode, SHD_HEXAGON_VALUE_SDF));
}

class HexagonFunction : public mf::MultiFunction {
 private:
  const int coord_mode_;
  const int direction_;
  const int use_clamp_;
  const int value_mode_;

 public:
  HexagonFunction(const int coord_mode,
                  const int direction,
                  const int use_clamp,
                  const int value_mode)
      : coord_mode_(coord_mode),
        direction_(direction),
        use_clamp_(use_clamp),
        value_mode_(value_mode)
  {
    static std::array<mf::Signature, 3> signatures{
        create_signature(SHD_HEXAGON_VALUE_HEX),
        create_signature(SHD_HEXAGON_VALUE_SDF),
        create_signature(SHD_HEXAGON_VALUE_DOT),
    };

    this->set_signature(&signatures[value_mode]);
  }

  static mf::Signature create_signature(int value_mode)
  {
    mf::Signature signature;
    mf::SignatureBuilder builder{"Hexagon", signature};
    builder.single_input<float3>("Vector");
    builder.single_input<float>("Scale");
    builder.single_input<float>("Size");

    switch (value_mode) {
      case SHD_HEXAGON_VALUE_SDF: {
        builder.single_input<float>("Radius");
        builder.single_input<float>("Roundness");
        break;
      }
      case SHD_HEXAGON_VALUE_HEX: {
        builder.single_input<float>("Radius");
        break;
      }
      case SHD_HEXAGON_VALUE_DOT: {
        /* Ignore. */
        break;
      }
    }

    builder.single_output<float>("Value");
    builder.single_output<ColorGeometry4f>("Color");
    builder.single_output<float3>("Hex Coords");
    builder.single_output<float3>("Position");
    builder.single_output<float3>("Cell UV");
    builder.single_output<float3>("Cell ID");
    return signature;
  }

  /* Hexagon */

#define HRATIO 1.1547005f
#define HSQRT3 1.7320508f
#define HSQRT2 1.4142136f

#define NODE_HEXAGON_DIRECTION_HORIZONTAL 0
#define NODE_HEXAGON_DIRECTION_VERTICAL 1
#define NODE_HEXAGON_DIRECTION_HORIZONTAL_TILED 2
#define NODE_HEXAGON_DIRECTION_VERTICAL_TILED 3

  /*
   * SDF Functions based on:
   * -
   * https://www.iquilezles.org/www/articles/distfunctions2d/distfunctions2d.htm
   */

  static float sdf_dimension(float w, float *r)
  {
    float roundness = *r;
    float sw = math::sign(w);
    w = math::abs(w);
    roundness = math::interpolate(0.0f, w, math::clamp(roundness, 0.0f, 1.0f));
    const float dimension = math::max(w - roundness, 0.0f);
    *r = roundness * 0.5f;
    return dimension * sw;
  }

  static float hex_value_sdf(const float3 pos, float r, float rd)
  {
    float2 p = float2(pos.x, pos.y);
    r = sdf_dimension(r, &rd);
    const float3 k = float3(HSQRT3 * -0.5f, 0.5f, HRATIO * 0.5f);
    p = math::abs(p);
    float2 kxy = float2(k.x, k.y);
    p = p - (2.0f * math::min(math::dot(kxy, p), 0.0f) * kxy);
    p = p - float2(math::clamp(p.x, -k.z * r, k.z * r), r);
    return math::length(p) * math::sign(p.y) - rd * 2.0f;
  }

  static float hex_value(const float3 hp, const float radius)
  {
    float3 fac = float3(math::abs(hp.x - hp.y), math::abs(hp.y - hp.z), math::abs(hp.z - hp.x));
    float f = math::max(fac.x, math::max(fac.y, fac.z));
    return (radius == 0.0f) ? f : math::interpolate(f, math::length(fac) / HSQRT2, radius);
  }

  static float3 xy_to_hex(const float3 xy, const float ratio)
  {
    float3 p = xy;
    p.x *= ratio;
    p.z = -0.5f * p.x - p.y;
    p.y = -0.5f * p.x + p.y;
    return p;
  }

  static float hexagon(float3 p,
                       const float scale,
                       const float size,
                       const float radius,
                       const float roundness,
                       const int coord_mode,
                       const int value_mode,
                       const int direction,
                       float3 *hex_coords,
                       float3 *grid_position,
                       float3 *cell_coords,
                       float3 *cell_id,
                       bool calc_value)
  {
    const float ratio = (direction == NODE_HEXAGON_DIRECTION_HORIZONTAL_TILED ||
                         direction == NODE_HEXAGON_DIRECTION_VERTICAL_TILED) ?
                            1.0f :
                            HRATIO;
    if (direction == NODE_HEXAGON_DIRECTION_VERTICAL ||
        direction == NODE_HEXAGON_DIRECTION_VERTICAL_TILED)
    {
      p = float3(p.y, p.x, p.y);
    }
    p = xy_to_hex(p * scale, ratio);
    *hex_coords = p;
    float3 ip = math::floor(p + 0.5f);
    const float s = ip.x + ip.y + ip.z;
    float3 abs_d = float3(0.0f);
    if (s != 0.0f) {
      abs_d = math::abs(ip - p);
      if (abs_d.x >= abs_d.y && abs_d.x >= abs_d.z) {
        ip.x -= s;
      }
      else if (abs_d.y >= abs_d.x && abs_d.y >= abs_d.z) {
        ip.y -= s;
      }
      else {
        ip.z -= s;
      }
    }
    float3 hp = p - ip;
    hp *= (size != 0.0f) ? 1.0f / size : 0.0f;
    const float3 xy_coords = float3(hp.x * HSQRT3, hp.y - hp.z, 0.0f);
    if (coord_mode == SHD_HEXAGON_COORDS_HEX) {
      *cell_coords = hp;
      *cell_id = ip;
    }
    else {
      *cell_coords = xy_coords;
      *cell_id = float3(
          ip.x / ratio, (ip.y - ip.z + (1.0f - safe_floored_modf(ip.x, 2.0f))) / 2.0f, 0.0f);
    }
    if (direction == NODE_HEXAGON_DIRECTION_VERTICAL ||
        direction == NODE_HEXAGON_DIRECTION_VERTICAL_TILED)
    {
      hp = float3(hp.y, hp.x, hp.z);
      *cell_coords = float3(cell_coords->y, cell_coords->x, cell_coords->z);
      *cell_id = float3(cell_id->y, cell_id->x, cell_id->z);
    }
    *grid_position = math::safe_divide(*cell_id, float3(scale));
    /* Calc value. */
    if (!calc_value) {
      return 0.0f;
    }

    if (value_mode == SHD_HEXAGON_VALUE_DOT) {
      return math::length(hp);
    }
    else if (value_mode == SHD_HEXAGON_VALUE_SDF) {
      return hex_value_sdf(xy_coords, radius, roundness);
    }
    else { /* SHD_HEXAGON_VALUE_HEX */
      return hex_value(hp, radius);
    }
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    auto get_float_input = [&](int param_index, StringRef name) -> const VArray<float> {
      return params.readonly_single_input<float>(param_index, name);
    };
    auto get_float3_input = [&](int param_index, StringRef name) -> const VArray<float3> {
      return params.readonly_single_input<float3>(param_index, name);
    };
    auto get_r_value = [&](int param_index) -> MutableSpan<float> {
      return params.uninitialized_single_output_if_required<float>(param_index, "Value");
    };
    auto get_r_color = [&](int param_index) -> MutableSpan<ColorGeometry4f> {
      return params.uninitialized_single_output_if_required<ColorGeometry4f>(param_index, "Color");
    };
    auto get_r_float3 = [&](int param_index, StringRef name) -> MutableSpan<float3> {
      return params.uninitialized_single_output<float3>(param_index, name);
    };

    int param = 0;
    const VArray<float3> &vector = get_float3_input(param++, "Vector");
    const VArray<float> &scale = get_float_input(param++, "Scale");
    const VArray<float> &size = get_float_input(param++, "Size");

    switch (value_mode_) {
      case SHD_HEXAGON_VALUE_SDF: {
        const VArray<float> &radius = get_float_input(param++, "Radius");
        const VArray<float> &roundness = get_float_input(param++, "Roundness");
        MutableSpan<float> r_value = get_r_value(param++);
        MutableSpan<ColorGeometry4f> r_color = get_r_color(param++);
        MutableSpan<float3> r_coords = get_r_float3(param++, "Hex Coords");
        MutableSpan<float3> r_position = get_r_float3(param++, "Position");
        MutableSpan<float3> r_cell_uv = get_r_float3(param++, "Cell UV");
        MutableSpan<float3> r_cell_id = get_r_float3(param++, "Cell ID");
        const bool calc_value = !r_value.is_empty();
        const bool calc_color = !r_color.is_empty();
        mask.foreach_index([&](const int64_t i) {
          float3 cell_id;
          const float value = hexagon(vector[i],
                                      scale[i],
                                      size[i],
                                      radius[i],
                                      roundness[i],
                                      coord_mode_,
                                      value_mode_,
                                      direction_,
                                      &r_coords[i],
                                      &r_position[i],
                                      &r_cell_uv[i],
                                      &cell_id,
                                      calc_value);
          r_cell_id[i] = cell_id;
          if (calc_value) {
            r_value[i] = use_clamp_ ? math::clamp(value, 0.0f, 1.0f) : value;
          }
          if (calc_color) {
            const float3 color = noise::hash_float_to_float3(cell_id);
            r_color[i] = ColorGeometry4f(color[0], color[1], color[2], 1.0f);
          }
        });
        break;
      }
      case SHD_HEXAGON_VALUE_HEX: {
        const VArray<float> &radius = get_float_input(param++, "Radius");
        MutableSpan<float> r_value = get_r_value(param++);
        MutableSpan<ColorGeometry4f> r_color = get_r_color(param++);
        MutableSpan<float3> r_coords = get_r_float3(param++, "Hex Coords");
        MutableSpan<float3> r_position = get_r_float3(param++, "Position");
        MutableSpan<float3> r_cell_uv = get_r_float3(param++, "Cell UV");
        MutableSpan<float3> r_cell_id = get_r_float3(param++, "Cell ID");
        const bool calc_value = !r_value.is_empty();
        const bool calc_color = !r_color.is_empty();
        mask.foreach_index([&](const int64_t i) {
          float3 cell_id;
          const float value = hexagon(vector[i],
                                      scale[i],
                                      size[i],
                                      radius[i],
                                      0.0f,
                                      coord_mode_,
                                      value_mode_,
                                      direction_,
                                      &r_coords[i],
                                      &r_position[i],
                                      &r_cell_uv[i],
                                      &cell_id,
                                      calc_value);
          r_cell_id[i] = cell_id;
          if (calc_value) {
            r_value[i] = use_clamp_ ? math::clamp(value, 0.0f, 1.0f) : value;
          }
          if (calc_color) {
            const float3 color = noise::hash_float_to_float3(cell_id);
            r_color[i] = ColorGeometry4f(color[0], color[1], color[2], 1.0f);
          }
        });
        break;
      }
      case SHD_HEXAGON_VALUE_DOT: {
        MutableSpan<float> r_value = get_r_value(param++);
        MutableSpan<ColorGeometry4f> r_color = get_r_color(param++);
        MutableSpan<float3> r_coords = get_r_float3(param++, "Hex Coords");
        MutableSpan<float3> r_position = get_r_float3(param++, "Position");
        MutableSpan<float3> r_cell_uv = get_r_float3(param++, "Cell UV");
        MutableSpan<float3> r_cell_id = get_r_float3(param++, "Cell ID");
        const bool calc_value = !r_value.is_empty();
        const bool calc_color = !r_color.is_empty();
        mask.foreach_index([&](const int64_t i) {
          float3 cell_id;
          const float value = hexagon(vector[i],
                                      scale[i],
                                      size[i],
                                      0.0f,
                                      0.0f,
                                      coord_mode_,
                                      value_mode_,
                                      direction_,
                                      &r_coords[i],
                                      &r_position[i],
                                      &r_cell_uv[i],
                                      &cell_id,
                                      calc_value);
          r_cell_id[i] = cell_id;
          if (calc_value) {
            r_value[i] = use_clamp_ ? math::clamp(value, 0.0f, 1.0f) : value;
          }
          if (calc_color) {
            const float3 color = noise::hash_float_to_float3(cell_id);
            r_color[i] = ColorGeometry4f(color[0], color[1], color[2], 1.0f);
          }
        });
        break;
      }
    }
  }
};

static void build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const bNode &node = builder.node();
  NodeTexHexagon *tex = (NodeTexHexagon *)node.storage;
  builder.construct_and_set_matching_fn<HexagonFunction>(
      tex->coord_mode, tex->direction, tex->use_clamp, tex->value_mode);
}

}  // namespace blender::nodes::node_shader_tex_hexagon_cc

void register_node_type_sh_tex_hexagon()
{
  namespace file_ns = blender::nodes::node_shader_tex_hexagon_cc;

  static blender::bke::bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_TEX_HEXAGON, "Hex Grid Texture", NODE_CLASS_TEXTURE);
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_layout;
  ntype.initfunc = file_ns::node_init;
  ntype.gpu_fn = file_ns::node_shader_gpu_tex_hexagon;
  ntype.updatefunc = file_ns::node_update;
  node_type_storage(
      &ntype, "NodeTexHexagon", node_free_standard_storage, node_copy_standard_storage);
  ntype.build_multi_function = file_ns::build_multi_function;

  blender::bke::node_register_type(&ntype);
}
