#include <iostream>

#include <emscripten/html5.h>
#include <webgl/webgl2.h>
 #include <emscripten/em_math.h>

#include <algorithm>
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
// line ~25
#define GAME_WIDTH 569
#define GAME_HEIGHT 388
#define STREET_HEIGHT 160

GLuint compile_shader(GLenum type, const char* src)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    return shader;
}

GLuint create_program(GLuint vertexShader, GLuint fragmentShader)
{
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glBindAttribLocation(program, 0, "pos");
    glLinkProgram(program);
    glUseProgram(program);
    return program;
}

GLuint vertexBuffer, matrixPosition, colorPosition;

// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/

// line ~201
void init_webgl()
{
    EM_ASM(document.body.style = 'margin: 0px; overflow: hidden; background: #787878;');
    EM_ASM(document.querySelector('canvas').style['imageRendering'] = 'pixelated');
    double scale = std::min(
        EM_ASM_DOUBLE(return window.innerWidth) / GAME_WIDTH,
        EM_ASM_DOUBLE(return window.innerHeight) / GAME_HEIGHT
    );
    emscripten_set_element_css_size("canvas", scale * GAME_WIDTH, scale * GAME_HEIGHT);
    emscripten_set_canvas_element_size("canvas", GAME_WIDTH, GAME_HEIGHT);

    EmscriptenWebGLContextAttributes attrs;
    emscripten_webgl_init_context_attributes(&attrs);
    attrs.alpha = 0;
    attrs.majorVersion = 2;
    emscripten_webgl_make_context_current(
        emscripten_webgl_create_context("canvas", &attrs)
    );

    static const char vertex_shader[] =
    R"(
        attribute vec4 pos;
        varying vec2 uv;
        uniform mat4 mat;
        void main()
        {
            uv = pos.xy;
            gl_Position= mat * pos;
        }
    )";

    GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_shader);

    static const char fragment_shader[] =
    R"(
        precision lowp float;
        uniform sampler2D spriteTexture;
        uniform vec4 constantColor;
        varying vec2 uv;
        void main()
        {
            gl_FragColor = constantColor * texture2D(spriteTexture, uv);
        }
    )";

    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_shader);
    GLuint program = create_program(vs, fs);
    matrixPosition = glGetUniformLocation(program, "mat");
    colorPosition = glGetUniformLocation(program, "constantColor");
    // alpha blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // geom. buffer
    glGenBuffers(1, &vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    const float pos[] = { 0, 0, 1, 0, 0, 1, 1, 1 };
    glBufferData(GL_ARRAY_BUFFER, sizeof(pos), pos, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
} // line 244

EM_BOOL game_tick(double t, void *)
{
    glClearColor(emscripten_math_sin(t/500.0), 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    return EM_TRUE; // Jatka peliloopin ajoa
}
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/

// line ~445
int main()
{
    init_webgl();
    emscripten_request_animation_frame_loop(&game_tick, 0);
}
