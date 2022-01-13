/**
 * @file        gpu_fractals_interactive.cpp
 * 
 * @brief       An application for interactively exploration of fractals.
 * 
 * @author      Filippo Maggioli\n 
 *              (maggioli@di.uniroma1.it, maggioli.filippo@gmail.com)\n 
 *              Sapienza, University of Rome - Department of Computer Science
 * @date        2021-10-25
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
#include <chrono>
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
"out vec2 fPos;\n"\
"void main() {\n"\
"gl_Position = vec4(aPos, 0.0f, 1.0f);\n"\
"fPos = aPos / 2.0f + 0.5f;\n"\
"}\n";


const float ScreenCoords[12] = {
    // Vertex positions 
    -1.0f,  -1.0f,
    -1.0f,   1.0f,
     1.0f,   1.0f,

    -1.0f,  -1.0f,
     1.0f,   1.0f,
     1.0f,  -1.0f,
};


// Number of milliseconds between user actions
const unsigned long long ActionDelay = 250;


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
    int niters      = 40;
    int nroots      = 3;
    double angle    = M_PI / 2.0;
    double xlim[2]  = { -1.0, 1.0 };
    double ylim[2]  = { -1.0, 1.0 };
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
    ss << "Screenshot" << std::setfill('0') << std::setw(3) << CurFrame++ << ".png";
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
        Params.xlim[0] = -2.0;
        Params.xlim[1] = 1.0;
        Params.ylim[0] = -1.5;
        Params.ylim[1] = 1.5;
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
    // Vertex shader is the same for all
    VShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(VShader, 1, &VSource, NULL);
    glCompileShader(VShader);
    if (!check_compile_errors(VShader, GL_VERTEX_SHADER))
        return -1;
    // Fragment shader depends on which fractal
    std::ifstream Stream;
    if (Type == FractalType::NEWTON)
        Stream.open(NEWTON_FRAGMENT_SHADER, std::ios::in);
    else if (Type == FractalType::MANDELBROT)
        Stream.open(MANDELBROT_FRAGMENT_SHADER, std::ios::in);
    else if (Type == FractalType::JULIA)
        Stream.open(JULIA_FRAGMENT_SHADER, std::ios::in);
    if (!Stream.is_open())
    {
        std::cerr << "Cannot open the compute shader." << std::endl;
        return -1;
    }
    std::stringstream ss;
    ss << Stream.rdbuf();
    Stream.close();
    std::string FSSource = ss.str();
    const char *FSource = FSSource.c_str();
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
    glBufferData(GL_ARRAY_BUFFER, 12 * sizeof(float), ScreenCoords, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), NULL);
    glEnableVertexAttribArray(0);
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
    if (Type == FractalType::NEWTON)
        Stream.open(NEWTON_COMPUTE_SHADER, std::ios::in);
    else if (Type == FractalType::MANDELBROT)
        Stream.open(MANDELBROT_COMPUTE_SHADER, std::ios::in);
    else if (Type == FractalType::JULIA)
        Stream.open(JULIA_COMPUTE_SHADER, std::ios::in);
    if (!Stream.is_open())
    {
        std::cerr << "Cannot open the compute shader." << std::endl;
        return -1;
    }
    ss = std::stringstream();
    ss << Stream.rdbuf();
    Stream.close();
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


    // Send the compute buffers
    GLuint ParamsBuf;
    glGenBuffers(1, &ParamsBuf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ParamsBuf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(Params), &Params, GL_STATIC_READ);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ParamsBuf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    double* Roots = NULL;
    GLuint RootsBuf = 0;
    
    if (Type == FractalType::NEWTON)
    {
        Roots = (double*)malloc(Params.nroots * 2 * sizeof(double));
        int i;
        for (i = 0; i < Params.nroots; ++i)
        {
            double theta = 2.0 * M_PI * ((double)i / (double)Params.nroots);
            Roots[2 * i] = cos(theta);
            Roots[2 * i + 1] = sin(theta);
        }
        glGenBuffers(1, &RootsBuf);
        glBindBuffer(GL_UNIFORM_BUFFER, RootsBuf);
        glBufferData(GL_UNIFORM_BUFFER, 2 * Params.nroots * sizeof(double), Roots, GL_STATIC_READ);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, RootsBuf);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }


    std::cout << "Left click and move the mouse to move the view around." << std::endl;
    std::cout << "Right click and move vertically the mouse to scale the view." << std::endl;
    std::cout << "Press numpad +/- to increase/decrease the number of iterations by 1." << std::endl;
    std::cout << "Press shift + numpad +/- to increase/decrease the number of iterations by 10." << std::endl;
    std::cout << "Press E to export the view." << std::endl;
    std::cout << "Press ESC to quit the application." << std::endl;


    double mouseX, mouseY;
    double oldMouseX, oldMouseY;
    glfwGetCursorPos(Window, &oldMouseX, &oldMouseY);
    std::chrono::system_clock::time_point LastAction = std::chrono::system_clock::from_time_t(0);
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
            double SurfArea = (Params.xlim[1] - Params.xlim[0]) * (Params.ylim[1] - Params.ylim[0]);
            double UnitLength = sqrt(SurfArea);
            Params.xlim[0] += dx * UnitLength / 600.0;
            Params.xlim[1] += dx * UnitLength / 600.0;
            Params.ylim[0] -= dy * UnitLength / 600.0;
            Params.ylim[1] -= dy * UnitLength / 600.0;
        }
        // Zoom
        else if (glfwGetMouseButton(Window, GLFW_MOUSE_BUTTON_2) == GLFW_PRESS)
        {
            double SurfArea = (Params.xlim[1] - Params.xlim[0]) * (Params.ylim[1] - Params.ylim[0]);
            double UnitLength = sqrt(SurfArea);
            Params.xlim[1] += dy * UnitLength / 600.0;
            Params.xlim[0] -= dy * UnitLength / 600.0;
            Params.ylim[0] -= dy * UnitLength / 600.0;
            Params.ylim[1] += dy * UnitLength / 600.0;
        }
        oldMouseX = mouseX;
        oldMouseY = mouseY;

        std::chrono::system_clock::time_point Now = std::chrono::system_clock::now();
        unsigned long long Delay = std::chrono::duration_cast<std::chrono::milliseconds>(Now - LastAction).count();
        if (Delay >= ActionDelay)
        {
            // Increment the number of iterations
            if (glfwGetKey(Window, GLFW_KEY_KP_ADD) == GLFW_PRESS)
            {
                if (glfwGetKey(Window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                    glfwGetKey(Window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)
                    Params.niters += 10;
                else
                    Params.niters += 1;
            }
            else if (glfwGetKey(Window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS)
            {
                if (glfwGetKey(Window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                    glfwGetKey(Window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)
                    Params.niters -= 10;
                else
                    Params.niters -= 1;
                if (Params.niters < 0)
                    Params.niters = 0;
            }

            // Export
            if (glfwGetKey(Window, GLFW_KEY_E) == GLFW_PRESS)
            {
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, ParamsBuf);
                glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(Params), &Params, GL_STATIC_READ);
                glUseProgram(CSProgram);
                glDispatchCompute((TEX_SIZE + 31) / 32, (TEX_SIZE + 31) / 32, 1);
                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

                export_tex(Tex);
            }
            LastAction = Now;
        }

        // Clear the window
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

        // Use the shader and send the data
        glUseProgram(Shader);
        glUniform1i(glGetUniformLocation(Shader, "NumIters"), Params.niters);
        glUniform2dv(glGetUniformLocation(Shader, "XLim"), 1, Params.xlim);
        glUniform2dv(glGetUniformLocation(Shader, "YLim"), 1, Params.ylim);
        if (Type == FractalType::JULIA)
            glUniform1d(glGetUniformLocation(Shader, "Angle"), Params.angle);
        else if (Type == FractalType::NEWTON)
        {
            glUniform1i(glGetUniformLocation(Shader, "NumRoots"), Params.nroots);
            glUniformBlockBinding(Shader, glGetUniformBlockIndex(Shader, "RootsBuf"), 2);
            glBindBufferBase(GL_UNIFORM_BUFFER, 2, RootsBuf);
        }


        // Draw
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);


        glfwSwapBuffers(Window);
        glfwPollEvents();
    }


    // Free memory
    if (Roots != NULL)
        free(Roots);
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &ParamsBuf);
    if (RootsBuf > 0)
        glDeleteBuffers(1, &RootsBuf);
    glDeleteProgram(Shader);


    // Close GLFW
    glfwTerminate();

    return 0;
}