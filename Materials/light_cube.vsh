cbuffer Constants
{
    float4x4 model;
    float4x4 view;
    float4x4 projection;
    float4x4 inverse_transpose_model;
};

struct VSInput
{
    float3 Pos      : ATTRIB0;
    float3 Normal   : ATTRIB1;
};

struct PSInput
{
    float4 Pos     : SV_POSITION;
};

void main(in  VSInput VSIn,
    out PSInput PSIn)
{
    PSIn.Pos = projection * view * model * float4(VSIn.Pos, 1.0);
}