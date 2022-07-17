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

#include "boost/any.hpp"
#include <array>
#include <iostream>
#include <optional>
//used in 
struct Constants
{
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 projection = glm::mat4(1.0f);
    glm::mat4 inverse_transpose_model;
};


struct Material {
    alignas(16) glm::vec3 ambient;
    alignas(16) glm::vec3 diffuse;
    alignas(16) glm::vec3 specular; 
    float shininess;
};

namespace MaterialsDictionary {

using namespace std::string_view_literals;

struct MaterialEntry {
    std::string_view name;
    Material material;
};

constexpr MaterialEntry emerald = MaterialEntry{.name = "emerald"sv, .material = {.ambient = glm::vec3(0.0215, 0.1745, 0.0215), .diffuse = glm::vec3(0.07568, 0.61424, 0.07568), .specular = glm::vec3(0.633, 0.727811, 0.633), .shininess = 0.6 }};
constexpr MaterialEntry jade = MaterialEntry{ .name = "jade"sv, .material = {.ambient = glm::vec3(0.135, 0.2225, 0.1575), .diffuse = glm::vec3(0.54, 0.89, 0.63), .specular = glm::vec3(0.316228, 0.316228, 0.316228), .shininess = 0.1 } };
constexpr MaterialEntry obsidian = MaterialEntry{ .name = "obsidian"sv, .material = {.ambient = glm::vec3(0.05375, 0.05, 0.06625), .diffuse = glm::vec3(0.18275, 0.17, 0.22525), .specular = glm::vec3(0.332741, 0.328634, 0.346435), .shininess = 0.3 } };
constexpr MaterialEntry pearl = MaterialEntry{ .name = "pearl"sv, .material = {.ambient = glm::vec3(0.25, 0.20725, 0.20725), .diffuse = glm::vec3(1, 0.829, 0.829), .specular = glm::vec3(0.296648, 0.296648, 0.296648), .shininess = 0.088 } };
constexpr MaterialEntry ruby = MaterialEntry{ .name = "ruby"sv, .material = {.ambient = glm::vec3(0.1745, 0.01175, 0.01175), .diffuse = glm::vec3(0.61424, 0.04136, 0.04136), .specular = glm::vec3(0.727811, 0.626959, 0.626959), .shininess = 0.6 } };
constexpr MaterialEntry turquoise = MaterialEntry{ .name = "turquoise"sv, .material = {.ambient = glm::vec3(0.1, 0.18725, 0.1745), .diffuse = glm::vec3(0.396, 0.74151, 0.69102), .specular = glm::vec3(0.297254, 0.30829, 0.306678), .shininess = 0.1 } };
constexpr MaterialEntry brass = MaterialEntry{ .name = "brass"sv, .material = {.ambient = glm::vec3(0.329412, 0.223529, 0.027451), .diffuse = glm::vec3(0.780392, 0.568627, 0.113725), .specular = glm::vec3(0.992157, 0.941176, 0.807843), .shininess = 0.21794872 } };
constexpr MaterialEntry bronze = MaterialEntry{ .name = "bronze"sv, .material = {.ambient = glm::vec3(0.2125, 0.1275, 0.054), .diffuse = glm::vec3(0.714, 0.4284, 0.18144), .specular = glm::vec3(0.393548, 0.271906, 0.166721), .shininess = 0.2 } };
constexpr MaterialEntry chrome = MaterialEntry{ .name = "chrome"sv, .material = {.ambient = glm::vec3(0.25, 0.25, 0.25), .diffuse = glm::vec3(0.4, 0.4, 0.4), .specular = glm::vec3(0.774597, 0.774597, 0.774597), .shininess = 0.6 } };
constexpr MaterialEntry copper = MaterialEntry{ .name = "copper"sv, .material = {.ambient = glm::vec3(0.19125, 0.0735, 0.0225), .diffuse = glm::vec3(0.7038, 0.27048, 0.0828), .specular = glm::vec3(0.256777, 0.137622, 0.086014), .shininess = 0.1 } };
constexpr MaterialEntry gold = MaterialEntry{ .name = "gold"sv, .material = {.ambient = glm::vec3(0.24725, 0.1995, 0.0745), .diffuse = glm::vec3(0.75164, 0.60648, 0.22648), .specular = glm::vec3(0.628281, 0.555802, 0.366065), .shininess = 0.4 } };
constexpr MaterialEntry silver = MaterialEntry{ .name = "silver"sv, .material = {.ambient = glm::vec3(0.19225, 0.19225, 0.19225), .diffuse = glm::vec3(0.50754, 0.50754, 0.50754), .specular = glm::vec3(0.508273, 0.508273, 0.508273), .shininess = 0.4 } };
constexpr MaterialEntry black_plastic = MaterialEntry{ .name = "black_plastic"sv, .material = {.ambient = glm::vec3(0.0, 0.0, 0.0), .diffuse = glm::vec3(0.01, 0.01, 0.01), .specular = glm::vec3(0.50, 0.50, 0.50), .shininess = .25 } };
constexpr MaterialEntry cyan_plastic = MaterialEntry{ .name = "cyan_plastic"sv, .material = {.ambient = glm::vec3(0.0, 0.1, 0.06), .diffuse = glm::vec3(0.0, 0.50980392, 0.50980392), .specular = glm::vec3(0.50196078, 0.50196078, 0.50196078), .shininess = .25 } };
constexpr MaterialEntry green_plastic = MaterialEntry{ .name = "green_plastic"sv, .material = {.ambient = glm::vec3(0.0, 0.0, 0.0), .diffuse = glm::vec3(0.1, 0.35, 0.1), .specular = glm::vec3(0.45, 0.55, 0.45), .shininess = .25 } };
constexpr MaterialEntry red_plastic = MaterialEntry{ .name = "red_plastic"sv, .material = {.ambient = glm::vec3(0.0, 0.0, 0.0), .diffuse = glm::vec3(0.5, 0.0, 0.0), .specular = glm::vec3(0.7, 0.6, 0.6), .shininess = .25 } };
constexpr MaterialEntry white_plastic = MaterialEntry{ .name = "white_plastic"sv, .material = {.ambient = glm::vec3(0.0, 0.0, 0.0), .diffuse = glm::vec3(0.55, 0.55, 0.55), .specular = glm::vec3(0.70, 0.70, 0.70), .shininess = .25 } };
constexpr MaterialEntry yellow_plastic = MaterialEntry{ .name = "yellow_plastic"sv, .material = {.ambient = glm::vec3(0.0, 0.0, 0.0), .diffuse = glm::vec3(0.5, 0.5, 0.0), .specular = glm::vec3(0.60, 0.60, 0.50), .shininess = .25 } };
constexpr MaterialEntry black_rubber = MaterialEntry{ .name = "black_rubber"sv, .material = {.ambient = glm::vec3(0.02, 0.02, 0.02), .diffuse = glm::vec3(0.01, 0.01, 0.01), .specular = glm::vec3(0.4, 0.4, 0.4), .shininess = .078125 } };
constexpr MaterialEntry cyan_rubber = MaterialEntry{ .name = "cyan_rubber"sv, .material = {.ambient = glm::vec3(0.0, 0.05, 0.05), .diffuse = glm::vec3(0.4, 0.5, 0.5), .specular = glm::vec3(0.04, 0.7, 0.7), .shininess = .078125 } };
constexpr MaterialEntry green_rubber = MaterialEntry{ .name = "green_rubber"sv, .material = {.ambient = glm::vec3(0.0, 0.05, 0.0), .diffuse = glm::vec3(0.4, 0.5, 0.4), .specular = glm::vec3(0.04, 0.7, 0.04), .shininess = .078125 } };
constexpr MaterialEntry red_rubber = MaterialEntry{ .name = "red_rubber"sv, .material = {.ambient = glm::vec3(0.05, 0.0, 0.0), .diffuse = glm::vec3(0.5, 0.4, 0.4), .specular = glm::vec3(0.7, 0.04, 0.04), .shininess = .078125 } };
constexpr MaterialEntry white_rubber = MaterialEntry{ .name = "white_rubber"sv, .material = {.ambient = glm::vec3(0.05, 0.05, 0.05), .diffuse = glm::vec3(0.5, 0.5, 0.5), .specular = glm::vec3(0.7, 0.7, 0.7), .shininess = .078125 } };
constexpr MaterialEntry yellow_rubber = MaterialEntry{ .name = "yellow_rubber"sv, .material = {.ambient = glm::vec3(0.05, 0.05, 0.0), .diffuse = glm::vec3(0.5, 0.5, 0.4), .specular = glm::vec3(0.7, 0.7, 0.04), .shininess = .078125 } };

constexpr MaterialEntry coral = MaterialEntry{ .name = "coral"sv, .material = {.ambient = glm::vec3(1.0f, 0.5f, 0.31f), .diffuse = glm::vec3(1.0f, 0.5f, 0.31f), .specular = glm::vec3(0.5f, 0.5f, 0.5f) , .shininess = 32.0f/128.0f } };

constexpr std::array materials = {
    emerald,
    jade,
    obsidian,
    pearl,
    ruby,
    turquoise,
    brass,
    bronze,
    chrome,
    copper,
    gold,
    silver,
    black_plastic,
    cyan_plastic,
    green_plastic,
    red_plastic,
    white_plastic,
    yellow_plastic,
    black_rubber,
    cyan_rubber,
    green_rubber,
    red_rubber,
    white_rubber,
    yellow_rubber,
    coral
};

}

struct Light {
    alignas(16) glm::vec3 position;

    alignas(16) glm::vec3 ambient;
    alignas(16) glm::vec3 diffuse;
    alignas(16) glm::vec3 specular;
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
                app->mode = render_mode::coral_cube;
                break;
            case GLFW_KEY_2:
                app->mode = render_mode::many_cubes;
                break;
            case GLFW_KEY_LEFT_SHIFT:
            {
                app->show_cursor = !app->show_cursor;
                glfwSetInputMode(window, GLFW_CURSOR, app->show_cursor ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
            }
            break;
            case GLFW_KEY_LEFT:
                --app->single_cube_material_index %= MaterialsDictionary::materials.size();
                std::cout << MaterialsDictionary::materials.at(app->single_cube_material_index).name << "\n";
                break;
            case GLFW_KEY_RIGHT:
                ++app->single_cube_material_index %= MaterialsDictionary::materials.size();
                std::cout << MaterialsDictionary::materials.at(app->single_cube_material_index).name << "\n";
                break;
            }
        }
        else if (action == GLFW_RELEASE) {
            if (key == GLFW_KEY_LEFT_SHIFT)
            {
                app->show_cursor = !app->show_cursor;
                glfwSetInputMode(window, GLFW_CURSOR, app->show_cursor ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
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

            glm::mat4 light_model(1.0f);
            light_model = glm::rotate(light_model, static_cast<float> (glfwGetTime ()) * glm::radians(50.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            light_model = glm::translate(light_model, glm::vec3(10.2f, 1.0f, 12.0f));
            const glm::vec4 light_pos = (light_model * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
            {
                Diligent::MapHelper<Light> CBLight(m_pImmediateContext, m_PSLight, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);

                CBLight->position = glm::vec3(light_pos.x, light_pos.y, light_pos.z);

                if (mode == render_mode::coral_cube && single_cube_material_index == MaterialsDictionary::materials.size() - 1) {
                    CBLight->ambient = glm::vec3(0.2f, 0.2f, 0.2f);
                    CBLight->diffuse = glm::vec3(0.5f, 0.5f, 0.5f);
                    CBLight->specular = glm::vec3(1.0f, 1.0f, 1.0f);
                }
                else {
                    CBLight->ambient = glm::vec3(1.0f);
                    CBLight->diffuse = glm::vec3(1.0f);
                    CBLight->specular = glm::vec3(1.0f);
                }

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

            const auto render_cube = [&](const glm::mat4& model, const Material& material) {
                {
                    c.model = glm::transpose(model);
                    c.inverse_transpose_model = glm::transpose(glm::transpose(glm::inverse(model)));

                    Diligent::MapHelper<Constants> CBConstants(m_pImmediateContext, m_VSConstants, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
                    *CBConstants = c;
                }
                {
                    Diligent::MapHelper<Material> CBMaterial(m_pImmediateContext, m_PSMaterial, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
                    *CBMaterial = material;
                    CBMaterial->shininess *= 128.0f;
                }

                m_pImmediateContext->CommitShaderResources(m_pCubeSRB, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                m_pImmediateContext->Draw(DrawAttrs);
            };

            Material material{ .ambient = glm::vec3(1.0f, 0.5f, 0.31f), .diffuse = glm::vec3(1.0f, 0.5f, 0.31f), .specular = glm::vec3(0.5f, 0.5f, 0.5f) , .shininess = 32.0f };


            switch (mode)
            {
            case render_mode::coral_cube:
            {
                render_cube(glm::mat4(1.0f), MaterialsDictionary::materials.at(single_cube_material_index).material);
            }
                break;
            case render_mode::many_cubes:
            {   
                for (int i = -2; i <= 2; ++i) {
                    for (int j = -2; j <= 2; ++j) {


                        const auto& m = MaterialsDictionary::materials[5 * i + j + 12];

                        glm::mat4 model = glm::mat4(1.0f);
                        model = glm::translate(model, glm::vec3(i * 3.0f , j * 3.0f,  0.0f));

                        render_cube(model, m.material);

                    }
                }
            }
                break;
            }




            

            render_cube(glm::mat4(1.0f), material);

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

        PSOCreateInfo.PSODesc.Name = "Cube PSO";
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

        RefCntAutoPtr<IShader> pVS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
            ShaderCI.EntryPoint = "main";
            ShaderCI.Desc.Name = "Colors vertex shader";
            ShaderCI.FilePath = "colors.vsh";
            m_pDevice->CreateShader(ShaderCI, &pVS);
        }

        RefCntAutoPtr<IShader> pCombinedPS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
            ShaderCI.EntryPoint = "main";
            ShaderCI.Desc.Name = "Colors pixel shader";
            ShaderCI.FilePath = "colors.psh";
            m_pDevice->CreateShader(ShaderCI, &pCombinedPS);
        }

        std::array LayoutElems =
        {
            LayoutElement{0, 0, 3, VT_FLOAT32, False},
            LayoutElement{1, 0, 3, VT_FLOAT32, False}
        };

        PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems.data();
        PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = LayoutElems.size();

        PSOCreateInfo.pVS = pVS;
        PSOCreateInfo.pPS = pCombinedPS;

        PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

        m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pCubePSO);

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


        create_uniform_buffers();


        m_pCubePSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);
        m_pLightCubePSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);

        m_pCubePSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "Lights")->Set(m_PSLight);
        m_pCubePSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "Materials")->Set(m_PSMaterial);
        m_pCubePSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "Camera")->Set(m_PSCamera);

        m_pCubePSO->CreateShaderResourceBinding(&m_pCubeSRB, true);
        m_pLightCubePSO->CreateShaderResourceBinding(&m_pLightCubeSRB, true);

    }

    void create_uniform_buffers() {
        using namespace Diligent;

        BufferDesc CBDesc;
        CBDesc.Usage = USAGE_DYNAMIC;
        CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
        CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;

        CBDesc.Name = "VS constants CB";
        CBDesc.uiSizeInBytes = sizeof(Constants);
        m_pDevice->CreateBuffer(CBDesc, nullptr, &m_VSConstants);

        CBDesc.Name = "PS Light CB";
        CBDesc.uiSizeInBytes = sizeof(Light);
        m_pDevice->CreateBuffer(CBDesc, nullptr, &m_PSLight);

        CBDesc.Name = "PS Material CB";
        CBDesc.uiSizeInBytes = sizeof(Material);
        m_pDevice->CreateBuffer(CBDesc, nullptr, &m_PSMaterial);

        CBDesc.Name = "PS Camera CB";
        CBDesc.uiSizeInBytes = sizeof(Camera::CB);
        m_pDevice->CreateBuffer(CBDesc, nullptr, &m_PSCamera);

    }

    void create_cube_buffer() {

        struct cube_vertex {
            glm::vec3 pos;
            glm::vec3 normal;
        };

        std::array vertices = {
            //face front
                cube_vertex{.pos = glm::vec3( -0.5f, -0.5f, -0.5f), .normal = glm::vec3( 0.0f,  0.0f, -1.0f)},
                cube_vertex{.pos = glm::vec3(  0.5f, -0.5f, -0.5f), .normal = glm::vec3( 0.0f,  0.0f, -1.0f)},
                cube_vertex{.pos = glm::vec3(  0.5f,  0.5f, -0.5f), .normal = glm::vec3( 0.0f,  0.0f, -1.0f)},
                cube_vertex{.pos = glm::vec3(  0.5f,  0.5f, -0.5f), .normal = glm::vec3( 0.0f,  0.0f, -1.0f)},
                cube_vertex{.pos = glm::vec3( -0.5f,  0.5f, -0.5f), .normal = glm::vec3( 0.0f,  0.0f, -1.0f)},
                cube_vertex{.pos = glm::vec3( -0.5f, -0.5f, -0.5f), .normal = glm::vec3( 0.0f,  0.0f, -1.0f)},
            //face back                                                                  
                cube_vertex{.pos = glm::vec3( -0.5f, -0.5f,  0.5f), .normal = glm::vec3( 0.0f,  0.0f,  1.0f)},
                cube_vertex{.pos = glm::vec3(  0.5f, -0.5f,  0.5f), .normal = glm::vec3( 0.0f,  0.0f,  1.0f)},
                cube_vertex{.pos = glm::vec3(  0.5f,  0.5f,  0.5f), .normal = glm::vec3( 0.0f,  0.0f,  1.0f)},
                cube_vertex{.pos = glm::vec3(  0.5f,  0.5f,  0.5f), .normal = glm::vec3( 0.0f,  0.0f,  1.0f)},
                cube_vertex{.pos = glm::vec3( -0.5f,  0.5f,  0.5f), .normal = glm::vec3( 0.0f,  0.0f,  1.0f)},
                cube_vertex{.pos = glm::vec3( -0.5f, -0.5f,  0.5f), .normal = glm::vec3( 0.0f,  0.0f,  1.0f)},
            //face right
                cube_vertex{.pos = glm::vec3( -0.5f,  0.5f,  0.5f), .normal = glm::vec3(-1.0f,  0.0f,  0.0f)},
                cube_vertex{.pos = glm::vec3( -0.5f,  0.5f, -0.5f), .normal = glm::vec3(-1.0f,  0.0f,  0.0f)},
                cube_vertex{.pos = glm::vec3( -0.5f, -0.5f, -0.5f), .normal = glm::vec3(-1.0f,  0.0f,  0.0f)},
                cube_vertex{.pos = glm::vec3( -0.5f, -0.5f, -0.5f), .normal = glm::vec3(-1.0f,  0.0f,  0.0f)},
                cube_vertex{.pos = glm::vec3( -0.5f, -0.5f,  0.5f), .normal = glm::vec3(-1.0f,  0.0f,  0.0f)},
                cube_vertex{.pos = glm::vec3( -0.5f,  0.5f,  0.5f), .normal = glm::vec3(-1.0f,  0.0f,  0.0f)},
            //face left
                cube_vertex{.pos = glm::vec3(  0.5f,  0.5f,  0.5f), .normal = glm::vec3( 1.0f,  0.0f,  0.0f)},
                cube_vertex{.pos = glm::vec3(  0.5f,  0.5f, -0.5f), .normal = glm::vec3( 1.0f,  0.0f,  0.0f)},
                cube_vertex{.pos = glm::vec3(  0.5f, -0.5f, -0.5f), .normal = glm::vec3( 1.0f,  0.0f,  0.0f)},
                cube_vertex{.pos = glm::vec3(  0.5f, -0.5f, -0.5f), .normal = glm::vec3( 1.0f,  0.0f,  0.0f)},
                cube_vertex{.pos = glm::vec3(  0.5f, -0.5f,  0.5f), .normal = glm::vec3( 1.0f,  0.0f,  0.0f)},
                cube_vertex{.pos = glm::vec3(  0.5f,  0.5f,  0.5f), .normal = glm::vec3( 1.0f,  0.0f,  0.0f)},
            //face top                                                                   
                cube_vertex{.pos = glm::vec3( -0.5f, -0.5f, -0.5f), .normal = glm::vec3( 0.0f, -1.0f,  0.0f)},
                cube_vertex{.pos = glm::vec3(  0.5f, -0.5f, -0.5f), .normal = glm::vec3( 0.0f, -1.0f,  0.0f)},
                cube_vertex{.pos = glm::vec3(  0.5f, -0.5f,  0.5f), .normal = glm::vec3( 0.0f, -1.0f,  0.0f)},
                cube_vertex{.pos = glm::vec3(  0.5f, -0.5f,  0.5f), .normal = glm::vec3( 0.0f, -1.0f,  0.0f)},
                cube_vertex{.pos = glm::vec3( -0.5f, -0.5f,  0.5f), .normal = glm::vec3( 0.0f, -1.0f,  0.0f)},
                cube_vertex{.pos = glm::vec3( -0.5f, -0.5f, -0.5f), .normal = glm::vec3( 0.0f, -1.0f,  0.0f)},
            //face bottom                                                                
                cube_vertex{.pos = glm::vec3( -0.5f,  0.5f, -0.5f), .normal = glm::vec3( 0.0f,  1.0f,  0.0f)},
                cube_vertex{.pos = glm::vec3(  0.5f,  0.5f, -0.5f), .normal = glm::vec3( 0.0f,  1.0f,  0.0f)},
                cube_vertex{.pos = glm::vec3(  0.5f,  0.5f,  0.5f), .normal = glm::vec3( 0.0f,  1.0f,  0.0f)},
                cube_vertex{.pos = glm::vec3(  0.5f,  0.5f,  0.5f), .normal = glm::vec3( 0.0f,  1.0f,  0.0f)},
                cube_vertex{.pos = glm::vec3( -0.5f,  0.5f,  0.5f), .normal = glm::vec3( 0.0f,  1.0f,  0.0f)},
                cube_vertex{.pos = glm::vec3( -0.5f,  0.5f, -0.5f), .normal = glm::vec3( 0.0f,  1.0f,  0.0f)}
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
    bool show_cursor = false;
    Camera camera;

    Diligent::RefCntAutoPtr<Diligent::IEngineFactory>         m_pEngineFactory;
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice>          m_pDevice;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext>         m_pImmediateContext;
    Diligent::RefCntAutoPtr<Diligent::ISwapChain>             m_pSwapChain;

    Diligent::RefCntAutoPtr<Diligent::IBuffer>                m_CubeVertexBuffer;

    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         m_pCubePSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pCubeSRB;

    Diligent::RefCntAutoPtr<Diligent::IBuffer>                m_VSConstants;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                m_PSLight;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                m_PSMaterial;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                m_PSCamera;

    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         m_pLightCubePSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pLightCubeSRB;

    enum class render_mode {
        coral_cube,
        many_cubes
    };

    render_mode mode = render_mode::coral_cube;

    size_t single_cube_material_index = MaterialsDictionary::materials.size() - 1;
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