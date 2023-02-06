#include <iostream>

#include <emscripten/html5.h>
#include <emscripten/em_math.h>
#include <webgl/webgl2.h>

#include <algorithm>
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
// line ~25
#define GAME_WIDTH 569
#define GAME_HEIGHT 388
#define STREET_HEIGHT 160

struct Image
{
    const char *url;
    GLuint glTexture;
    int width, height;
};

Image images[] =
{
    {},
    { "title.png" },
    { "scorebar.png" },
    { "road.png" },
    { "batman.png" },
    { "life.png" },
    { "car1.png" },
    { "car2.png" },
    { "car3.png" },
    { "car4.png" },
    { "car5.png" },
    { "car6.png" },
    { "car7.png" },
    { "car8.png" }
};
// In the same order as images array
enum
{
    IMG_TEXT = 0,
    IMG_TITLE,
    IMG_SCOREBAR,
    IMG_ROAD,
    IMG_BATMAN,
    IMG_LIFE,
    IMG_CAR1, // ..., IMG_CAR8
};

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

extern "C" void load_image(GLuint glTexture, const char*url, int *width, int *height);

GLuint create_texture()
{
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tex;
}

void draw_image(GLuint glTexture, float x, float y, float width, float height, float r=1.f, float g=1.f, float b=1.f, float a=1.f)
{
    const float pixelWidth = 2.f / GAME_WIDTH;
    const float pixelHeight = 2.f / GAME_HEIGHT;
    float spriteMatrix[16] = {
        width*pixelWidth, 0, 0, 0,
        0, height*pixelHeight, 0, 0,
        0, 0, 1, 0,
        (int)x*pixelWidth-1.f, (int)y*pixelHeight-1.f, 0, 1
    };

    glUniformMatrix4fv(matrixPosition, 1, 0, spriteMatrix);
    glUniform4f(colorPosition, r, g, b, a);
    glBindTexture(GL_TEXTURE_2D, glTexture);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

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

 GLuint testImage;
int testImageWidth, testImageHeight;

EM_BOOL game_tick(double t, void *)
{
    glClearColor(0.f, emscripten_math_sin(t/500.0), 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    draw_image(testImage, 0, 0, testImageWidth, testImageHeight);
    return EM_TRUE; // true == continue the loop
}
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/

int main() // line 445
{
    init_webgl();
    emscripten_request_animation_frame_loop(&game_tick, 0);

    testImage = create_texture();
    load_image(testImage, "title.png", &testImageWidth, &testImageHeight);

    for(auto &i : images)
    {
        i.glTexture = create_texture();
        if (i.url)
            load_image(i.glTexture, i.url, &i.width, &i.height);
    }
    // ...
} // line 461
