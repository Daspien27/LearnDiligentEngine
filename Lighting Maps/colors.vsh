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
    float2 UV       : ATTRIB2;
};

struct PSInput
{
    float4 Pos     : SV_POSITION;
    float3 FragPos : POSITION0;
    float3 Normal  : NORMAL0;
    float2 UV      : TEXTURE0;
};

void main(in  VSInput VSIn,
    out PSInput PSIn)
{
    PSIn.Normal = float3x3(inverse_transpose_model) * VSIn.Normal;
    PSIn.FragPos = float3(model * float4(VSIn.Pos, 1.0));
    PSIn.Pos = projection * view * float4(PSIn.FragPos, 1.0);
    PSIn.UV = VSIn.UV;
}