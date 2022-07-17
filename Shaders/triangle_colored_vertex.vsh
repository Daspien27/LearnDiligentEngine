struct VSInput
{
    float3 Pos   : ATTRIB0;
    float3 Color : ATTRIB1;
};

struct PSInput
{
    float4 Pos   : SV_POSITION;
    float4 Color : COLOR0;
};

void main(in  VSInput VSIn,
    out PSInput PSIn)
{
    PSIn.Pos = float4(VSIn.Pos, 1.0);
    PSIn.Color = float4(VSIn.Color.rgb, 1.0);
}