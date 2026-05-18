float sRGBtoLin(float colorChannel)
{
  if (colorChannel <= 0.04045) {
    return colorChannel / 12.92;
  }
  else {
    return pow((colorChannel + 0.055) / 1.055, 2.4);
  }
}

float YtoLstar(float Y)
{
  if (Y <= (216.0 / 24389.0)) {
    return Y * (24389.0 / 27.0);
  }
  else {
    return pow(Y, 1.0 / 3.0) * 116.0 - 16.0;
  }
}

void node_light_info_simple(vec4 light_color,
                            float light_power,
                            out vec4 out_light_color,
                            out float out_light_power,
                            out float out_perceptual_power)
{
  out_light_color = light_color;
  out_light_power = light_power;

  // Linearize sRGB channels
  float vR = sRGBtoLin(light_color.r);
  float vG = sRGBtoLin(light_color.g);
  float vB = sRGBtoLin(light_color.b);

  // Calculate luminance
  float Y = 0.2126 * vR + 0.7152 * vG + 0.0722 * vB;

  // Calculate perceptual lightness (L*)
  float Lstar = YtoLstar(Y);

  // Output perceptual lightness * light power
  out_perceptual_power = ((Lstar * light_power) / 255.0) * (2.489645); 
  // normalize, then use the computed white constant
}