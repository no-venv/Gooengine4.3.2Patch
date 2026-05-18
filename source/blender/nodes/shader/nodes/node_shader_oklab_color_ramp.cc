/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 */

#include "DNA_texture_types.h"
#include "DNA_color_types.h"

#include "BKE_colorband.hh"

#include "BLI_color.hh"

#include "NOD_multi_function.hh"

#include "node_shader_util.hh"
#include "node_util.hh"

namespace blender::nodes::node_shader_oklab_color_ramp_cc {

/* OKLab color space conversion functions */

static float3 linear_srgb_to_oklab(const float3 &c)
{
  /* Convert Linear sRGB to LMS (cone response) */
  float l = 0.4122214708f * c.x + 0.5363325363f * c.y + 0.0514459929f * c.z;
  float m = 0.2119034982f * c.x + 0.6806995451f * c.y + 0.1073969566f * c.z;
  float s = 0.0883024619f * c.x + 0.2817188376f * c.y + 0.6299787005f * c.z;

  /* Apply cube root */
  float l_ = (l >= 0.0f) ? powf(l, 1.0f/3.0f) : -powf(-l, 1.0f/3.0f);
  float m_ = (m >= 0.0f) ? powf(m, 1.0f/3.0f) : -powf(-m, 1.0f/3.0f);
  float s_ = (s >= 0.0f) ? powf(s, 1.0f/3.0f) : -powf(-s, 1.0f/3.0f);

  /* Convert to OKLab */
  return float3(
      0.2104542553f * l_ + 0.7936177850f * m_ - 0.0040720468f * s_,
      1.9779984951f * l_ - 2.4285922050f * m_ + 0.4505937099f * s_,
      0.0259040371f * l_ + 0.7827717662f * m_ - 0.8086757660f * s_
  );
}

static float3 oklab_to_linear_srgb(const float3 &c)
{
  /* Convert OKLab back to LMS cone response */
  float l_ = c.x + 0.3963377774f * c.y + 0.2158037573f * c.z;
  float m_ = c.x - 0.1055613458f * c.y - 0.0638541728f * c.z;
  float s_ = c.x - 0.0894841775f * c.y - 1.2914855480f * c.z;

  /* Apply cube (power of 3) */
  float l = l_ * l_ * l_;
  float m = m_ * m_ * m_;
  float s = s_ * s_ * s_;

  /* Convert back to Linear sRGB */
  return float3(
      +4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s,
      -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s,
      -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s
  );
}

static float3 srgb_to_linear(const float3 &color)
{
  float3 a = color / 12.92f;
  float3 temp = (color + 0.055f) / 1.055f;
  float3 b = float3(math::pow(temp.x, 2.4f), math::pow(temp.y, 2.4f), math::pow(temp.z, 2.4f));
  float3 selector = float3(color.x > 0.04045f ? 1.0f : 0.0f, 
                          color.y > 0.04045f ? 1.0f : 0.0f, 
                          color.z > 0.04045f ? 1.0f : 0.0f);
  return a * (float3(1.0f) - selector) + b * selector;
}

static float3 linear_to_srgb(const float3 &color)
{
  float3 a = color * 12.92f;
  float3 powered = float3(math::pow(color.x, 1.0f/2.4f), math::pow(color.y, 1.0f/2.4f), math::pow(color.z, 1.0f/2.4f));
  float3 b = 1.055f * powered - 0.055f;
  float3 selector = float3(color.x > 0.0031308f ? 1.0f : 0.0f, 
                          color.y > 0.0031308f ? 1.0f : 0.0f, 
                          color.z > 0.0031308f ? 1.0f : 0.0f);
  return a * (float3(1.0f) - selector) + b * selector;
}

/* OKLab-based colorband evaluation */
static void oklab_colorband_evaluate(const ColorBand *coba, float in, ColorGeometry4f &out)
{
  /* Clamp input */
  in = math::clamp(in, 0.0f, 1.0f);
  
  /* Handle edge cases */
  if (coba->tot == 0) {
    out = ColorGeometry4f(0.0f, 0.0f, 0.0f, 0.0f);
    return;
  }
  
  if (coba->tot == 1) {
    const CBData &cbd = coba->data[0];
    out = ColorGeometry4f(cbd.r, cbd.g, cbd.b, cbd.a);
    return;
  }
  
  /* Find the appropriate color stops */
  int left_index = 0;
  int right_index = 0;
  
  for (int i = 0; i < coba->tot; i++) {
    if (coba->data[i].pos <= in) {
      left_index = i;
    } else {
      right_index = i;
      break;
    }
  }
  
  /* Handle boundary cases */
  if (left_index == right_index) {
    if (in <= coba->data[0].pos) {
      /* Before first stop */
      const CBData &cbd = coba->data[0];
      out = ColorGeometry4f(cbd.r, cbd.g, cbd.b, cbd.a);
    } else {
      /* After last stop */
      const CBData &cbd = coba->data[coba->tot - 1];
      out = ColorGeometry4f(cbd.r, cbd.g, cbd.b, cbd.a);
    }
    return;
  }
  
  /* Interpolate between the two stops using OKLab */
  const CBData &left = coba->data[left_index];
  const CBData &right = coba->data[right_index];
  
  /* Calculate interpolation factor */
  float factor = (in - left.pos) / (right.pos - left.pos);
  factor = math::clamp(factor, 0.0f, 1.0f);
  
  /* Convert colors to Linear sRGB first */
  float3 left_linear = srgb_to_linear(float3(left.r, left.g, left.b));
  float3 right_linear = srgb_to_linear(float3(right.r, right.g, right.b));
  
  /* Convert to OKLab */
  float3 left_oklab = linear_srgb_to_oklab(left_linear);
  float3 right_oklab = linear_srgb_to_oklab(right_linear);
  
  /* Interpolate in OKLab space */
  float3 mixed_oklab = math::interpolate(left_oklab, right_oklab, factor);
  
  /* Convert back to Linear sRGB */
  float3 mixed_linear = oklab_to_linear_srgb(mixed_oklab);
  
  /* Convert back to sRGB */
  float3 mixed_srgb = linear_to_srgb(mixed_linear);
  
  /* Mix alpha linearly */
  float mixed_alpha = math::interpolate(left.a, right.a, factor);
  
  /* Clamp and assign */
  out = ColorGeometry4f(
      math::clamp(mixed_srgb.x, 0.0f, 1.0f),
      math::clamp(mixed_srgb.y, 0.0f, 1.0f),
      math::clamp(mixed_srgb.z, 0.0f, 1.0f),
      math::clamp(mixed_alpha, 0.0f, 1.0f)
  );
}

static void sh_node_oklab_valtorgb_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Float>("Fac")
      .default_value(0.5f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description(
          "The value used to map onto the color gradient using OKLab color space for better perceptual interpolation");
  b.add_output<decl::Color>("Color");
  b.add_output<decl::Float>("Alpha");
}

static void node_shader_init_oklab_valtorgb(bNodeTree * /*ntree*/, bNode *node)
{
  node->storage = BKE_colorband_add(true);
}

static int gpu_shader_oklab_valtorgb(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData * /*execdata*/,
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  ColorBand *coba = (ColorBand *)node->storage;

  /* For OKLab, use direct GPU calculation for 1-2 stops (which work perfectly) */
  /* and texture approach for 3+ stops (with improved CPU OKLab evaluation) */
  
  if (coba->tot == 1) {
    /* Single color */
    return GPU_stack_link(mat,
                          node,
                          "oklab_valtorgb_opti_constant",
                          in,
                          out,
                          GPU_constant(&coba->data[0].pos),
                          GPU_uniform(&coba->data[0].r),
                          GPU_uniform(&coba->data[0].r));
  }
  
  if (coba->tot == 2) {
    /* Two stops - use direct OKLab interpolation with Ease (this works perfectly) */
    float mul_bias[2];
    mul_bias[0] = 1.0f / (coba->data[1].pos - coba->data[0].pos);
    mul_bias[1] = -mul_bias[0] * coba->data[0].pos;
    
    return GPU_stack_link(mat,
                          node,
                          "oklab_valtorgb_opti_ease",
                          in,
                          out,
                          GPU_uniform(mul_bias),
                          GPU_uniform(&coba->data[0].r),
                          GPU_uniform(&coba->data[1].r));
  }
  
  /* For 3+ stops, use texture approach with improved OKLab evaluation */
  float *array, layer;
  int size;
  
  /* Generate the texture array using our OKLab evaluation with ease interpolation */
  size = 257; /* Standard colorband texture size - must match compute_color_map_coordinate */
  array = (float *)MEM_mallocN(sizeof(float) * size * 4, "OKLab Colorband Array");
  
  /* Force ease interpolation mode for texture generation since you want only ease */
  int original_ipotype = coba->ipotype;
  const_cast<ColorBand*>(coba)->ipotype = COLBAND_INTERP_EASE;
  
  for (int i = 0; i < size; i++) {
    float pos = (float)i / (float)(size - 1);
    float color[4];
    
    /* Use our OKLab evaluation function with EASE interpolation */
    BKE_colorband_evaluate_oklab(coba, pos, color);
    
    array[i * 4 + 0] = color[0]; /* R */
    array[i * 4 + 1] = color[1]; /* G */
    array[i * 4 + 2] = color[2]; /* B */
    array[i * 4 + 3] = color[3]; /* A */
  }
  
  /* Restore original interpolation mode */
  const_cast<ColorBand*>(coba)->ipotype = original_ipotype;
  
  GPUNodeLink *tex = GPU_color_band(mat, size, array, &layer);

  /* Use simple linear texture sampling since ease and OKLab are baked into texture */
  return GPU_stack_link(mat, node, "oklab_valtorgb", in, out, tex, GPU_constant(&layer));
}

class OKLabColorBandFunction : public mf::MultiFunction {
 private:
  const ColorBand &color_band_;

 public:
  OKLabColorBandFunction(const ColorBand &color_band) : color_band_(color_band)
  {
    static const mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder{"OKLab Color Band", signature};
      builder.single_input<float>("Value");
      builder.single_output<ColorGeometry4f>("Color");
      builder.single_output<float>("Alpha");
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArray<float> &values = params.readonly_single_input<float>(0, "Value");
    MutableSpan<ColorGeometry4f> colors = params.uninitialized_single_output<ColorGeometry4f>(
        1, "Color");
    MutableSpan<float> alphas = params.uninitialized_single_output<float>(2, "Alpha");

    mask.foreach_index([&](const int64_t i) {
      ColorGeometry4f color;
      /* Use our custom OKLab evaluation instead of standard BKE_colorband_evaluate */
      oklab_colorband_evaluate(&color_band_, values[i], color);
      
      colors[i] = color;
      alphas[i] = color.a;
    });
  }
};

static void sh_node_oklab_valtorgb_build_multi_function(nodes::NodeMultiFunctionBuilder &builder)
{
  const bNode &bnode = builder.node();
  const ColorBand *color_band = (const ColorBand *)bnode.storage;
  builder.construct_and_set_matching_fn<OKLabColorBandFunction>(*color_band);
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  /* TODO: Implement MaterialX support for OKLab Color Ramp */
  NodeItem res = empty();
  return res;
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_oklab_color_ramp_cc

void register_node_type_sh_oklab_color_ramp()
{
  namespace file_ns = blender::nodes::node_shader_oklab_color_ramp_cc;

  static blender::bke::bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_OKLAB_COLOR_RAMP, "OKLab Color Ramp", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::sh_node_oklab_valtorgb_declare;
  ntype.initfunc = file_ns::node_shader_init_oklab_valtorgb;
  blender::bke::node_type_size_preset(&ntype, blender::bke::eNodeSizePreset::Large);
  node_type_storage(&ntype, "ColorBand", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::gpu_shader_oklab_valtorgb;
  ntype.build_multi_function = file_ns::sh_node_oklab_valtorgb_build_multi_function;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::node_register_type(&ntype);
}