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

struct Camera
{
    glm::vec3 eye       = glm::vec3(0.0f, 0.0f, 3.0f);
    glm::vec3 target    = glm::vec3(0.0f, 0.0f, 0.0f);
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
            case GLFW_KEY_1:
                app->mode = camera_mode::rotating;
                app->camera.fov = 45.0;
                app->camera.eye = glm::vec3(0.0f, 0.0f, 3.0f);
                app->camera.target = glm::vec3(0.0f, 0.0f, 0.0f);
                break;
            case GLFW_KEY_2:
                app->mode = camera_mode::fly_cam;
                app->camera.fov = 45.0;
                app->camera.last_x.reset();
                app->camera.last_y.reset();
                app->camera.eye = glm::vec3(0.0f, 0.0f, 3.0f);
                app->camera.front = glm::vec3(0.0f, 0.0f, -1.0f);
                app->camera.yaw = -90.0f;
                app->camera.pitch = 0.0f;
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

        if (app->mode == camera_mode::fly_cam)
        {
            app->camera.fov = std::clamp(app->camera.fov - yoffset, 1.0, 45.0);
        }
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
        camera.last_x = width / 2;
        camera.last_y = height / 2;

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
            SCDesc.ColorBufferFormat = TEX_FORMAT_BGRA8_UNORM_SRGB;
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
        if (mode == camera_mode::fly_cam)
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
    }

    void render() {
        auto pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
        auto pDSV = m_pSwapChain->GetDepthBufferDSV();
        m_pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        const glm::vec4 ClearColor = { 0.2f, 0.3f, 0.3f, 1.0f };
        m_pImmediateContext->ClearRenderTarget(pRTV, glm::value_ptr(ClearColor), Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearDepthStencil(pDSV, Diligent::CLEAR_DEPTH_FLAG, 1.f, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        m_pImmediateContext->SetPipelineState(m_pCombinedPSO);


        Diligent::Uint64 offset = 0;
        std::array pBuffs = { m_CubeVertexBuffer.RawPtr() };
        m_pImmediateContext->SetVertexBuffers(0, pBuffs.size(), pBuffs.data(), &offset, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION, Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);

        {
            Constants c;

            const auto& SwapChainDesc = m_pSwapChain->GetDesc();
            const float aspect = static_cast<float> (SwapChainDesc.Width) / static_cast<float> (SwapChainDesc.Height);

            switch (mode)
            {
            case camera_mode::rotating:
            {
                c.projection = glm::transpose(glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f));

                const float radius = 10.0f;
                float camX = std::sin(glfwGetTime()) * radius;
                float camZ = std::cos(glfwGetTime()) * radius;
                camera.eye = glm::vec3(camX, 0.0f, camZ);

                c.view = glm::transpose(glm::lookAt(camera.eye, camera.target, camera.up));
            }
                break;
            case camera_mode::fly_cam:

                c.view = glm::transpose(glm::lookAt(camera.eye, camera.eye + camera.front, camera.up));
                c.projection = glm::transpose(glm::perspective(glm::radians(static_cast<float> (camera.fov)), aspect, 0.1f, 100.0f));

                break;
            }
           
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
                c.model = glm::translate(glm::mat4(1.0f), cube_position);
                float angle = 20.0f * i++;
                c.model = glm::transpose(glm::rotate(c.model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f)));

                Diligent::MapHelper<Constants> CBConstants(m_pImmediateContext, m_VSConstants, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
                *CBConstants = c;
                m_pImmediateContext->CommitShaderResources(m_pCombinedSRB, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                m_pImmediateContext->Draw(DrawAttrs);
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
            ShaderCI.Desc.Name = "Coordinate systems vertex shader";
            ShaderCI.FilePath = "coordinate_systems.vsh";
            m_pDevice->CreateShader(ShaderCI, &pVS);

            BufferDesc CBDesc;
            CBDesc.Name = "VS constants CB";
            CBDesc.Size = sizeof(Constants);
            CBDesc.Usage = USAGE_DYNAMIC;
            CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
            CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
            m_pDevice->CreateBuffer(CBDesc, nullptr, &m_VSConstants);
        }

        RefCntAutoPtr<IShader> pCombinedPS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
            ShaderCI.EntryPoint = "main";
            ShaderCI.Desc.Name = "Combined Texture pixel shader";
            ShaderCI.FilePath = "combined_texture.psh";
            m_pDevice->CreateShader(ShaderCI, &pCombinedPS);
        }

        std::array LayoutElems =
        {
            LayoutElement{0, 0, 3, VT_FLOAT32, False},
            LayoutElement{1, 0, 2, VT_FLOAT32, False}
        };

        PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems.data();
        PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = LayoutElems.size();

        PSOCreateInfo.pVS = pVS;
        PSOCreateInfo.pPS = pCombinedPS;

        PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

        std::array CombinedVars =
        {
            ShaderResourceVariableDesc{SHADER_TYPE_PIXEL, "g_ContainerTexture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
            ShaderResourceVariableDesc{SHADER_TYPE_PIXEL, "g_FaceTexture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
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
            ImmutableSamplerDesc {SHADER_TYPE_PIXEL, "g_Texture_sampler", SamLinearClampDesc}
        };

        PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers = CombinedImtblSamplers.data();
        PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = CombinedImtblSamplers.size();

        m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pCombinedPSO);

        m_pCombinedPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);

        m_pCombinedPSO->CreateShaderResourceBinding(&m_pCombinedSRB, true);


    }

    void create_cube_buffer() {

        struct cube_vertex {
            glm::vec3 pos;
            glm::vec2 uv;
        };

        std::array vertices = {
            //face front
                cube_vertex{.pos = glm::vec3(-0.5f, -0.5f,  0.5f), .uv = glm::vec2(0.0f, 1.0f) },
                cube_vertex{.pos = glm::vec3(0.5f,  0.5f,  0.5f), .uv = glm::vec2(1.0f, 0.0f) },
                cube_vertex{.pos = glm::vec3(0.5f, -0.5f,  0.5f), .uv = glm::vec2(1.0f, 1.0f) },
                cube_vertex{.pos = glm::vec3(-0.5f, -0.5f,  0.5f), .uv = glm::vec2(0.0f, 1.0f) },
                cube_vertex{.pos = glm::vec3(-0.5f,  0.5f,  0.5f), .uv = glm::vec2(0.0f, 0.0f) },
                cube_vertex{.pos = glm::vec3(0.5f,  0.5f,  0.5f), .uv = glm::vec2(1.0f, 0.0f) },
            //face back                                         
                cube_vertex{.pos = glm::vec3(0.5f,  0.5f, -0.5f), .uv = glm::vec2(1.0f, 0.0f) },
                cube_vertex{.pos = glm::vec3(-0.5f,  0.5f, -0.5f), .uv = glm::vec2(0.0f, 0.0f) },
                cube_vertex{.pos = glm::vec3(-0.5f, -0.5f, -0.5f), .uv = glm::vec2(0.0f, 1.0f) },
                cube_vertex{.pos = glm::vec3(0.5f,  0.5f, -0.5f), .uv = glm::vec2(1.0f, 0.0f) },
                cube_vertex{.pos = glm::vec3(-0.5f, -0.5f, -0.5f), .uv = glm::vec2(0.0f, 1.0f) },
                cube_vertex{.pos = glm::vec3(0.5f, -0.5f, -0.5f), .uv = glm::vec2(1.0f, 1.0f) },
            //face right                                        
                cube_vertex{.pos = glm::vec3(-0.5f, -0.5f, -0.5f), .uv = glm::vec2(0.0f, 1.0f) },
                cube_vertex{.pos = glm::vec3(-0.5f,  0.5f, -0.5f), .uv = glm::vec2(1.0f, 1.0f) },
                cube_vertex{.pos = glm::vec3(-0.5f,  0.5f,  0.5f), .uv = glm::vec2(1.0f, 0.0f) },
                cube_vertex{.pos = glm::vec3(-0.5f, -0.5f, -0.5f), .uv = glm::vec2(0.0f, 1.0f) },
                cube_vertex{.pos = glm::vec3(-0.5f,  0.5f,  0.5f), .uv = glm::vec2(1.0f, 0.0f) },
                cube_vertex{.pos = glm::vec3(-0.5f, -0.5f,  0.5f), .uv = glm::vec2(0.0f, 0.0f) },
            //face left                                         
                cube_vertex{.pos = glm::vec3(0.5f, -0.5f, -0.5f), .uv = glm::vec2(0.0f, 1.0f) },
                cube_vertex{.pos = glm::vec3(0.5f, -0.5f,  0.5f), .uv = glm::vec2(0.0f, 0.0f) },
                cube_vertex{.pos = glm::vec3(0.5f,  0.5f,  0.5f), .uv = glm::vec2(1.0f, 0.0f) },
                cube_vertex{.pos = glm::vec3(0.5f, -0.5f, -0.5f), .uv = glm::vec2(0.0f, 1.0f) },
                cube_vertex{.pos = glm::vec3(0.5f,  0.5f,  0.5f), .uv = glm::vec2(1.0f, 0.0f) },
                cube_vertex{.pos = glm::vec3(0.5f,  0.5f, -0.5f), .uv = glm::vec2(1.0f, 1.0f) },
            //face top                                          
                cube_vertex{.pos = glm::vec3(0.5f, -0.5f, -0.5f), .uv = glm::vec2(1.0f, 1.0f) },
                cube_vertex{.pos = glm::vec3(-0.5f, -0.5f, -0.5f), .uv = glm::vec2(0.0f, 1.0f) },
                cube_vertex{.pos = glm::vec3(-0.5f, -0.5f,  0.5f), .uv = glm::vec2(0.0f, 0.0f) },
                cube_vertex{.pos = glm::vec3(0.5f, -0.5f, -0.5f), .uv = glm::vec2(1.0f, 1.0f) },
                cube_vertex{.pos = glm::vec3(-0.5f, -0.5f,  0.5f), .uv = glm::vec2(0.0f, 0.0f) },
                cube_vertex{.pos = glm::vec3(0.5f, -0.5f,  0.5f), .uv = glm::vec2(1.0f, 0.0f) },
            //face bottom                                       
                cube_vertex{.pos = glm::vec3(-0.5f,  0.5f, -0.5f), .uv = glm::vec2(0.0f, 1.0f) },
                cube_vertex{.pos = glm::vec3(0.5f,  0.5f,  0.5f), .uv = glm::vec2(1.0f, 0.0f) },
                cube_vertex{.pos = glm::vec3(-0.5f,  0.5f,  0.5f), .uv = glm::vec2(0.0f, 0.0f) },
                cube_vertex{.pos = glm::vec3(-0.5f,  0.5f, -0.5f), .uv = glm::vec2(0.0f, 1.0f) },
                cube_vertex{.pos = glm::vec3(0.5f,  0.5f, -0.5f), .uv = glm::vec2(1.0f, 1.0f) },
                cube_vertex{.pos = glm::vec3(0.5f,  0.5f,  0.5f), .uv = glm::vec2(1.0f, 0.0f) }
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

    void load_container_texture() {
        using namespace Diligent;

        TextureLoadInfo loadInfo;
        loadInfo.IsSRGB = true;
        RefCntAutoPtr<ITexture> Tex;
        CreateTextureFromFile(R"(..\assets\container.jpg)", loadInfo, m_pDevice, &Tex);

        m_ContainerTextureSRV = Tex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

        m_pCombinedSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_ContainerTexture")->Set(m_ContainerTextureSRV);
    }

    void load_awesome_face_texture() {
        using namespace Diligent;

        TextureLoadInfo loadInfo;
        loadInfo.IsSRGB = true;
        RefCntAutoPtr<ITexture> Tex;
        CreateTextureFromFile(R"(..\assets\awesomeface.png)", loadInfo, m_pDevice, &Tex);

        m_FaceTextureSRV = Tex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

        m_pCombinedSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_FaceTexture")->Set(m_FaceTextureSRV);
    }

    void load_textures() {
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
        create_cube_buffer();
        load_textures();

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

    Diligent::RefCntAutoPtr<Diligent::ITextureView>           m_ContainerTextureSRV;
    Diligent::RefCntAutoPtr<Diligent::ITextureView>           m_FaceTextureSRV;

    Diligent::RefCntAutoPtr<Diligent::IBuffer>                m_CubeVertexBuffer;

    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         m_pCombinedPSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pCombinedSRB;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                m_VSConstants;

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