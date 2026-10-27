Texture2D image:register(t0);SamplerState linearSampler:register(s0);
cbuffer Parameters:register(b0){float4 geometry;};
struct Out{float4 position:SV_POSITION;float2 uv:TEXCOORD0;};
float4 pixel(Out input):SV_TARGET{
 float2 p=abs(input.uv*geometry.xy-geometry.xy*.5)-(geometry.xy*.5-geometry.z);
 float distance=length(max(p,0))+min(max(p.x,p.y),0)-geometry.z;
 float4 color=image.Sample(linearSampler,input.uv);color.a*=saturate(.5-distance);return color;
}
