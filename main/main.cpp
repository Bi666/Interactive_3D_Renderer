#include <glad.h>
#include <GLFW/glfw3.h>

#include <typeinfo>
#include <stdexcept>

#include <cstdio>
#include <cstdlib>

#include "../support/error.hpp"
#include "../support/program.hpp"
#include "../support/checkpoint.hpp"
#include "../support/debug_output.hpp"

#include "../vmlib/vec4.hpp"
#include "../vmlib/mat44.hpp"
#include "../third_party/rapidobj/include/rapidobj/rapidobj.hpp"

#include "defaults.hpp"
#include "iostream"

namespace
{
    constexpr char const* kWindowTitle = "COMP3811 - CW2";

    struct Camera {
        Vec3f position{0.0f, 0.0f, 3.0f};
        float yaw = 0.0f;
        float pitch = 0.0f;
    };

    struct Mouse {
        bool rightButtonPressed = false;
        double lastX = 400.0;
        double lastY = 300.0;
        bool firstMouse = true;
    };

    static Camera g_camera;
    static Mouse g_mouse;
	float lastFrame = 0.0f;
    float baseSpeed = 2.5f;

    GLuint g_vao = 0;
    GLuint g_vbo = 0;
    GLuint g_shaderProgram = 0;
    std::vector<float> g_vertices;
    std::vector<float> g_normals;

    struct GLFWCleanupHelper
    {
        ~GLFWCleanupHelper();
    };
    
    struct GLFWWindowDeleter
    {
        ~GLFWWindowDeleter();
        GLFWwindow* window;
    };

    void glfw_callback_mouse_(GLFWwindow* window, double xpos, double ypos)
    {
        if (!g_mouse.rightButtonPressed)
            return;

        if (g_mouse.firstMouse)
        {
            g_mouse.lastX = xpos;
            g_mouse.lastY = ypos;
            g_mouse.firstMouse = false;
            return;
        }

        float xoffset = float(xpos - g_mouse.lastX);
        float yoffset = float(g_mouse.lastY - ypos);
        g_mouse.lastX = xpos;
        g_mouse.lastY = ypos;

        float sensitivity = 0.1f;
        xoffset *= sensitivity;
        yoffset *= sensitivity;

        g_camera.yaw += xoffset;
        g_camera.pitch += yoffset;

        if(g_camera.pitch > 89.0f)
            g_camera.pitch = 89.0f;
        if(g_camera.pitch < -89.0f)
            g_camera.pitch = -89.0f;
    }

    void glfw_callback_mouse_button_(GLFWwindow* window, int button, int action, int)
    {
        if (button == GLFW_MOUSE_BUTTON_RIGHT)
        {
            if (action == GLFW_PRESS)
            {
                g_mouse.rightButtonPressed = true;
                g_mouse.firstMouse = true;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }
            else if (action == GLFW_RELEASE)
            {
                g_mouse.rightButtonPressed = false;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        }
    }

	void glfw_callback_error_( int aErrNum, char const* aErrDesc )
	{
		std::fprintf( stderr, "GLFW error: %s (%d)\n", aErrDesc, aErrNum );
	}

    void glfw_callback_key_(GLFWwindow* aWindow, int aKey, int, int aAction, int)
    {
        if (GLFW_KEY_ESCAPE == aKey && GLFW_PRESS == aAction)
        {
            glfwSetWindowShouldClose(aWindow, GLFW_TRUE);
            return;
        }

        if (aAction == GLFW_PRESS || aAction == GLFW_REPEAT)
        {
            float currentFrame = glfwGetTime();
			float deltaTime = currentFrame - lastFrame;
			lastFrame = currentFrame;
			float speed = baseSpeed * deltaTime;
            if (glfwGetKey(aWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) 
                speed *= 2.0f;
            if (glfwGetKey(aWindow, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) 
                speed *= 0.5f;

            if (aKey == GLFW_KEY_W) g_camera.position.z -= speed;
            if (aKey == GLFW_KEY_S) g_camera.position.z += speed;
            if (aKey == GLFW_KEY_A) g_camera.position.x -= speed;
            if (aKey == GLFW_KEY_D) g_camera.position.x += speed;
            if (aKey == GLFW_KEY_E) g_camera.position.y += speed;
            if (aKey == GLFW_KEY_Q) g_camera.position.y -= speed;
        }
    }

    void load_mesh_(const std::string& path)
    {
        auto result = rapidobj::ParseFile(path);
        if (result.error) {
            throw Error("Failed to load OBJ file");
        }

        g_vertices.clear();
        g_normals.clear();

        for (const auto& shape : result.shapes) {
            for (const auto& index : shape.mesh.indices) {
                // Vertices
                g_vertices.push_back(result.attributes.positions[3 * index.position_index + 0]);
                g_vertices.push_back(result.attributes.positions[3 * index.position_index + 1]);
                g_vertices.push_back(result.attributes.positions[3 * index.position_index + 2]);

                // Normals
                if (index.normal_index >= 0) {
                    g_normals.push_back(result.attributes.normals[3 * index.normal_index + 0]);
                    g_normals.push_back(result.attributes.normals[3 * index.normal_index + 1]);
                    g_normals.push_back(result.attributes.normals[3 * index.normal_index + 2]);
                }
            }
        }

        glGenVertexArrays(1, &g_vao);
        glBindVertexArray(g_vao);

        glGenBuffers(1, &g_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, g_vbo);

        std::vector<float> vertex_data;
        for (size_t i = 0; i < g_vertices.size() / 3; ++i) {
            vertex_data.push_back(g_vertices[i * 3 + 0]);
            vertex_data.push_back(g_vertices[i * 3 + 1]);
            vertex_data.push_back(g_vertices[i * 3 + 2]);
            vertex_data.push_back(g_normals[i * 3 + 0]);
            vertex_data.push_back(g_normals[i * 3 + 1]);
            vertex_data.push_back(g_normals[i * 3 + 2]);
        }

        glBufferData(GL_ARRAY_BUFFER, vertex_data.size() * sizeof(float), 
                    vertex_data.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), 
                           (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void render_scene_(int iwidth, int iheight)
    {
        glUseProgram(g_shaderProgram);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		float aspect = float(iwidth) / float(iheight);
		Mat44f projection = make_perspective_projection(
			45.0f * M_PI / 180.0f,  // FOV
			aspect,                  // 宽高比
			0.1f,                   // 近平面
			100.0f                  // 远平面
		);

        Mat44f view_matrix = make_translation(-g_camera.position);
        Mat44f rotation_x = make_rotation_x(g_camera.pitch * M_PI / 180.0f);
        Mat44f rotation_y = make_rotation_y(g_camera.yaw * M_PI / 180.0f);
        Mat44f view = rotation_x * rotation_y * view_matrix;

		// 修改为包含投影矩阵
		Mat44f pv = projection * view;  // 透视矩阵 * 视图矩阵

		// 更新着色器中的矩阵
		GLint pv_loc = glGetUniformLocation(g_shaderProgram, "pv");
		if (pv_loc != -1) {
			glUniformMatrix4fv(pv_loc, 1, GL_FALSE, pv.v);
		}

        Vec3f lightDir = normalize(Vec3f{0.0f, 1.0f, -1.0f});
        GLint lightDirLoc = glGetUniformLocation(g_shaderProgram, "lightDir");
        if (lightDirLoc != -1) {
            glUniform3fv(lightDirLoc, 1, &lightDir.x);
        }

        Vec3f lightColor{1.0f, 1.0f, 1.0f};
        GLint lightColorLoc = glGetUniformLocation(g_shaderProgram, "lightColor");
        if (lightColorLoc != -1) {
            glUniform3fv(lightColorLoc, 1, &lightColor.x);
        }

        glBindVertexArray(g_vao);
        glDrawArrays(GL_TRIANGLES, 0, g_vertices.size() / 3);
        glBindVertexArray(0);
    }

    GLuint create_shader_program_(const char* vertex_source, const char* fragment_source)
    {
        GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex_shader, 1, &vertex_source, nullptr);
        glCompileShader(vertex_shader);

        GLint success;
        glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            GLchar infoLog[512];
            glGetShaderInfoLog(vertex_shader, 512, nullptr, infoLog);
            throw Error("Vertex shader compilation failed: %s", infoLog);
        }

        GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment_shader, 1, &fragment_source, nullptr);
        glCompileShader(fragment_shader);

        glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            GLchar infoLog[512];
            glGetShaderInfoLog(fragment_shader, 512, nullptr, infoLog);
            throw Error("Fragment shader compilation failed: %s", infoLog);
        }

        GLuint program = glCreateProgram();
        glAttachShader(program, vertex_shader);
        glAttachShader(program, fragment_shader);
        glLinkProgram(program);

        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            GLchar infoLog[512];
            glGetProgramInfoLog(program, 512, nullptr, infoLog);
            throw Error("Shader program linking failed: %s", infoLog);
        }

        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);

        return program;
    }

    GLFWCleanupHelper::~GLFWCleanupHelper()
    {
        glfwTerminate();
    }

    GLFWWindowDeleter::~GLFWWindowDeleter()
    {
        if(window)
            glfwDestroyWindow(window);
    }
}

const char* kVertexShaderSource = R"(
    #version 330 core
    layout(location = 0) in vec3 aPos;
    layout(location = 1) in vec3 aNormal;

    out vec3 FragPos;
    out vec3 Normal;

    // 修改为投影视图矩阵
    uniform mat4 pv;  // 替换原来的 view

    void main()
    {
        gl_Position = pv * vec4(aPos, 1.0);  // 使用新的 pv 矩阵
        FragPos = aPos;
        Normal = aNormal;
    }
)";

const char* kFragmentShaderSource = R"(
    #version 330 core
    in vec3 Normal;
    out vec4 FragColor;

    uniform vec3 lightDir;
    uniform vec3 lightColor;

    void main()
    {
        // 添加环境光
        float ambientStrength = 0.1;
        vec3 ambient = ambientStrength * lightColor;

        vec3 norm = normalize(Normal);
        vec3 lightDirNorm = normalize(lightDir);
        float diff = max(dot(norm, lightDirNorm), 0.0);
        vec3 diffuse = lightColor * diff;

        // 合并环境光和漫反射
        vec3 result = ambient + diffuse;
        FragColor = vec4(result, 1.0);
    }
)";

int main() try
{
    // Initialize GLFW
    if( GLFW_TRUE != glfwInit() )
    {
        char const* msg = nullptr;
        int ecode = glfwGetError( &msg );
        throw Error( "glfwInit() failed with '%s' (%d)", msg, ecode );
    }

    GLFWCleanupHelper cleanupHelper;

    // Configure GLFW and create window
    glfwSetErrorCallback( &glfw_callback_error_ );

    glfwWindowHint( GLFW_SRGB_CAPABLE, GLFW_TRUE );
    glfwWindowHint( GLFW_DOUBLEBUFFER, GLFW_TRUE );

    glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 4 );
    glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 1 );
    glfwWindowHint( GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE );
    glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );

    glfwWindowHint( GLFW_DEPTH_BITS, 24 );

#   if !defined(NDEBUG)
    glfwWindowHint( GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE );
#   endif // ~ !NDEBUG

    GLFWwindow* window = glfwCreateWindow(
        1280,
        720,
        kWindowTitle,
        nullptr, nullptr
    );

    if( !window )
    {
        char const* msg = nullptr;
        int ecode = glfwGetError( &msg );
        throw Error( "glfwCreateWindow() failed with '%s' (%d)", msg, ecode );
    }

    GLFWWindowDeleter windowDeleter{ window };

    // Set up event handling
    glfwSetKeyCallback(window, &glfw_callback_key_);
    glfwSetMouseButtonCallback(window, &glfw_callback_mouse_button_);
    glfwSetCursorPosCallback(window, &glfw_callback_mouse_);

    // Set up drawing stuff
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // V-Sync is on.

    // Initialize GLAD
    if(!gladLoadGLLoader((GLADloadproc)&glfwGetProcAddress))
        throw Error("gladLoadGLLoader() failed - cannot load GL API!");

    std::printf("RENDERER %s\n", glGetString(GL_RENDERER));
    std::printf("VENDOR %s\n", glGetString(GL_VENDOR));
    std::printf("VERSION %s\n", glGetString(GL_VERSION));
    std::printf("SHADING_LANGUAGE_VERSION %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

#   if !defined(NDEBUG)
    setup_gl_debug_output();
#   endif // ~ !NDEBUG

    // Load mesh and create shaders
    load_mesh_("assets/parlahti.obj");
    g_shaderProgram = create_shader_program_(kVertexShaderSource, kFragmentShaderSource);

    // Get actual framebuffer size
    int iwidth, iheight;
    glfwGetFramebufferSize(window, &iwidth, &iheight);
    glViewport(0, 0, iwidth, iheight);

    // Global GL state
    glClearColor(0.4f, 0.4f, 0.4f, 1.0f);
    glEnable(GL_DEPTH_TEST);

    // Main loop
    while(!glfwWindowShouldClose(window))
    {
        // Let GLFW process events
        glfwPollEvents();
        
        // Check if window was resized
        float fbwidth, fbheight;
        {
            int nwidth, nheight;
            glfwGetFramebufferSize(window, &nwidth, &nheight);

            fbwidth = float(nwidth);
            fbheight = float(nheight);

            if(0 == nwidth || 0 == nheight)
            {
                // Window minimized? Pause until it is unminimized.
                do
                {
                    glfwWaitEvents();
                    glfwGetFramebufferSize(window, &nwidth, &nheight);
                } while(0 == nwidth || 0 == nheight);
            }

            glViewport(0, 0, nwidth, nheight);
        }

        // Draw scene
        OGL_CHECKPOINT_DEBUG();
        render_scene_(iwidth, iheight);
        OGL_CHECKPOINT_DEBUG();

        // Display results
        glfwSwapBuffers(window);
    }

    // Cleanup
    glDeleteVertexArrays(1, &g_vao);
    glDeleteBuffers(1, &g_vbo);
    glDeleteProgram(g_shaderProgram);

    return 0;
}
catch(std::exception const& eErr)
{
    std::fprintf(stderr, "Top-level Exception (%s):\n", typeid(eErr).name());
    std::fprintf(stderr, "%s\n", eErr.what());
    std::fprintf(stderr, "Bye.\n");
    return 1;
}
