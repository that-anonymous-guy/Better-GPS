Texture2D image:register(t0);SamplerState pointSampler:register(s0);
cbuffer Parameters:register(b0){float4 settings;};
struct Out{float4 position:SV_POSITION;float2 uv:TEXCOORD0;};
Out vertex(uint id:SV_VertexID){Out o;o.uv=float2((id<<1)&2,id&2);o.position=float4(o.uv.x*2-1,1-o.uv.y*2,0,1);return o;}
float4 pixel(Out input):SV_TARGET{if(settings.z<-2.5)return float4(image.SampleLevel(pointSampler,float2(.5+(input.uv.x-.5)*settings.x,.5+(.5-input.uv.y)*settings.y),0).rgb,1);if(settings.z<-1.5)return image.Sample(pointSampler,float2(input.uv.x*settings.x,input.uv.y));if(settings.z<-0.5)return float4(image.SampleLevel(pointSampler,float2(input.uv.x,0.5+(0.5-input.uv.y)*settings.x),0).rgb,1);if(settings.z>0.5)return float4(0,0,0,settings.x);float4 color=image.Sample(pointSampler,input.uv);color.a*=1-settings.w;return settings.y>0.5?float4(color.rgb*(1-settings.x),1):color;}
