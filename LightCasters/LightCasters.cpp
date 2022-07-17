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
#include <optional>
#include <algorithm>
#include <variant>

struct Constants
{
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 projection = glm::mat4(1.0f);
    glm::mat4 inverse_transpose_model;
};

struct Material {
    float shininess;
};

struct Camera
{
    struct CB {
        alignas(16) glm::vec3 view_position;
    };

    glm::vec3 eye       = glm::vec3(0.0f, 0.0f, 3.0f);
    glm::vec3 front     = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up        = glm::vec3(0.0f, 1.0f, 0.0f);

    float yaw = -90.0f;
    float pitch = 0.0f;

    std::optional<float> last_x;
    std::optional<float> last_y;

    double fov = 45.0;

    CB create_buffer() const {
        return CB{ .view_position = eye };
    }
};


template <typename Data>
struct Resource {
    Diligent::RefCntAutoPtr<Diligent::IBuffer> buffer;
    Data data;

    void CreateBuffer(Diligent::IRenderDevice& Device, Diligent::BufferDesc PartialDesc, const char* Name) {
        PartialDesc.Name = Name;
        PartialDesc.Size = sizeof(Data);
        Device.CreateBuffer(PartialDesc, nullptr, &buffer);
    }

    void Map(Diligent::IDeviceContext* ImmediateContext) {
        Diligent::MapHelper<Data> MappedBuffer (ImmediateContext, buffer, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);

        *MappedBuffer = data;
    }
};

struct DirectionalLight {
    alignas(16) glm::vec3 direction;

    alignas(16) glm::vec3 ambient;
    alignas(16) glm::vec3 diffuse;
    alignas(16) glm::vec3 specular;
};

struct PointLight {
    alignas(16) glm::vec3 position;

    alignas(16) glm::vec3 ambient;
    alignas(16) glm::vec3 diffuse;
    alignas(16) glm::vec3 specular;

    float constant;
    float linear;
    float quadratic;
};

struct SpotLight {
    alignas(16) glm::vec3 position;
    float outerCutOff;
    alignas(16) glm::vec3 direction;
    float cutOff;

    alignas(16) glm::vec3 ambient;
    alignas(16) glm::vec3 diffuse;
    alignas(16) glm::vec3 specular;
};

struct light_setting_visitor {
    std::variant<DirectionalLight, PointLight, SpotLight>& light_variant;
    int key = GLFW_KEY_UNKNOWN;

    light_setting_visitor(std::variant<DirectionalLight, PointLight, SpotLight>& light_variant_use, int key_use) : light_variant(light_variant_use) {

    }

    void operator()(const DirectionalLight&) const {
        switch (key) {
        case GLFW_KEY_2:
            light_variant = PointLight{};
            break;
        case GLFW_KEY_3:
            light_variant = SpotLight{};
            break;
        }
    }

};

class application {
    static void  framebuffer_size_callback(GLFWwindow* window, int width, int height) {
        auto app = reinterpret_cast<application*> (glfwGetWindowUserPointer(window));

        if (app->m_pSwapChain)
        {
            app->m_pSwapChain->Resize(width, height);
        }
    }

    static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        auto app = reinterpret_cast<application*> (glfwGetWindowUserPointer(window));

        if (action == GLFW_PRESS) {
            switch (key) {
            case GLFW_KEY_ESCAPE:
                glfwSetWindowShouldClose(window, true);
                break;
            case GLFW_KEY_1:
                app->PSO_use = app->m_pDirectionalLightPSO;
                app->SRB_use = app->m_pDirectionalLightSRB;
                app->light_use = std::ref(std::get<Resource<DirectionalLight>>(app->lights));
                break;
            case GLFW_KEY_2:
                app->PSO_use = app->m_pPointLightPSO;
                app->SRB_use = app->m_pPointLightSRB;
                app->light_use = std::ref(std::get<Resource<PointLight>>(app->lights));
                break;
            case GLFW_KEY_3:
                app->PSO_use = app->m_pSpotLightPSO;
                app->SRB_use = app->m_pSpotLightSRB;
                app->light_use = std::ref(std::get<Resource<SpotLight>>(app->lights));
                break;
            }
        }
    }

    static void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
        auto app = reinterpret_cast<application*> (glfwGetWindowUserPointer(window));

        float xoffset = xpos - app->camera.last_x.value_or(xpos);
        float yoffset = app->camera.last_y.value_or(ypos) - ypos; // reversed since y-coordinates range from bottom to top
        app->camera.last_x = xpos;
        app->camera.last_y = ypos;

        const float sensitivity = 0.1f;
        xoffset *= sensitivity;
        yoffset *= sensitivity;

        app->camera.yaw += xoffset;
        app->camera.pitch = std::clamp(app->camera.pitch + yoffset, -89.0f, 89.0f);

        glm::vec3 direction = glm::vec3{
            std::cos(glm::radians(app->camera.yaw)) * std::cos(glm::radians(app->camera.pitch)),
            std::sin(glm::radians(app->camera.pitch)),
            std::sin(glm::radians(app->camera.yaw))* std::cos(glm::radians(app->camera.pitch))
        };

        app->camera.front = glm::normalize(direction);
    }

    static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
    {
        auto app = reinterpret_cast<application*> (glfwGetWindowUserPointer(window));

        app->camera.fov = std::clamp(app->camera.fov - yoffset, 1.0, 45.0);
    }

    void initialize_glfw() {
        constexpr auto width = 800;
        constexpr auto height = 600;

        if (auto res = glfwInit(); res == GLFW_FALSE) {
            throw std::runtime_error("GLFW failed to initialize.");
        }
        if (!glfwVulkanSupported()) {
            throw std::runtime_error("GLFW failed to initialize Vulkan.");
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(width, height, "LearnDiligent", nullptr, nullptr);

        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, &application::framebuffer_size_callback);
        glfwSetKeyCallback(window, &application::key_callback);

        glfwSetInputMode(window, GLFW_CURSOR, show_cursor ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
        glfwSetCursorPosCallback(window, &application::mouse_callback);
        glfwSetScrollCallback(window, &application::scroll_callback);

        framebuffer_size_callback(window, width, height);
    }

    void initialize_diligent_engine() {
        using namespace Diligent;

        EngineVkCreateInfo engine_ci;

        auto vk_factory = Diligent::GetEngineFactoryVk();

        vk_factory->CreateDeviceAndContextsVk(engine_ci, &m_pDevice, &m_pImmediateContext);

        auto handle = glfwGetWin32Window(window);

        if (!m_pSwapChain && handle != nullptr)
        {
            Win32NativeWindow Window{ handle };

            SwapChainDesc SCDesc;
            SCDesc.ColorBufferFormat = Diligent::TEXTURE_FORMAT::TEX_FORMAT_BGRA8_UNORM;
            SCDesc.DepthBufferFormat = TEX_FORMAT_D32_FLOAT;
            vk_factory->CreateSwapChainVk(m_pDevice, m_pImmediateContext, SCDesc, Window, &m_pSwapChain);
        }

        m_pEngineFactory = vk_factory;
    }

    void init() {
        initialize_glfw();
        initialize_diligent_engine();
    }

    void process_input(float delta)
    {
        const float camera_speed = 2.5f * delta;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            camera.eye += camera_speed * camera.front;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            camera.eye -= camera_speed * camera.front;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            camera.eye -= glm::normalize(glm::cross(camera.front, camera.up)) * camera_speed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            camera.eye += glm::normalize(glm::cross(camera.front, camera.up)) * camera_speed;


        auto& spot_light = std::get<Resource<SpotLight>>(lights);
        spot_light.data.position = camera.eye;
        spot_light.data.direction = camera.front;
    }

    void render() {
        auto pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
        auto pDSV = m_pSwapChain->GetDepthBufferDSV();
        m_pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        const glm::vec4 ClearColor = { 0.1f, 0.1f, 0.1f, 1.0f };
        m_pImmediateContext->ClearRenderTarget(pRTV, glm::value_ptr(ClearColor), Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearDepthStencil(pDSV, Diligent::CLEAR_DEPTH_FLAG, 1.f, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        {
            m_pImmediateContext->SetPipelineState(PSO_use);

            Diligent::Uint64 offset = 0;
            std::array pBuffs = { m_CubeVertexBuffer.RawPtr() };
            m_pImmediateContext->SetVertexBuffers(0, pBuffs.size(), pBuffs.data(), &offset, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION, Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);

            glm::mat4 light_model(1.0f);

            const glm::vec4 light_pos = (light_model * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
            {
                std::visit([this](auto& LightResource) {

                    LightResource.get().Map(m_pImmediateContext);

                }, light_use);
            }

            if (auto point_light_use = std::get_if<std::reference_wrapper<Resource<PointLight>>>(&light_use)) {
                light_model = glm::translate(light_model, point_light_use->get().data.position);
            }
            else if (auto directional_light_use = std::get_if<std::reference_wrapper<Resource<DirectionalLight>>>(&light_use)) {
                light_model = glm::translate(light_model, directional_light_use->get().data.direction);
            }


            {
                Diligent::MapHelper<Camera::CB> CBCamera(m_pImmediateContext, m_PSCamera, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);

                *CBCamera = camera.create_buffer();
            }

            Constants c;
            {
                const auto& SwapChainDesc = m_pSwapChain->GetDesc();
                const float aspect = static_cast<float> (SwapChainDesc.Width) / static_cast<float> (SwapChainDesc.Height);
                c.view = glm::transpose(glm::lookAt(camera.eye, camera.eye + camera.front, camera.up));
                c.projection = glm::transpose(glm::perspective(glm::radians(static_cast<float> (camera.fov)), aspect, 0.1f, 100.0f));
            }

            Diligent::DrawAttribs DrawAttrs;
            DrawAttrs.NumVertices = 36;
            DrawAttrs.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;

            const auto render_cube = [&](const glm::mat4& model) {
                {
                    c.model = glm::transpose(model);
                    c.inverse_transpose_model = glm::transpose(glm::transpose(glm::inverse(model)));

                    Diligent::MapHelper<Constants> CBConstants(m_pImmediateContext, m_VSConstants, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
                    *CBConstants = c;
                }
                {
                    Diligent::MapHelper<Material> CBMaterial(m_pImmediateContext, m_PSMaterial, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
                    CBMaterial->shininess = 64.0f;
                }

                m_pImmediateContext->CommitShaderResources(SRB_use, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                m_pImmediateContext->Draw(DrawAttrs);
            };

            {
                std::array cube_positions = {
                    glm::vec3(0.0f,  0.0f,  0.0f),
                    glm::vec3(2.0f,  5.0f, -15.0f),
                    glm::vec3(-1.5f, -2.2f, -2.5f),
                    glm::vec3(-3.8f, -2.0f, -12.3f),
                    glm::vec3(2.4f, -0.4f, -3.5f),
                    glm::vec3(-1.7f,  3.0f, -7.5f),
                    glm::vec3(1.3f, -2.0f, -2.5f),
                    glm::vec3(1.5f,  2.0f, -2.5f),
                    glm::vec3(1.5f,  0.2f, -1.5f),
                    glm::vec3(-1.3f,  1.0f, -1.5f)
                };

                int i = 0;

                Diligent::DrawAttribs DrawAttrs;
                DrawAttrs.NumVertices = 36;
                DrawAttrs.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;

                for (const auto& cube_position : cube_positions)
                {
                    glm::mat4 model = glm::translate(glm::mat4(1.0f), cube_position);
                    float angle = 20.0f * i++;
                    model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
                    render_cube(model);
                }
            }

            m_pImmediateContext->SetPipelineState(m_pLightCubePSO);
            c.model = glm::transpose(glm::scale(light_model, glm::vec3(0.2f)));
            c.inverse_transpose_model = glm::transpose(glm::inverse(glm::transpose(c.model)));
            Diligent::MapHelper<Constants> CBConstants(m_pImmediateContext, m_VSConstants, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
            *CBConstants = c;

            m_pImmediateContext->CommitShaderResources(m_pLightCubeSRB, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            m_pImmediateContext->Draw(DrawAttrs);
        }

        m_pImmediateContext->Flush();
        m_pSwapChain->Present();
    }

    void create_pipeline_states() {
        using namespace Diligent;
        GraphicsPipelineStateCreateInfo PSOCreateInfo;

        PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
        PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
        PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = m_pSwapChain->GetDesc().ColorBufferFormat;
        PSOCreateInfo.GraphicsPipeline.DSVFormat = m_pSwapChain->GetDesc().DepthBufferFormat;
        PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;
        PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;

        ShaderCreateInfo ShaderCI;
        ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

        RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
        m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

        std::array LayoutElems =
        {
            LayoutElement{0, 0, 3, VT_FLOAT32, False},
            LayoutElement{1, 0, 3, VT_FLOAT32, False},
            LayoutElement{2, 0, 2, VT_FLOAT32, False}
        };

        PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems.data();
        PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = LayoutElems.size();

        PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

        RefCntAutoPtr<IShader> pLightCubeVS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
            ShaderCI.Desc.Name = "Light Cube vertex shader";
            ShaderCI.FilePath = "light_cube.vsh";
            m_pDevice->CreateShader(ShaderCI, &pLightCubeVS);
        }

        RefCntAutoPtr<IShader> pLightCubePS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
            ShaderCI.Desc.Name = "Light Cube pixel shader";
            ShaderCI.FilePath = "light_cube.psh";
            m_pDevice->CreateShader(ShaderCI, &pLightCubePS);
        }

        PSOCreateInfo.PSODesc.Name = "Light Cube PSO";
        PSOCreateInfo.pVS = pLightCubeVS;
        PSOCreateInfo.pPS = pLightCubePS;
        m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pLightCubePSO);

        std::array CombinedVars =
        {
            ShaderResourceVariableDesc{SHADER_TYPE_PIXEL, "diffuse_texture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
          , ShaderResourceVariableDesc{SHADER_TYPE_PIXEL, "specular_texture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
        };

        PSOCreateInfo.PSODesc.ResourceLayout.Variables = CombinedVars.data();
        PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = CombinedVars.size();

        SamplerDesc SamLinearClampDesc
        {
            FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
            TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP
        };

        std::array CombinedImtblSamplers =
        {
            ImmutableSamplerDesc {SHADER_TYPE_PIXEL, "diffuse_sampler", SamLinearClampDesc}
        };

        PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers = CombinedImtblSamplers.data();
        PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = CombinedImtblSamplers.size();

        RefCntAutoPtr<IShader> pVS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
            ShaderCI.EntryPoint = "main";
            ShaderCI.Desc.Name = "Colors vertex shader";
            ShaderCI.FilePath = "colors.vsh";
            m_pDevice->CreateShader(ShaderCI, &pVS);
        }

        PSOCreateInfo.pVS = pVS;

        create_uniform_buffers();

        const auto BindResources = [this](Diligent::IPipelineState* PSO, Diligent::IShaderResourceBinding** SRB, Diligent::IDeviceObject* LightBuffer) {
            PSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);
            PSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "Lights")->Set(LightBuffer);
            PSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "Materials")->Set(m_PSMaterial);
            PSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "Camera")->Set(m_PSCamera);
            PSO->CreateShaderResourceBinding(SRB, true);
        };


        {
            RefCntAutoPtr<IShader> pDirectionalLightPS;
            {
                ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
                ShaderCI.EntryPoint = "main";
                ShaderCI.Desc.Name = "Directional light pixel shader";
                ShaderCI.FilePath = "directional_light.psh";
                m_pDevice->CreateShader(ShaderCI, &pDirectionalLightPS);
            }

            PSOCreateInfo.pPS = pDirectionalLightPS;
            PSOCreateInfo.PSODesc.Name = "Directional Light PSO";
            m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pDirectionalLightPSO);
            BindResources(m_pDirectionalLightPSO, &m_pDirectionalLightSRB, std::get<Resource<DirectionalLight>> (lights).buffer);
        }

        {
            RefCntAutoPtr<IShader> pPointLightPS;
            {
                ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
                ShaderCI.EntryPoint = "main";
                ShaderCI.Desc.Name = "Point light pixel shader";
                ShaderCI.FilePath = "point_light.psh";
                m_pDevice->CreateShader(ShaderCI, &pPointLightPS);
            }

            PSOCreateInfo.pPS = pPointLightPS;
            PSOCreateInfo.PSODesc.Name = "Point Light PSO";
            m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPointLightPSO);
            BindResources(m_pPointLightPSO, &m_pPointLightSRB, std::get<Resource<PointLight>>(lights).buffer);
        }

        {
            RefCntAutoPtr<IShader> pSpotLightPS;
            {
                ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
                ShaderCI.EntryPoint = "main";
                ShaderCI.Desc.Name = "Spot light pixel shader";
                ShaderCI.FilePath = "spot_light.psh";
                m_pDevice->CreateShader(ShaderCI, &pSpotLightPS);
            }

            PSOCreateInfo.pPS = pSpotLightPS;
            PSOCreateInfo.PSODesc.Name = "Spot Light PSO";
            m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pSpotLightPSO);
            BindResources(m_pSpotLightPSO, &m_pSpotLightSRB, std::get<Resource<SpotLight>>(lights).buffer);
        }

        PSO_use = m_pDirectionalLightPSO;
        SRB_use = m_pDirectionalLightSRB;


        m_pLightCubePSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);
        m_pLightCubePSO->CreateShaderResourceBinding(&m_pLightCubeSRB, true);

    }

    void load_container_texture() {
        using namespace Diligent;

        TextureLoadInfo loadInfo;
        loadInfo.IsSRGB = true;
        RefCntAutoPtr<ITexture> Tex;
        CreateTextureFromFile(R"(..\assets\container2.png)", loadInfo, m_pDevice, &Tex);

        m_ContainerTextureSRV = Tex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

        m_pDirectionalLightSRB->GetVariableByName(SHADER_TYPE_PIXEL, "diffuse_texture")->Set(m_ContainerTextureSRV);
        m_pPointLightSRB->GetVariableByName(SHADER_TYPE_PIXEL, "diffuse_texture")->Set(m_ContainerTextureSRV);
        m_pSpotLightSRB->GetVariableByName(SHADER_TYPE_PIXEL, "diffuse_texture")->Set(m_ContainerTextureSRV);
    }

    void load_container_specular_texture() {
        using namespace Diligent;

        TextureLoadInfo loadInfo;
        loadInfo.IsSRGB = true;
        RefCntAutoPtr<ITexture> Tex;
        CreateTextureFromFile(R"(..\assets\container2_specular.png)", loadInfo, m_pDevice, &Tex);

        m_ContainerSpecularTextureSRV = Tex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

        m_pDirectionalLightSRB->GetVariableByName(SHADER_TYPE_PIXEL, "specular_texture")->Set(m_ContainerSpecularTextureSRV);
        m_pPointLightSRB->GetVariableByName(SHADER_TYPE_PIXEL, "specular_texture")->Set(m_ContainerSpecularTextureSRV);
        m_pSpotLightSRB->GetVariableByName(SHADER_TYPE_PIXEL, "specular_texture")->Set(m_ContainerSpecularTextureSRV);
    }

    void load_textures() {
        load_container_texture();
        load_container_specular_texture();
    }

    void create_uniform_buffers() {
        using namespace Diligent;

        BufferDesc CBDesc;
        CBDesc.Usage = USAGE_DYNAMIC;
        CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
        CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;

        CBDesc.Name = "VS constants CB";
        CBDesc.Size = sizeof(Constants);
        m_pDevice->CreateBuffer(CBDesc, nullptr, &m_VSConstants);

        std::apply([this, &CBDesc](auto&... Resource) {
            constexpr std::array LightResourceNames = {
                "PS Directional Light CB",
                "PS Point Light CB",
                "PS Spot Light CB"
            };
            
            size_t i = 0;
            (Resource.CreateBuffer(*m_pDevice, CBDesc, LightResourceNames[i++]), ...);
        }, lights);

        CBDesc.Name = "PS Material CB";
        CBDesc.Size = sizeof(Material);
        m_pDevice->CreateBuffer(CBDesc, nullptr, &m_PSMaterial);

        CBDesc.Name = "PS Camera CB";
        CBDesc.Size = sizeof(Camera::CB);
        m_pDevice->CreateBuffer(CBDesc, nullptr, &m_PSCamera);

    }

    void create_cube_buffer() {

        struct cube_vertex {
            glm::vec3 pos;
            glm::vec3 normal;
            glm::vec2 uv;
        };

        std::array vertices = {
            //face front
                cube_vertex{.pos = glm::vec3(-0.5f, -0.5f, -0.5f), .normal = glm::vec3(0.0f,  0.0f, -1.0f), .uv = glm::vec2(0.0f,  0.0f)},
                cube_vertex{.pos = glm::vec3( 0.5f, -0.5f, -0.5f), .normal = glm::vec3(0.0f,  0.0f, -1.0f), .uv = glm::vec2(1.0f,  0.0f)},
                cube_vertex{.pos = glm::vec3( 0.5f,  0.5f, -0.5f), .normal = glm::vec3(0.0f,  0.0f, -1.0f), .uv = glm::vec2(1.0f,  1.0f)},
                cube_vertex{.pos = glm::vec3( 0.5f,  0.5f, -0.5f), .normal = glm::vec3(0.0f,  0.0f, -1.0f), .uv = glm::vec2(1.0f,  1.0f)},
                cube_vertex{.pos = glm::vec3(-0.5f,  0.5f, -0.5f), .normal = glm::vec3(0.0f,  0.0f, -1.0f), .uv = glm::vec2(0.0f,  1.0f)},
                cube_vertex{.pos = glm::vec3(-0.5f, -0.5f, -0.5f), .normal = glm::vec3(0.0f,  0.0f, -1.0f), .uv = glm::vec2(0.0f,  0.0f)},
            //face back                                              
                cube_vertex{.pos = glm::vec3(-0.5f, -0.5f,  0.5f), .normal = glm::vec3(0.0f,  0.0f,  1.0f), .uv = glm::vec2(0.0f,  0.0f)},
                cube_vertex{.pos = glm::vec3( 0.5f, -0.5f,  0.5f), .normal = glm::vec3(0.0f,  0.0f,  1.0f), .uv = glm::vec2(1.0f,  0.0f)},
                cube_vertex{.pos = glm::vec3( 0.5f,  0.5f,  0.5f), .normal = glm::vec3(0.0f,  0.0f,  1.0f), .uv = glm::vec2(1.0f,  1.0f)},
                cube_vertex{.pos = glm::vec3( 0.5f,  0.5f,  0.5f), .normal = glm::vec3(0.0f,  0.0f,  1.0f), .uv = glm::vec2(1.0f,  1.0f)},
                cube_vertex{.pos = glm::vec3(-0.5f,  0.5f,  0.5f), .normal = glm::vec3(0.0f,  0.0f,  1.0f), .uv = glm::vec2(0.0f,  1.0f)},
                cube_vertex{.pos = glm::vec3(-0.5f, -0.5f,  0.5f), .normal = glm::vec3(0.0f,  0.0f,  1.0f), .uv = glm::vec2(0.0f,  0.0f)},
            //face right
                cube_vertex{.pos = glm::vec3(-0.5f,  0.5f,  0.5f), .normal = glm::vec3(1.0f,  0.0f,  0.0f), .uv = glm::vec2(1.0f,  0.0f)},
                cube_vertex{.pos = glm::vec3(-0.5f,  0.5f, -0.5f), .normal = glm::vec3(1.0f,  0.0f,  0.0f), .uv = glm::vec2(1.0f,  1.0f)},
                cube_vertex{.pos = glm::vec3(-0.5f, -0.5f, -0.5f), .normal = glm::vec3(1.0f,  0.0f,  0.0f), .uv = glm::vec2(0.0f,  1.0f)},
                cube_vertex{.pos = glm::vec3(-0.5f, -0.5f, -0.5f), .normal = glm::vec3(1.0f,  0.0f,  0.0f), .uv = glm::vec2(0.0f,  1.0f)},
                cube_vertex{.pos = glm::vec3(-0.5f, -0.5f,  0.5f), .normal = glm::vec3(1.0f,  0.0f,  0.0f), .uv = glm::vec2(0.0f,  0.0f)},
                cube_vertex{.pos = glm::vec3(-0.5f,  0.5f,  0.5f), .normal = glm::vec3(1.0f,  0.0f,  0.0f), .uv = glm::vec2(1.0f,  0.0f)},
            //face left
                cube_vertex{.pos = glm::vec3( 0.5f,  0.5f,  0.5f), .normal = glm::vec3(1.0f,  0.0f,  0.0f), .uv = glm::vec2(1.0f,  0.0f)},
                cube_vertex{.pos = glm::vec3( 0.5f,  0.5f, -0.5f), .normal = glm::vec3(1.0f,  0.0f,  0.0f), .uv = glm::vec2(1.0f,  1.0f)},
                cube_vertex{.pos = glm::vec3( 0.5f, -0.5f, -0.5f), .normal = glm::vec3(1.0f,  0.0f,  0.0f), .uv = glm::vec2(0.0f,  1.0f)},
                cube_vertex{.pos = glm::vec3( 0.5f, -0.5f, -0.5f), .normal = glm::vec3(1.0f,  0.0f,  0.0f), .uv = glm::vec2(0.0f,  1.0f)},
                cube_vertex{.pos = glm::vec3( 0.5f, -0.5f,  0.5f), .normal = glm::vec3(1.0f,  0.0f,  0.0f), .uv = glm::vec2(0.0f,  0.0f)},
                cube_vertex{.pos = glm::vec3( 0.5f,  0.5f,  0.5f), .normal = glm::vec3(1.0f,  0.0f,  0.0f), .uv = glm::vec2(1.0f,  0.0f)},
            //face top                                               
                cube_vertex{.pos = glm::vec3(-0.5f, -0.5f, -0.5f), .normal = glm::vec3(0.0f, -1.0f,  0.0f), .uv = glm::vec2(0.0f,  1.0f)},
                cube_vertex{.pos = glm::vec3( 0.5f, -0.5f, -0.5f), .normal = glm::vec3(0.0f, -1.0f,  0.0f), .uv = glm::vec2(1.0f,  1.0f)},
                cube_vertex{.pos = glm::vec3( 0.5f, -0.5f,  0.5f), .normal = glm::vec3(0.0f, -1.0f,  0.0f), .uv = glm::vec2(1.0f,  0.0f)},
                cube_vertex{.pos = glm::vec3( 0.5f, -0.5f,  0.5f), .normal = glm::vec3(0.0f, -1.0f,  0.0f), .uv = glm::vec2(1.0f,  0.0f)},
                cube_vertex{.pos = glm::vec3(-0.5f, -0.5f,  0.5f), .normal = glm::vec3(0.0f, -1.0f,  0.0f), .uv = glm::vec2(0.0f,  0.0f)},
                cube_vertex{.pos = glm::vec3(-0.5f, -0.5f, -0.5f), .normal = glm::vec3(0.0f, -1.0f,  0.0f), .uv = glm::vec2(0.0f,  1.0f)},
            //face bottom                                            
                cube_vertex{.pos = glm::vec3(-0.5f,  0.5f, -0.5f), .normal = glm::vec3(0.0f,  1.0f,  0.0f), .uv = glm::vec2(0.0f,  1.0f)},
                cube_vertex{.pos = glm::vec3( 0.5f,  0.5f, -0.5f), .normal = glm::vec3(0.0f,  1.0f,  0.0f), .uv = glm::vec2(1.0f,  1.0f)},
                cube_vertex{.pos = glm::vec3( 0.5f,  0.5f,  0.5f), .normal = glm::vec3(0.0f,  1.0f,  0.0f), .uv = glm::vec2(1.0f,  0.0f)},
                cube_vertex{.pos = glm::vec3( 0.5f,  0.5f,  0.5f), .normal = glm::vec3(0.0f,  1.0f,  0.0f), .uv = glm::vec2(1.0f,  0.0f)},
                cube_vertex{.pos = glm::vec3(-0.5f,  0.5f,  0.5f), .normal = glm::vec3(0.0f,  1.0f,  0.0f), .uv = glm::vec2(0.0f,  0.0f)},
                cube_vertex{.pos = glm::vec3(-0.5f,  0.5f, -0.5f), .normal = glm::vec3(0.0f,  1.0f,  0.0f), .uv = glm::vec2(0.0f,  1.0f)}       
        };

        using namespace Diligent;
        BufferDesc VertBuffDesc;
        VertBuffDesc.Name = "Cube vertex buffer";
        VertBuffDesc.Usage = USAGE_IMMUTABLE;
        VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
        VertBuffDesc.Size = vertices.size() * sizeof(decltype(vertices)::value_type);
        BufferData VBData;
        VBData.pData = vertices.data();
        VBData.DataSize = vertices.size() * sizeof(decltype(vertices)::value_type);
        m_pDevice->CreateBuffer(VertBuffDesc, &VBData, &m_CubeVertexBuffer);

    }

    void initialize_lights() {

        {
            auto& directional_light = std::get<Resource<DirectionalLight>>(lights).data;
            directional_light.direction = glm::vec3(-0.2f, -1.0f, -0.3f);
            directional_light.ambient = glm::vec3(0.2f, 0.2f, 0.2f);
            directional_light.diffuse = glm::vec3(0.7f, 0.7f, 0.7f);
            directional_light.specular = glm::vec3(1.0f, 1.0f, 1.0f);
        }

        {
            auto& point_light = std::get<Resource<PointLight>>(lights).data;
            point_light.position = glm::vec3(1.2f, 1.0f, 2.0f);
            point_light.constant = 1.0f;
            point_light.linear = 0.09f;
            point_light.quadratic = 0.032f;

            point_light.ambient = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);
            point_light.diffuse = glm::vec4(0.7f, 0.7f, 0.7f, 1.0f);
            point_light.specular = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        }
        
        {
            auto& spot_light = std::get<Resource<SpotLight>>(lights).data;

            spot_light.position = camera.eye;
            spot_light.outerCutOff = glm::cos(glm::radians(17.5f));

            spot_light.direction = camera.front;
            spot_light.cutOff = glm::cos(glm::radians(12.5f));

            spot_light.ambient = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);
            spot_light.diffuse = glm::vec4(0.7f, 0.7f, 0.7f, 1.0f);
            spot_light.specular = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        }

    }

public:

    application() {
        init();     
    }

    ~application() {
        glfwTerminate();
    }

    void run() {
        create_pipeline_states();
        load_textures();
        create_cube_buffer();
        initialize_lights();

        float delta_time = 0.0f; // Time between current frame and last frame
        float last_frame = 0.0f; // Time of last frame

        while (!glfwWindowShouldClose(window)) {

            float current_frame = glfwGetTime();
            delta_time = current_frame - last_frame;
            last_frame = current_frame;

            glfwPollEvents();
            process_input(delta_time);

            render();
        }
    }

private:

    GLFWwindow* window;
    bool show_cursor = false;
    Camera camera;

    Diligent::RefCntAutoPtr<Diligent::IEngineFactory>         m_pEngineFactory;
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice>          m_pDevice;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext>         m_pImmediateContext;
    Diligent::RefCntAutoPtr<Diligent::ISwapChain>             m_pSwapChain;

    Diligent::RefCntAutoPtr<Diligent::IBuffer>                m_CubeVertexBuffer;

    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         m_pDirectionalLightPSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pDirectionalLightSRB;

    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         m_pPointLightPSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pPointLightSRB;

    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         m_pSpotLightPSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pSpotLightSRB;

    Diligent::RefCntAutoPtr<Diligent::IBuffer>                m_VSConstants;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                m_PSMaterial;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                m_PSCamera;

    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         m_pLightCubePSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pLightCubeSRB;

    Diligent::RefCntAutoPtr<Diligent::ITextureView>           m_ContainerTextureSRV;
    Diligent::RefCntAutoPtr<Diligent::ITextureView>           m_ContainerSpecularTextureSRV;

    std::tuple<Resource<DirectionalLight>, Resource<PointLight>, Resource<SpotLight>> lights;

    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         PSO_use;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> SRB_use;
    std::variant<std::reference_wrapper<Resource<DirectionalLight>>, std::reference_wrapper<Resource<PointLight>>, std::reference_wrapper<Resource<SpotLight>>> light_use = std::ref(std::get<0>(lights));
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