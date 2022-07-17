struct VSInput
{
    float3 Pos   : ATTRIB0;
    float3 Color : ATTRIB1;
    float2 UV    : ATTRIB2;
};

struct PSInput
{
    float4 Pos   : SV_POSITION;
    float3 Color : COLOR0;
    float2 UV    : TEX_COORD;
};

void main(in  VSInput VSIn,
    out PSInput PSIn)
{
    PSIn.Pos = float4(VSIn.Pos, 1.0);
    PSIn.Color = VSIn.Color;
    PSIn.UV = VSIn.UV;
}

