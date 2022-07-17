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

#include "vulkan/vulkan.hpp"

#include <array>
#include <iostream>
#include <optional>
//used in 
struct Constants
{
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 projection = glm::mat4(1.0f);
};


struct Colors
{
    glm::vec3 object_color;
    glm::vec3 light_color;
};

struct Camera
{
    glm::vec3 eye       = glm::vec3(0.0f, 0.0f, 3.0f);
    glm::vec3 front     = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up        = glm::vec3(0.0f, 1.0f, 0.0f);

    float yaw = -90.0f;
    float pitch = 0.0f;

    std::optional<float> last_x;
    std::optional<float> last_y;

    double fov = 45.0;
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

        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
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
    }

    void render() {
        auto pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
        auto pDSV = m_pSwapChain->GetDepthBufferDSV();
        m_pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        const glm::vec4 ClearColor = { 0.1f, 0.1f, 0.1f, 1.0f };
        m_pImmediateContext->ClearRenderTarget(pRTV, glm::value_ptr(ClearColor), Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearDepthStencil(pDSV, Diligent::CLEAR_DEPTH_FLAG, 1.f, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        {
            m_pImmediateContext->SetPipelineState(m_pCubePSO);

            Diligent::Uint32 offset = 0;
            std::array pBuffs = { m_CubeVertexBuffer.RawPtr() };
            m_pImmediateContext->SetVertexBuffers(0, pBuffs.size(), pBuffs.data(), &offset, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION, Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);

            {
                Colors lights;
                lights.object_color = glm::vec3(1.0f, 0.5f, 0.31f);
                lights.light_color = glm::vec3(1.0f, 1.0f, 1.0f);
                Diligent::MapHelper<Colors> CBColors(m_pImmediateContext, m_FSColors, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
                *CBColors = lights;
            }


            Constants c;
            {
                const auto& SwapChainDesc = m_pSwapChain->GetDesc();
                const float aspect = static_cast<float> (SwapChainDesc.Width) / static_cast<float> (SwapChainDesc.Height);
                c.view = glm::transpose(glm::lookAt(camera.eye, camera.eye + camera.front, camera.up));
                c.projection = glm::transpose(glm::perspective(glm::radians(static_cast<float> (camera.fov)), aspect, 0.1f, 100.0f));
                c.model = glm::mat4(1.0f);

                Diligent::MapHelper<Constants> CBConstants(m_pImmediateContext, m_VSConstants, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
                *CBConstants = c;
            }

            Diligent::DrawAttribs DrawAttrs;
            DrawAttrs.NumVertices = 36;
            DrawAttrs.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;

            m_pImmediateContext->CommitShaderResources(m_pCubeSRB, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            m_pImmediateContext->Draw(DrawAttrs);


            m_pImmediateContext->SetPipelineState(m_pLightCubePSO);
            const glm::vec3 light_pos(1.2f, 1.0f, 2.0f);
            c.model = glm::translate(c.model, light_pos);
            c.model = glm::transpose(glm::scale(c.model, glm::vec3(0.2f)));
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

        PSOCreateInfo.PSODesc.Name = "Cube PSO";
        PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
        PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
        PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = m_pSwapChain->GetDesc().ColorBufferFormat;
        PSOCreateInfo.GraphicsPipeline.DSVFormat = m_pSwapChain->GetDesc().DepthBufferFormat;
        PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;

        ShaderCreateInfo ShaderCI;
        ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

        RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
        m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

        RefCntAutoPtr<IShader> pVS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
            ShaderCI.EntryPoint = "main";
            ShaderCI.Desc.Name = "Colors vertex shader";
            ShaderCI.FilePath = "colors.vsh";
            m_pDevice->CreateShader(ShaderCI, &pVS);

            BufferDesc CBDesc;
            CBDesc.Name = "VS constants CB";
            CBDesc.uiSizeInBytes = sizeof(Constants);
            CBDesc.Usage = USAGE_DYNAMIC;
            CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
            CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
            m_pDevice->CreateBuffer(CBDesc, nullptr, &m_VSConstants);
        }

        RefCntAutoPtr<IShader> pCombinedPS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
            ShaderCI.EntryPoint = "main";
            ShaderCI.Desc.Name = "Colors pixel shader";
            ShaderCI.FilePath = "colors.psh";
            m_pDevice->CreateShader(ShaderCI, &pCombinedPS);

            BufferDesc LBDesc;
            LBDesc.Name = "FS Colors CB";
            LBDesc.uiSizeInBytes = sizeof(Colors);
            LBDesc.Usage = USAGE_DYNAMIC;
            LBDesc.BindFlags = BIND_UNIFORM_BUFFER;
            LBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
            m_pDevice->CreateBuffer(LBDesc, nullptr, &m_FSColors);
        }

        std::array LayoutElems =
        {
            LayoutElement{0, 0, 3, VT_FLOAT32, False}
        };

        PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems.data();
        PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = LayoutElems.size();

        PSOCreateInfo.pVS = pVS;
        PSOCreateInfo.pPS = pCombinedPS;

        PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

        m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pCubePSO);

        m_pCubePSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);
        m_pCubePSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "Colors")->Set(m_FSColors);
        m_pCubePSO->CreateShaderResourceBinding(&m_pCubeSRB, true);


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

        m_pLightCubePSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);
        m_pLightCubePSO->CreateShaderResourceBinding(&m_pLightCubeSRB, true);

    }

    void create_cube_buffer() {

        struct cube_vertex {
            glm::vec3 pos;
        };

        std::array vertices = {
            //face front
                cube_vertex{.pos = glm::vec3(-0.5f, -0.5f,  0.5f)},
                cube_vertex{.pos = glm::vec3(0.5f,  0.5f,  0.5f)},
                cube_vertex{.pos = glm::vec3(0.5f, -0.5f,  0.5f)},
                cube_vertex{.pos = glm::vec3(-0.5f, -0.5f,  0.5f)},
                cube_vertex{.pos = glm::vec3(-0.5f,  0.5f,  0.5f)},
                cube_vertex{.pos = glm::vec3(0.5f,  0.5f,  0.5f)},
            //face back                                         
                cube_vertex{.pos = glm::vec3(0.5f,  0.5f, -0.5f)},
                cube_vertex{.pos = glm::vec3(-0.5f,  0.5f, -0.5f)},
                cube_vertex{.pos = glm::vec3(-0.5f, -0.5f, -0.5f)},
                cube_vertex{.pos = glm::vec3(0.5f,  0.5f, -0.5f)},
                cube_vertex{.pos = glm::vec3(-0.5f, -0.5f, -0.5f)},
                cube_vertex{.pos = glm::vec3(0.5f, -0.5f, -0.5f)},
            //face right                                        
                cube_vertex{.pos = glm::vec3(-0.5f, -0.5f, -0.5f)},
                cube_vertex{.pos = glm::vec3(-0.5f,  0.5f, -0.5f)},
                cube_vertex{.pos = glm::vec3(-0.5f,  0.5f,  0.5f)},
                cube_vertex{.pos = glm::vec3(-0.5f, -0.5f, -0.5f)},
                cube_vertex{.pos = glm::vec3(-0.5f,  0.5f,  0.5f)},
                cube_vertex{.pos = glm::vec3(-0.5f, -0.5f,  0.5f)},
            //face left                                         
                cube_vertex{.pos = glm::vec3(0.5f, -0.5f, -0.5f)},
                cube_vertex{.pos = glm::vec3(0.5f, -0.5f,  0.5f)},
                cube_vertex{.pos = glm::vec3(0.5f,  0.5f,  0.5f)},
                cube_vertex{.pos = glm::vec3(0.5f, -0.5f, -0.5f)},
                cube_vertex{.pos = glm::vec3(0.5f,  0.5f,  0.5f)},
                cube_vertex{.pos = glm::vec3(0.5f,  0.5f, -0.5f)},
            //face top                                          
                cube_vertex{.pos = glm::vec3(0.5f, -0.5f, -0.5f)},
                cube_vertex{.pos = glm::vec3(-0.5f, -0.5f, -0.5f)},
                cube_vertex{.pos = glm::vec3(-0.5f, -0.5f,  0.5f)},
                cube_vertex{.pos = glm::vec3(0.5f, -0.5f, -0.5f)},
                cube_vertex{.pos = glm::vec3(-0.5f, -0.5f,  0.5f)},
                cube_vertex{.pos = glm::vec3(0.5f, -0.5f,  0.5f)},
            //face bottom                                       
                cube_vertex{.pos = glm::vec3(-0.5f,  0.5f, -0.5f)},
                cube_vertex{.pos = glm::vec3(0.5f,  0.5f,  0.5f)},
                cube_vertex{.pos = glm::vec3(-0.5f,  0.5f,  0.5f)},
                cube_vertex{.pos = glm::vec3(-0.5f,  0.5f, -0.5f)},
                cube_vertex{.pos = glm::vec3(0.5f,  0.5f, -0.5f)},
                cube_vertex{.pos = glm::vec3(0.5f,  0.5f,  0.5f)}
        };

        using namespace Diligent;
        BufferDesc VertBuffDesc;
        VertBuffDesc.Name = "Cube vertex buffer";
        VertBuffDesc.Usage = USAGE_IMMUTABLE;
        VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
        VertBuffDesc.uiSizeInBytes = vertices.size() * sizeof(decltype(vertices)::value_type);
        BufferData VBData;
        VBData.pData = vertices.data();
        VBData.DataSize = vertices.size() * sizeof(decltype(vertices)::value_type);
        m_pDevice->CreateBuffer(VertBuffDesc, &VBData, &m_CubeVertexBuffer);

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
        create_cube_buffer();

        float delta_time = 0.0f;	// Time between current frame and last frame
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

    Camera camera;

    Diligent::RefCntAutoPtr<Diligent::IEngineFactory>         m_pEngineFactory;
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice>          m_pDevice;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext>         m_pImmediateContext;
    Diligent::RefCntAutoPtr<Diligent::ISwapChain>             m_pSwapChain;

    Diligent::RefCntAutoPtr<Diligent::IBuffer>                m_CubeVertexBuffer;

    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         m_pCubePSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pCubeSRB;

    Diligent::RefCntAutoPtr<Diligent::IBuffer>                m_VSConstants;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                m_FSColors;

    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         m_pLightCubePSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pLightCubeSRB;

    enum class camera_mode {
        rotating,
        fly_cam
    };

    camera_mode mode = camera_mode::rotating;
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