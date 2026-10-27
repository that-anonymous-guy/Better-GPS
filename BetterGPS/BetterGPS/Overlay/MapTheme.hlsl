// RETIRED: unreferenced legacy map postprocessing. NativeMapTheme.h is used instead.
cbuffer Palette:register(b0){float4 settings;float4 src[32];float4 light[32];float4 dark[32];};
Texture2D image:register(t0);SamplerState pointSampler:register(s0);
struct Out{float4 position:SV_POSITION;float2 uv:TEXCOORD0;};
Out vertex(uint id:SV_VertexID){Out o;o.uv=float2((id<<1)&2,id&2);o.position=float4(o.uv.x*2-1,1-o.uv.y*2,0,1);return o;}
float3 toSRGB(float3 c){return lerp(12.92*c,1.055*pow(max(c,0),1/2.4)-.055,step(.0031308,c));}
float3 toLinear(float3 c){return lerp(c/12.92,pow(max((c+.055)/1.055,0),2.4),step(.04045,c));}
float4 pixel(Out input):SV_TARGET{
 float4 c=image.Sample(pointSampler,input.uv);if(c.a<.0001)return c;
 bool encoded=settings.z>.5;float3 observed=encoded?toSRGB(c.rgb):c.rgb;
 float best=100;int index=-1;
 for(int i=0;i<(int)settings.y;++i){
  // Navigation colours originate in ATS map_data/navigation_current_color.
  // No route recolouring, mixture recovery or player-ring masking here.
  // This pass only switches the background map palette between day/night.
  if((i>=9&&i<=11)||(i>=20&&i<=25)||src[i].a==0)continue;
  float d=dot(observed-src[i].rgb,observed-src[i].rgb);
  if(d<best){best=d;index=i;}
 }
 if(index>=0&&best<.0015){float3 target=settings.x>.5?dark[index].rgb:light[index].rgb;c.rgb=encoded?toLinear(target):target;}
 return c;
}
