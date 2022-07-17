cbuffer Constants
{
    float4x4 model;
    float4x4 view;
    float4x4 projection;
};

struct VSInput
{
    float3 Pos   : ATTRIB0;
    float2 UV    : ATTRIB1;
};

struct PSInput
{
    float4 Pos   : SV_POSITION;
    float2 UV    : TEX_COORD;
};

void main(in  VSInput VSIn,
    out PSInput PSIn)
{
    PSIn.Pos = projection * view * model * float4(VSIn.Pos, 1.0);
    PSIn.UV = VSIn.UV;
}