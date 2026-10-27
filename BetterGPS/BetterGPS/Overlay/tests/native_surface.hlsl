// SCS dif.lum.a: alpha test leaves the original physical screen untouched.
Texture2D image:register(t0);SamplerState pointSampler:register(s0);
float4 pixel(float4 position:SV_POSITION,float2 uv:TEXCOORD0):SV_TARGET {
 float4 color=image.Sample(pointSampler,uv);clip(color.a-0.05);return color;
}
