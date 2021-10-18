/**
 * @file        gpu_fractals.cpp
 * 
 * @brief       Creates a window containing the rendering of a fractal computed with an OpenGL compute shader.
 * 
 * @author      Filippo Maggioli\n 
 *              (maggioli@di.uniroma1.it, maggioli.filippo@gmail.com)\n 
 *              Sapienza, University of Rome - Department of Computer Science
 * @date        2021-10-18
 */
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <string>
#define _USE_MATH_DEFINES
#include <math.h>


#include <defines.hpp>



void fbcallback(GLFWwindow* Window, int Width, int Height)
{
    glViewport(0, 0, Width, Height);
}


const char *VSource = 
"#version 440 core\n"\
"layout(location = 0) in vec2 aPos;\n"\
"layout(location = 1) in vec2 aTex;\n"\
"out vec2 fTex;\n"\
"void main() {\n"\
"gl_Position = vec4(aPos, 0.0f, 1.0f);\n"\
"fTex = aTex;\n"\
"}\n";

const char *FSource =
"#version 440 core\n"\
"in vec2 fTex;\n"\
"layout(binding = 0) uniform sampler2D Tex;\n"\
"out vec4 FragColor;\n"\
"void main() {\n"\
"FragColor = texture(Tex, fTex);\n"\
"}\n";


const float ScreenCoords[24] = {
    // Vertex positions                 // Texture positions
    -1.0f, -1.0f,                       0.0f, 0.0f,
    -1.0f, 1.0f,                        0.0f, 1.0f,
    1.0f, 1.0f,                         1.0f, 1.0f,

    -1.0f, -1.0f,                       0.0f, 0.0f,
    1.0f, 1.0f,                         1.0f, 1.0f,
    1.0f, -1.0f,                        1.0f, 0.0f
};


bool check_compile_errors(GLuint Shader, GLenum Type)
{
    int Success;
    char Log[4096];

    glGetShaderiv(Shader, GL_COMPILE_STATUS, &Success);
    if (!Success)
    {
        std::string TypeStr;
        switch (Type)
        {
        case GL_VERTEX_SHADER: TypeStr = "vertex shader"; break;
        case GL_FRAGMENT_SHADER: TypeStr = "fragment shader"; break;
        case GL_COMPUTE_SHADER: TypeStr = "compute shader"; break;
        case GL_PROGRAM: TypeStr = "shader program"; break;
        
        default:
            break;
        }
        glGetShaderInfoLog(Shader, 4096, NULL, Log);
        std::cerr << "Error compiling " << TypeStr << "." << std::endl;
        std::cerr << "==========================================================" << std::endl;
        std::cerr << Log << std::endl;
        std::cerr << "==========================================================" << std::endl;
        return false;
    }
    return true;
}



struct ParamsStruct
{
    int niters = 40;
    int nroots = -3;        // Signum identifies whether to use z^3 - 1 == 0 or three random points
    double angle = M_PI / 2.0;
    double xmax = 1.0;
    double xmin = -1.0;
    double ymax = 1.0;
    double ymin = -1.0;
};

enum FractalType
{
    NEWTON,
    JULIA,
    MANDELBROT,
    INVALID
};



void usage(const char* argv0, std::ostream& Stream)
{
    Stream << "Usage: " << argv0 << " TYPE [ OPTIONS ]" << std::endl;
    Stream << "    TYPE indicates the type of fractal to render. Admissible values are Newton, Mandelbrot and Julia." << std::endl;
    Stream << "    According to the type of fractals, different options are available." << std::endl;
    Stream << "    For Newton\'s fractal the number of roots can be specified. If no option is given, the polynomial" << std::endl;
    Stream << "    used will be z^3 - 1 = 0." << std::endl;
    Stream << "    For Julia\'s set the rotation coefficient can be specified. If nothing is given, pi/2 is assumed." << std::endl;
    Stream << "    For Mandelbrot\'s set no option can be specified." << std::endl;
}


bool istreq(const std::string& s1, const std::string& s2)
{
    if (s1.length() != s2.length())
        return false;
    
    for (int i = 0; i < s1.length(); ++i)
    {
        if (std::tolower(s1[i]) != std::tolower(s2[i]))
            return false;
    }
}


void export_tex(GLuint Texture)
{
    static int CurFrame = 0;
    static void* raw_image = NULL;
    if (raw_image == NULL)
    {
        raw_image = malloc(TEX_SIZE * TEX_SIZE * (4 * sizeof(float) + 3 * sizeof(unsigned char)));
        if (raw_image == NULL)
        {
            std::cerr << "Cannot export images." << std::endl;
            return;
        }
    }
    float* FImage = (float*)raw_image;
    unsigned char* CImage = (unsigned char*)(FImage + TEX_SIZE * TEX_SIZE * 4);
    std::stringstream ss;
    ss << "Screenshot" << std::setfill('0') << std::setw(3) << CurFrame << ".png";
    std::string ExportFile = ss.str();
    glBindTexture(GL_TEXTURE_2D, Texture);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, FImage);
    for (register int i = 0; i < TEX_SIZE; ++i)
    {
        for (register int j = 0; j < TEX_SIZE; ++j)
        {
            for (int register k = 0; k < 3; ++k)
                CImage[(TEX_SIZE - i - 1) * TEX_SIZE * 3 + j * 3 + k] = (unsigned char)(FImage[i * TEX_SIZE * 4 + j * 4 + k] * 255.0f);
        }
    }
    stbi_write_png(ExportFile.c_str(), TEX_SIZE, TEX_SIZE, 3, CImage, 3 * TEX_SIZE);
}


FractalType parse_args(int argc, char const* argv[], ParamsStruct& Params)
{
    if (argc < 2)
    {
        std::cerr << "At least one argument is required." << std::endl;
        usage(argv[0], std::cerr);
        return FractalType::INVALID;
    }

    if (istreq(argv[1], "Newton"))
    {
        if (argc > 2)
        {
            Params.nroots = std::atoi(argv[2]);
            if (Params.nroots < 1)
            {
                std::cerr << "Number of roots must be greater than zero." << std::endl;
                return FractalType::INVALID;
            }
        }
        return FractalType::NEWTON;
    }
    else if (istreq(argv[1], "Julia"))
    {
        if (argc > 2)
            Params.angle = std::atof(argv[2]);
        return FractalType::JULIA;
    }
    else if (istreq(argv[1], "Mandelbrot"))
    {
        Params.xmin = -2.0;
        Params.xmax = 1.0;
        Params.ymin = -1.5;
        Params.ymax = 1.5;
        return FractalType::MANDELBROT;
    }
    
    std::cerr << "Invalid fractal type." << std::endl;
    usage(argv[0], std::cerr);
    return FractalType::INVALID;
}



int main(int argc, char const *argv[])
{
    // Parse arguments
    struct ParamsStruct Params;
    FractalType Type = parse_args(argc, argv, Params);
    if (Type == FractalType::INVALID)
        return -1;


    // Initialize a window
    if (!glfwInit())
    {
        std::cerr << "Cannot initialize GLFW." << std::endl;
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWmonitor* Monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* VideoMode = glfwGetVideoMode(Monitor);
    GLFWwindow* Window = glfwCreateWindow(800, 800, "GPU Fractals", NULL, NULL);
    if (Window == NULL)
    {
        std::cerr << "Cannot initialize a window." << std::endl;
        return -1;
    }
    glfwMakeContextCurrent(Window);
    glfwSetFramebufferSizeCallback(Window, fbcallback);

    // Initialize GLAD
    int GLADStatus = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    if (!GLADStatus)
    {
        std::cerr << "Cannot initialize GLAD." << std::endl;
    }


    // Create the shader program
    GLuint VShader, FShader;
    VShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(VShader, 1, &VSource, NULL);
    glCompileShader(VShader);
    if (!check_compile_errors(VShader, GL_VERTEX_SHADER))
        return -1;
    FShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(FShader, 1, &FSource, NULL);
    glCompileShader(FShader);
    if (!check_compile_errors(FShader, GL_FRAGMENT_SHADER))
        return -1;
    GLuint Shader = glCreateProgram();
    glAttachShader(Shader, VShader);
    glAttachShader(Shader, FShader);
    glLinkProgram(Shader);
    if (!check_compile_errors(Shader, GL_PROGRAM))
        return -1;


    // Create the vertex buffer
    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, 24 * sizeof(float), ScreenCoords, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), NULL);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);


    // Create the texture
    GLuint Tex;
    glGenTextures(1, &Tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, Tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, TEX_SIZE, TEX_SIZE, 0, GL_RGBA, GL_FLOAT, NULL);
    glBindImageTexture(0, Tex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);


    // Compile the compute shader
    std::ifstream Stream;
    if (Type == FractalType::NEWTON)
        Stream.open(NEWTON_SHADER, std::ios::in);
    else if (Type == FractalType::MANDELBROT)
        Stream.open(MANDELBROT_SHADER, std::ios::in);
    else if (Type == FractalType::JULIA)
        Stream.open(JULIA_SHADER, std::ios::in);
    if (!Stream.is_open())
    {
        std::cerr << "Cannot open the compute shader." << std::endl;
        return -1;
    }
    std::stringstream ss;
    ss << Stream.rdbuf();
    std::string CSSource = ss.str();
    const char *CSource = CSSource.c_str();
    GLuint CShader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(CShader, 1, &CSource, NULL);
    glCompileShader(CShader);
    if (!check_compile_errors(CShader, GL_COMPUTE_SHADER))
        return -1;
    GLuint CSProgram = glCreateProgram();
    glAttachShader(CSProgram, CShader);
    glLinkProgram(CSProgram);
    if (!check_compile_errors(CSProgram, GL_PROGRAM))
        return -1;
    Stream.close();


    // Send the compute buffers
    GLuint ParamsBuf;
    glGenBuffers(1, &ParamsBuf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ParamsBuf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(Params), &Params, GL_STATIC_READ);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ParamsBuf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    double* Roots;
    if (Params.nroots == -3)
    {
        Roots = (double*)malloc(6 * sizeof(double));
        Roots[0] = 1.0;             Roots[1] = 0.0;
        Roots[2] = -0.5;            Roots[3] = -0.86603;
        Roots[4] = -0.5;            Roots[5] = 0.86603;
        Params.nroots = 3;
    }
    else
    {
#ifdef ENABLE_REPRODUCIBILITY
        srand(0);
#else
        srand(time(NULL));
#endif
        Roots = (double*)malloc(Params.nroots * 2 * sizeof(double));
        int i;
        for (i = 0; i < Params.nroots; ++i)
        {
            Roots[2 * i] = rand() / (double)RAND_MAX;
            Roots[2 * i + 1] = rand() / (double)RAND_MAX;
            Roots[2 * i] = (Params.xmax - Params.xmin) * Roots[2 * i] + Params.xmin;
            Roots[2 * i + 1] = (Params.ymax - Params.ymin) * Roots[2 * i + 1] + Params.ymin;
        }
    }
    GLuint RootsBuf;
    glGenBuffers(1, &RootsBuf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, RootsBuf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 2 * Params.nroots * sizeof(double), Roots, GL_STATIC_READ);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, RootsBuf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);


    std::cout << "Left click and move the mouse to move the view around." << std::endl;
    std::cout << "Right click and move the mouse to scale along X and/or Y axis." << std::endl;
    std::cout << "Press numnpad +/- to increase/decrease the number of iterations." << std::endl;
    std::cout << "Press E to export the current texture." << std::endl;
    std::cout << "Press ESC to quit the application." << std::endl;


    bool Refresh = true;
    double mouseX, mouseY;
    double oldMouseX, oldMouseY;
    glfwGetCursorPos(Window, &oldMouseX, &oldMouseY);
    while (!glfwWindowShouldClose(Window))
    {
        // Must close?
        if (glfwGetKey(Window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(Window, true);

        glfwGetCursorPos(Window, &mouseX, &mouseY);
        double dx = oldMouseX - mouseX;
        double dy = oldMouseY - mouseY;
        // Movement
        if (glfwGetMouseButton(Window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS)
        {
            Params.xmin += dx * (Params.xmax - Params.xmin) / 60.0;
            Params.xmax += dx * (Params.xmax - Params.xmin) / 60.0;
            Params.ymin += dy * (Params.ymax - Params.ymin) / 60.0;
            Params.ymax += dy * (Params.ymax - Params.ymin) / 60.0;
            Refresh = true;
        }
        // Zoom
        else if (glfwGetMouseButton(Window, GLFW_MOUSE_BUTTON_2) == GLFW_PRESS)
        {
            Params.xmax += dx / 600.0;
            Params.xmin -= dx / 600.0;
            Params.ymax -= dy / 600.0;
            Params.ymin += dy / 600.0;
            Refresh = true;
        }
        oldMouseX = mouseX;
        oldMouseY = mouseY;

        // Increment the number of iterations
        if (glfwGetKey(Window, GLFW_KEY_KP_ADD) == GLFW_PRESS)
        {
            if (glfwGetKey(Window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                glfwGetKey(Window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)
                Params.niters += 10;
            else
                Params.niters += 1;
            Refresh = true;
        }
        else if (glfwGetKey(Window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS)
        {
            if (glfwGetKey(Window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                glfwGetKey(Window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)
                Params.niters -= 10;
            else
                Params.niters -= 1;
            Refresh = true;
        }

        // Export
        if (glfwGetKey(Window, GLFW_KEY_E) == GLFW_PRESS)
            export_tex(Tex);



        // Run the compute shader, if needed
        if (Refresh)
        {
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, ParamsBuf);
            glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(Params), &Params, GL_STATIC_READ);
            glUseProgram(CSProgram);
            glDispatchCompute((TEX_SIZE + 31) / 32, (TEX_SIZE + 31) / 32, 1);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
            Refresh = false;
        }

        // Clear the window
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

        // Draw the texture
        glUseProgram(Shader);
        glBindTexture(GL_TEXTURE_2D, Tex);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);


        glfwSwapBuffers(Window);
        glfwPollEvents();
    }


    // Free memory
    free(Roots);
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &ParamsBuf);
    glDeleteBuffers(1, &RootsBuf);
    glDeleteProgram(Shader);
    glDeleteProgram(CSProgram);
    glDeleteTextures(1, &Tex);


    // Close GLFW
    glfwTerminate();

    return 0;
}