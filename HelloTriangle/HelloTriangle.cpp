#include "GLFW/glfw3.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3native.h"

#define PLATFORM_WIN32 1
#define VULKAN_SUPPORTED
#include "DiligentCore/Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h"
#include "DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "DiligentCore/Graphics/GraphicsEngine/interface/SwapChain.h"

#include "DiligentCore/Common/interface/RefCntAutoPtr.hpp"

#include "glm/glm.hpp"
#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <iostream>

class application {

    static void  framebuffer_size_callback(GLFWwindow* window, int width, int height) {
        auto app = reinterpret_cast<application*> (glfwGetWindowUserPointer(window));
        app->m_pSwapChain->Resize(width, height);
    }

    static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        auto app = reinterpret_cast<application*> (glfwGetWindowUserPointer(window));
        
        if (action == GLFW_PRESS) {
            switch (key) {
            case GLFW_KEY_ESCAPE:
                glfwSetWindowShouldClose(window, true);
                break;
            case GLFW_KEY_W:
                app->WireframeMode = !app->WireframeMode;
                break;
            case GLFW_KEY_Q:
                app->QuadMode = !app->QuadMode;
                break;
            }
        }
    }

    void initialize_glfw() {

        if (auto res = glfwInit(); res == GLFW_FALSE) {
            throw std::runtime_error("GLFW failed to initialize.");
        }
        if (!glfwVulkanSupported()) {
            throw std::runtime_error("GLFW failed to initialize Vulkan.");
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(800, 600, "LearnDiligent", nullptr, nullptr);

        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, &application::framebuffer_size_callback);
        glfwSetKeyCallback(window, &application::key_callback);
    }

    void initialize_diligent_engine() {
        using namespace Diligent;

        SwapChainDesc SCDesc;

        EngineVkCreateInfo engine_ci;

        auto vk_factory = Diligent::GetEngineFactoryVk();

        vk_factory->CreateDeviceAndContextsVk(engine_ci, &m_pDevice, &m_pImmediateContext);

        auto handle = glfwGetWin32Window(window);

        if (!m_pSwapChain && handle != nullptr)
        {
            Win32NativeWindow Window{ handle };
            vk_factory->CreateSwapChainVk(m_pDevice, m_pImmediateContext, SCDesc, Window, &m_pSwapChain);
        }
    }

    void init() {
        initialize_glfw();
        initialize_diligent_engine();
    }

    void render() {
        auto pRTV = m_pSwapChain->GetCurrentBackBufferRTV();

        m_pImmediateContext->SetRenderTargets(1, &pRTV, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        const glm::vec4 ClearColor = { 0.2f, 0.3f, 0.3f, 1.0f };
        m_pImmediateContext->ClearRenderTarget(pRTV, glm::value_ptr(ClearColor), Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        m_pImmediateContext->SetPipelineState(WireframeMode ? m_pWireframePSO : m_pPSO);

        if (!QuadMode)
        {
            Diligent::Uint32 offset = 0;
            std::array pBuffs = { m_TriangleVertexBuffer.RawPtr() };
            m_pImmediateContext->SetVertexBuffers(0, pBuffs.size(), pBuffs.data(), &offset, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION, Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);

            Diligent::DrawAttribs drawAttrs;
            drawAttrs.NumVertices = 3;
            
            drawAttrs.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;
            m_pImmediateContext->Draw(drawAttrs);
        }
        else
        {
            Diligent::Uint32 offset = 0;
            std::array pBuffs = { m_QuadVertexBuffer.RawPtr() };
            m_pImmediateContext->SetVertexBuffers(0, pBuffs.size(), pBuffs.data(), &offset, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION, Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
            m_pImmediateContext->SetIndexBuffer(m_QuadIndexBuffer, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            Diligent::DrawIndexedAttribs DrawAttrs;     // This is an indexed draw call
            DrawAttrs.IndexType = Diligent::VT_UINT32; // Index type
            DrawAttrs.NumIndices = 6;
            
            DrawAttrs.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;
            m_pImmediateContext->DrawIndexed(DrawAttrs);
        }

        m_pImmediateContext->Flush();
        m_pSwapChain->Present();
    }

    void create_pipeline_state() {

        using namespace Diligent;
        GraphicsPipelineStateCreateInfo PSOCreateInfo;

        PSOCreateInfo.PSODesc.Name = "PSO";
        PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
        PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
        PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = m_pSwapChain->GetDesc().ColorBufferFormat;
        PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;

        const char* VSSource = R"(
struct VSInput 
{ 
    float3 Pos   : ATTRIB0;
};

struct PSInput 
{ 
    float4 Pos   : SV_POSITION; 
};

void main(in  VSInput VSIn,
          out PSInput PSIn) 
{
    PSIn.Pos   = float4(VSIn.Pos, 1.0);
}
)";
        ShaderCreateInfo ShaderCI;
        ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        RefCntAutoPtr<IShader> pVS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
            ShaderCI.EntryPoint = "main";
            ShaderCI.Desc.Name = "Triangle vertex shader";
            ShaderCI.Source = VSSource;
            m_pDevice->CreateShader(ShaderCI, &pVS);
        }

        const char* PSSource = R"(
struct PSInput 
{ 
    float4 Pos   : SV_POSITION; 
};

struct PSOutput
{ 
    float4 Color : SV_TARGET; 
};

void main(in  PSInput  PSIn,
          out PSOutput PSOut)
{
    PSOut.Color = float4(1.0f, 0.5f, 0.2f, 1.0);
}
)";

        // Create a pixel shader
        RefCntAutoPtr<IShader> pPS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
            ShaderCI.EntryPoint = "main";
            ShaderCI.Desc.Name = "Triangle pixel shader";
            ShaderCI.Source = PSSource;
            m_pDevice->CreateShader(ShaderCI, &pPS);
        }

        std::array LayoutElems =
        {
            // Attribute 0 - vertex position
            LayoutElement{0, 0, 3, VT_FLOAT32, False}
        };

        PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems.data();
        PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = LayoutElems.size();

        PSOCreateInfo.pVS = pVS;
        PSOCreateInfo.pPS = pPS;

        // Define variable type that will be used by default
        PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
        
        m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPSO);

        PSOCreateInfo.PSODesc.Name = "Wireframe PSO";
        PSOCreateInfo.GraphicsPipeline.RasterizerDesc.FillMode = FILL_MODE_WIREFRAME;
        m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pWireframePSO);
    }

    void create_triangle_buffer() {
        std::array vertices = {
            -0.5f, -0.5f, 0.0f,
            0.5f, -0.5f, 0.0f, 
            0.0f,  0.5f, 0.0f  
        };

        using namespace Diligent;
        BufferDesc VertBuffDesc;
        VertBuffDesc.Name = "Triangle vertex buffer";
        VertBuffDesc.Usage = USAGE_IMMUTABLE;
        VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
        VertBuffDesc.uiSizeInBytes = vertices.size() * sizeof(decltype(vertices)::value_type);
        BufferData VBData;
        VBData.pData = vertices.data();
        VBData.DataSize = vertices.size() * sizeof(decltype(vertices)::value_type);

        m_pDevice->CreateBuffer(VertBuffDesc, &VBData, &m_TriangleVertexBuffer);
    }

    void create_quad_buffers() {
        std::array vertices = {
             0.5f,  0.5f, 0.0f,  // top right
             0.5f, -0.5f, 0.0f,  // bottom right
            -0.5f, -0.5f, 0.0f,  // bottom left
            -0.5f,  0.5f, 0.0f   // top left 
        };

        using namespace Diligent;
        BufferDesc VertBuffDesc;
        VertBuffDesc.Name = "Quad vertex buffer";
        VertBuffDesc.Usage = USAGE_IMMUTABLE;
        VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
        VertBuffDesc.uiSizeInBytes = vertices.size() * sizeof(decltype(vertices)::value_type);
        BufferData VBData;
        VBData.pData = vertices.data();
        VBData.DataSize = vertices.size() * sizeof(decltype(vertices)::value_type);
        m_pDevice->CreateBuffer(VertBuffDesc, &VBData, &m_QuadVertexBuffer);


        std::array indices = {  // note that we start from 0!
            0u, 1u, 3u,   // first triangle
            1u, 2u, 3u    // second triangle
        };

        BufferDesc IndBuffDesc;
        IndBuffDesc.Name = "Quad index buffer";
        IndBuffDesc.Usage = USAGE_IMMUTABLE;
        IndBuffDesc.BindFlags = BIND_INDEX_BUFFER;
        IndBuffDesc.uiSizeInBytes = indices.size() * sizeof(decltype(indices)::value_type);
        BufferData IBData;
        IBData.pData = indices.data();
        IBData.DataSize = indices.size() * sizeof(decltype(indices)::value_type);
        m_pDevice->CreateBuffer(IndBuffDesc, &IBData, &m_QuadIndexBuffer);
    }

public:

    application() {
        init();
    }

    ~application() {
        glfwTerminate();
    }

    void run() {
        
        create_pipeline_state();
        create_triangle_buffer();
        create_quad_buffers();

        while (!glfwWindowShouldClose(window)) {

            render();

            glfwPollEvents();
        }
    }

private:

    GLFWwindow* window;
    bool QuadMode = false;
    bool WireframeMode = false;

    Diligent::RefCntAutoPtr<Diligent::IRenderDevice>  m_pDevice;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> m_pImmediateContext;
    Diligent::RefCntAutoPtr<Diligent::ISwapChain>     m_pSwapChain;
    
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> m_pPSO;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> m_pWireframePSO;

    Diligent::RefCntAutoPtr<Diligent::IBuffer>        m_TriangleVertexBuffer;

    Diligent::RefCntAutoPtr<Diligent::IBuffer>        m_QuadVertexBuffer;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>        m_QuadIndexBuffer;
};

int main()
{
    try {
        application app;

        app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return -1;
    }
}