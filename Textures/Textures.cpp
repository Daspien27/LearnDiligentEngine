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
#include "DiligentCore/Graphics/GraphicsTools/interface/MapHelper.hpp"

#include "DiligentTools/TextureLoader/interface/TextureUtilities.h"

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
            case GLFW_KEY_1:
                app->mode = transform_mode::textured_triangle;
                break;
            case GLFW_KEY_2:
                app->mode = transform_mode::textured_quad;
                break;
            case GLFW_KEY_3:
                app->mode = transform_mode::rainbow_textured_quad;
                break;
            case GLFW_KEY_4:
                app->mode = transform_mode::combined_textures;
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
        SCDesc.ColorBufferFormat = TEX_FORMAT_BGRA8_UNORM_SRGB;

        EngineVkCreateInfo engine_ci;

        auto vk_factory = Diligent::GetEngineFactoryVk();

        vk_factory->CreateDeviceAndContextsVk(engine_ci, &m_pDevice, &m_pImmediateContext);

        auto handle = glfwGetWin32Window(window);

        if (!m_pSwapChain && handle != nullptr)
        {
            Win32NativeWindow Window{ handle };
            vk_factory->CreateSwapChainVk(m_pDevice, m_pImmediateContext, SCDesc, Window, &m_pSwapChain);
        }

        m_pEngineFactory = vk_factory;
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

        {
            using enum transform_mode;
            switch (mode)
            {
            case textured_triangle:
            case textured_quad:
                m_pImmediateContext->SetPipelineState(m_pPSO);
                break;
            case rainbow_textured_quad:
                m_pImmediateContext->SetPipelineState(m_pRainbowPSO);
                break;
            case combined_textures:
                m_pImmediateContext->SetPipelineState(m_pCombinedPSO);
                break;
            }

            switch (mode)
            {
            case textured_triangle:
                m_pImmediateContext->CommitShaderResources(m_pSRB, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                break;
            case textured_quad:
                m_pImmediateContext->CommitShaderResources(m_pContainerSRB, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                break;
            case rainbow_textured_quad:
                m_pImmediateContext->CommitShaderResources(m_pRainbowSRB, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                break;
            case combined_textures:
                m_pImmediateContext->CommitShaderResources(m_pCombinedSRB, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                break;
            }

            switch (mode)
            {
            case textured_triangle:
            {
                Diligent::Uint64 offset = 0;
                std::array pBuffs = { m_TriangleVertexBuffer.RawPtr() };
                m_pImmediateContext->SetVertexBuffers(0, pBuffs.size(), pBuffs.data(), &offset, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION, Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);

                Diligent::DrawAttribs drawAttrs;
                drawAttrs.NumVertices = 3;

                drawAttrs.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;
                m_pImmediateContext->Draw(drawAttrs);
            }
                break;
            case textured_quad:
            case combined_textures:
            {
                Diligent::Uint64 offset = 0;
                std::array pBuffs = { m_QuadVertexBuffer.RawPtr() };
                m_pImmediateContext->SetVertexBuffers(0, pBuffs.size(), pBuffs.data(), &offset, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION, Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
                m_pImmediateContext->SetIndexBuffer(m_QuadIndexBuffer, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

                Diligent::DrawIndexedAttribs DrawAttrs;     // This is an indexed draw call
                DrawAttrs.IndexType = Diligent::VT_UINT32; // Index type
                DrawAttrs.NumIndices = 6;

                DrawAttrs.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;
                m_pImmediateContext->DrawIndexed(DrawAttrs);
            }
                break;
            case rainbow_textured_quad:
            {
                Diligent::Uint64 offset = 0;
                std::array pBuffs = { m_QuadRainbowVertexBuffer.RawPtr() };
                m_pImmediateContext->SetVertexBuffers(0, pBuffs.size(), pBuffs.data(), &offset, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION, Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
                m_pImmediateContext->SetIndexBuffer(m_QuadIndexBuffer, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

                Diligent::DrawIndexedAttribs DrawAttrs;     // This is an indexed draw call
                DrawAttrs.IndexType = Diligent::VT_UINT32; // Index type
                DrawAttrs.NumIndices = 6;

                DrawAttrs.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;
                m_pImmediateContext->DrawIndexed(DrawAttrs);
            }
            break;
            }

        }

        m_pImmediateContext->Flush();
        m_pSwapChain->Present();
    }

    void create_pipeline_state() {

        using namespace Diligent;
        GraphicsPipelineStateCreateInfo PSOCreateInfo;

        PSOCreateInfo.PSODesc.Name = "Texture PSO";
        PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
        PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
        PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = m_pSwapChain->GetDesc().ColorBufferFormat;
        PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;

        ShaderCreateInfo ShaderCI;
        ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

        RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
        m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

        RefCntAutoPtr<IShader> pVS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
            ShaderCI.EntryPoint = "main";
            ShaderCI.Desc.Name = "Texture vertex shader";
            ShaderCI.FilePath = "texture.vsh";
            m_pDevice->CreateShader(ShaderCI, &pVS);
        }

        // Create a pixel shader
        RefCntAutoPtr<IShader> pPS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
            ShaderCI.EntryPoint = "main";
            ShaderCI.Desc.Name = "Texture pixel shader";
            ShaderCI.FilePath = "texture.psh";
            m_pDevice->CreateShader(ShaderCI, &pPS);
        }

        std::array LayoutElems =
        {
            LayoutElement{0, 0, 3, VT_FLOAT32, False},
            LayoutElement{1, 0, 2, VT_FLOAT32, False}
        };

        PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems.data();
        PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = LayoutElems.size();

        PSOCreateInfo.pVS = pVS;
        PSOCreateInfo.pPS = pPS;

        // Define variable type that will be used by default
        PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

        std::array Vars =
        {
            ShaderResourceVariableDesc{SHADER_TYPE_PIXEL, "g_Texture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
        };
        // clang-format on
        PSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars.data();
        PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = Vars.size();

        SamplerDesc SamLinearClampDesc
        {
            FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
            TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP
        };

        std::array ImtblSamplers =
        {
            ImmutableSamplerDesc {SHADER_TYPE_PIXEL, "g_Texture_sampler", SamLinearClampDesc}
        };

        PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers = ImtblSamplers.data();
        PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = ImtblSamplers.size();

        m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPSO);

        m_pPSO->CreateShaderResourceBinding(&m_pSRB, true);
        m_pPSO->CreateShaderResourceBinding(&m_pContainerSRB, true);

        // RainbowPSO
        RefCntAutoPtr<IShader> pRainbowVS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
            ShaderCI.EntryPoint = "main";
            ShaderCI.Desc.Name = "Rainbow Texture vertex shader";
            ShaderCI.FilePath = "rainbow_texture.vsh";
            m_pDevice->CreateShader(ShaderCI, &pRainbowVS);
        }

        RefCntAutoPtr<IShader> pRainbowPS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
            ShaderCI.EntryPoint = "main";
            ShaderCI.Desc.Name = "Rainbow Texture pixel shader";
            ShaderCI.FilePath = "rainbow_texture.psh";
            m_pDevice->CreateShader(ShaderCI, &pRainbowPS);
        }

        std::array RainbowLayoutElems =
        {
            LayoutElement{0, 0, 3, VT_FLOAT32, False},
            LayoutElement{1, 0, 3, VT_FLOAT32, False},
            LayoutElement{2, 0, 2, VT_FLOAT32, False}
        };

        PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = RainbowLayoutElems.data();
        PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = RainbowLayoutElems.size();

        PSOCreateInfo.pVS = pRainbowVS;
        PSOCreateInfo.pPS = pRainbowPS;

        m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pRainbowPSO);
        m_pRainbowPSO->CreateShaderResourceBinding(&m_pRainbowSRB, true);
        //CombinedPSO

        RefCntAutoPtr<IShader> pCombinedPS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
            ShaderCI.EntryPoint = "main";
            ShaderCI.Desc.Name = "Combined Texture pixel shader";
            ShaderCI.FilePath = "combined_texture.psh";
            m_pDevice->CreateShader(ShaderCI, &pCombinedPS);
        }

        PSOCreateInfo.pVS = pVS;
        PSOCreateInfo.pPS = pCombinedPS;

        PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems.data();
        PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = LayoutElems.size();

        std::array CombinedVars =
        {
            ShaderResourceVariableDesc{SHADER_TYPE_PIXEL, "g_ContainerTexture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
            ShaderResourceVariableDesc{SHADER_TYPE_PIXEL, "g_FaceTexture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
        };

        PSOCreateInfo.PSODesc.ResourceLayout.Variables = CombinedVars.data();
        PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = CombinedVars.size();

        std::array CombinedImtblSamplers =
        {
            ImmutableSamplerDesc {SHADER_TYPE_PIXEL, "g_Texture_sampler", SamLinearClampDesc}
        };

        PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers = CombinedImtblSamplers.data();
        PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = CombinedImtblSamplers.size();

        m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pCombinedPSO);
        m_pCombinedPSO->CreateShaderResourceBinding(&m_pCombinedSRB, true);
    }

    void create_triangle_buffer() {
        std::array vertices = {
            -0.5f, -0.5f, 0.0f, 0.0f, 1.0f,
            0.5f, -0.5f, 0.0f, 1.0f, 1.0f,
            0.0f,  0.5f, 0.0f, 0.5f, 0.0f
        };

        using namespace Diligent;
        BufferDesc VertBuffDesc;
        VertBuffDesc.Name = "Triangle vertex buffer";
        VertBuffDesc.Usage = USAGE_IMMUTABLE;
        VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
        VertBuffDesc.Size = vertices.size() * sizeof(decltype(vertices)::value_type);
        BufferData VBData;
        VBData.pData = vertices.data();
        VBData.DataSize = vertices.size() * sizeof(decltype(vertices)::value_type);
        m_pDevice->CreateBuffer(VertBuffDesc, &VBData, &m_TriangleVertexBuffer);
    }

    void create_quad_vertex_buffer() {
        std::array vertices = {
            // positions          // texture coords
             0.5f,  0.5f, 0.0f,   1.0f, 0.0f,   // top right
             0.5f, -0.5f, 0.0f,   1.0f, 1.0f,   // bottom right
            -0.5f, -0.5f, 0.0f,   0.0f, 1.0f,   // bottom left
            -0.5f,  0.5f, 0.0f,   0.0f, 0.0f    // top left 
        };

        using namespace Diligent;
        BufferDesc VertBuffDesc;
        VertBuffDesc.Name = "Quad vertex buffer";
        VertBuffDesc.Usage = USAGE_IMMUTABLE;
        VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
        VertBuffDesc.Size = vertices.size() * sizeof(decltype(vertices)::value_type);
        BufferData VBData;
        VBData.pData = vertices.data();
        VBData.DataSize = vertices.size() * sizeof(decltype(vertices)::value_type);
        m_pDevice->CreateBuffer(VertBuffDesc, &VBData, &m_QuadVertexBuffer);

    }

    void create_quad_rainbow_vertex_buffer() {
        std::array rainbow_vertices = {
            // positions          // colors           // texture coords
             0.5f,  0.5f, 0.0f,   1.0f, 0.0f, 0.0f,   1.0f, 0.0f,   // top right
             0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f,   1.0f, 1.0f,   // bottom right
            -0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 1.0f,   // bottom left
            -0.5f,  0.5f, 0.0f,   1.0f, 1.0f, 0.0f,   0.0f, 0.0f    // top left 
        };

        using namespace Diligent;
        BufferDesc VertBuffDesc;
        VertBuffDesc.Name = "Quad Rainbow vertex buffer";
        VertBuffDesc.Usage = USAGE_IMMUTABLE;
        VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
        VertBuffDesc.Size = rainbow_vertices.size() * sizeof(decltype(rainbow_vertices)::value_type);
        BufferData VBData;
        VBData.pData = rainbow_vertices.data();
        VBData.DataSize = rainbow_vertices.size() * sizeof(decltype(rainbow_vertices)::value_type);
        m_pDevice->CreateBuffer(VertBuffDesc, &VBData, &m_QuadRainbowVertexBuffer);
    }

    void create_quad_index_buffer() {
        using namespace Diligent;
        std::array indices = {  // note that we start from 0!
            0u, 1u, 3u,   // first triangle
            1u, 2u, 3u    // second triangle
        };

        BufferDesc IndBuffDesc;
        IndBuffDesc.Name = "Quad index buffer";
        IndBuffDesc.Usage = USAGE_IMMUTABLE;
        IndBuffDesc.BindFlags = BIND_INDEX_BUFFER;
        IndBuffDesc.Size = indices.size() * sizeof(decltype(indices)::value_type);
        BufferData IBData;
        IBData.pData = indices.data();
        IBData.DataSize = indices.size() * sizeof(decltype(indices)::value_type);
        m_pDevice->CreateBuffer(IndBuffDesc, &IBData, &m_QuadIndexBuffer);
    }

    void create_quad_buffers() {
        create_quad_vertex_buffer();
        create_quad_rainbow_vertex_buffer();
        create_quad_index_buffer();
    }

    void load_wall_texture() {
        using namespace Diligent;

        TextureLoadInfo loadInfo;
        loadInfo.IsSRGB = true;
        RefCntAutoPtr<ITexture> Tex;
        CreateTextureFromFile(R"(..\assets\wall.jpg)", loadInfo, m_pDevice, &Tex);
        // Get shader resource view from the texture
        m_WallTextureSRV = Tex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

        // Set texture SRV in the SRB
        m_pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_WallTextureSRV);
    }

    void load_container_texture() {
        using namespace Diligent;

        TextureLoadInfo loadInfo;
        loadInfo.IsSRGB = true;
        RefCntAutoPtr<ITexture> Tex;
        CreateTextureFromFile(R"(..\assets\container.jpg)", loadInfo, m_pDevice, &Tex);
        // Get shader resource view from the texture
        m_ContainerTextureSRV = Tex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

        // Set texture SRV in the SRB
        m_pContainerSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_ContainerTextureSRV);
        m_pRainbowSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_ContainerTextureSRV);
        m_pCombinedSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_ContainerTexture")->Set(m_ContainerTextureSRV);
    }

    void load_awesome_face_texture() {
        using namespace Diligent;

        TextureLoadInfo loadInfo;
        loadInfo.IsSRGB = true;

        RefCntAutoPtr<ITexture> Tex;
        CreateTextureFromFile(R"(..\assets\awesomeface.png)", loadInfo, m_pDevice, &Tex);
        // Get shader resource view from the texture
        m_FaceTextureSRV = Tex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

        // Set texture SRV in the SRB
        m_pCombinedSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_FaceTexture")->Set(m_FaceTextureSRV);
    }

    void load_textures() {
        load_wall_texture();
        load_container_texture();
        load_awesome_face_texture();
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
        load_textures();

        while (!glfwWindowShouldClose(window)) {

            render();

            glfwPollEvents();
        }
    }

private:

    GLFWwindow* window;

    Diligent::RefCntAutoPtr<Diligent::IEngineFactory>         m_pEngineFactory;
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice>          m_pDevice;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext>         m_pImmediateContext;
    Diligent::RefCntAutoPtr<Diligent::ISwapChain>             m_pSwapChain;

    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         m_pPSO;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                m_TriangleVertexBuffer;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pSRB;
    Diligent::RefCntAutoPtr<Diligent::ITextureView>           m_WallTextureSRV;

    Diligent::RefCntAutoPtr<Diligent::ITextureView>           m_ContainerTextureSRV;
    Diligent::RefCntAutoPtr<Diligent::ITextureView>           m_FaceTextureSRV;

    Diligent::RefCntAutoPtr<Diligent::IBuffer>                m_QuadVertexBuffer;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                m_QuadIndexBuffer;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pContainerSRB;

    Diligent::RefCntAutoPtr<Diligent::IBuffer>                m_QuadRainbowVertexBuffer;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         m_pRainbowPSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pRainbowSRB;

    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         m_pCombinedPSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pCombinedSRB;
    
    enum class transform_mode {
        textured_triangle,
        textured_quad,
        rainbow_textured_quad,
        combined_textures
    };

    transform_mode mode = transform_mode::textured_triangle;
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