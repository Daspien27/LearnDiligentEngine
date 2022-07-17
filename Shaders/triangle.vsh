cbuffer Constants
{
    float4 ourColor;
};

struct VSInput
{
    float3 Pos   : ATTRIB0;
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
    PSIn.Color = ourColor;
}