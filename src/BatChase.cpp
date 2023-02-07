// cspell:disable
#include <iostream>

#include <emscripten/html5.h>
#include <emscripten/em_math.h>
#include <emscripten/dom_pk_codes.h>
#include <webgl/webgl2.h>

#include <algorithm>
#include <vector>

uint8_t keysOld[0x10000] = {}, keysNow[0x10000] = {};

EM_BOOL key_handler(int eventType, const EmscriptenKeyboardEvent* keyEvent, void* userData)
{
    uint16_t code = (uint16_t)emscripten_compute_dom_pk_code(keyEvent->code);
    keysNow[code] = (eventType == EMSCRIPTEN_EVENT_KEYDOWN) ? 1 : 0;
    return EM_FALSE; // don't suppress the key event
}

bool is_key_pressed(DOM_PK_CODE_TYPE code) { return keysNow[code] && !keysOld[code]; }

bool is_key_down(DOM_PK_CODE_TYPE code) { return keysNow[code]; }

#define GAME_WIDTH 569
#define GAME_HEIGHT 388
#define STREET_HEIGHT 160

struct Image
{
    const char* url{};
    GLuint glTexture{};
    int width{};
    int height{};
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
    IMG_CAR1,
    IMG_CAR2,
    IMG_CAR3,
    IMG_CAR4,
    IMG_CAR5,
    IMG_CAR6,
    IMG_CAR7,
    IMG_CAR8,
    IMG_NUMELEMS
};
std::array<Image, IMG_NUMELEMS> images{{
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
}};
static_assert(IMG_NUMELEMS == images.size());

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

enum Tag
{
    TAG_NONE = 0,
    TAG_PLAYER,
    TAG_ENEMY,
    TAG_ROAD,
    TAG_LIFE1,
    TAG_LIFE2,
    TAG_LIFE3,
    TAG_SCORE,
    TAG_HIGH_SCORE,
    TAG_MINUTES,
    TAG_SECONDS
};

struct Object
{
    float x, y;
    int img;
    Tag tag;
    // Jos img == IMG_TEXT:
    char text[64];
    float r,g,b,a;
    int fontId, fontSize, spacing;
    // Jos img != IMG_TEXT:
    float mass, velx, vely;
};

std::vector<Object> scene;

int find_sprite_index(Tag tag)
{
    for(size_t i = 0; i < scene.size(); ++i)
        if (scene[i].tag == tag) return i;
    return -1;
}

Object* find_sprite(Tag tag)
{
    int i = find_sprite_index(tag);
    return (i >= 0) ? &scene[i] : 0;
}

void remove_sprite_at_index(int i)
{
    scene.erase(scene.begin() + i);
}

void remove_sprite(Tag tag)
{
    int i = find_sprite_index(tag);
    if (i >= 0)
        remove_sprite_at_index(i);
}
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/

extern "C" void load_image(GLuint glTexture, const char* url, int* width, int* height);

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

// test code: test image
//GLuint testImage;
//int testImageWidth, testImageHeight;

void (*currentRoom)(float t, float dt) = nullptr;

EM_BOOL game_tick(double t, void *)
{
    // test code: animated colour
    // glClearColor(0.f, emscripten_math_sin(t/500.0), 0.f, 1.f);
    // glClear(GL_COLOR_BUFFER_BIT);

    // test code: test image
    // draw_image(testImage, 0, 0, testImageWidth, testImageHeight);

    static double prevT;
    float dt = std::min(50.f, (float)(t - prevT));
    prevT = t;

    if (currentRoom)
        currentRoom(t, dt);

    for(auto& o : scene)
    {
        if (o.img != IMG_TEXT)
            draw_image(images[o.img].glTexture, o.x, o.y, images[o.img].width, images[o.img].height);
        // ...
    }

    memcpy(keysOld, keysNow, sizeof(keysOld));
    return EM_TRUE; // true == continue the loop
}
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
void enter_game();

void update_title(float t, float dt)
{
    if (is_key_pressed(DOM_PK_ENTER) || is_key_pressed(DOM_PK_SPACE))
        enter_game();
}

// [min, max[
float rnd(float min, float max) { return min + (float)emscripten_math_random() * (max - min); }
int rnd(int min, int max) { return min + (int)(emscripten_math_random() * (max - min)); }

// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
float sign(float x) { return x > 0.f ? 1.f : (x < 0.f ? -1.f : 0.f); }

float spawnTimer, score;

void update_game(float t, float dt) // line 301 
{
    auto* player = find_sprite(TAG_PLAYER);

    // slow down (friction)
    player->vely -= sign(player->vely) * std::min(std::fabsf(player->vely), 0.004f * dt);

    // increase speed
    if (is_key_down(DOM_PK_ARROW_UP))
        player->vely += dt * 0.008f;
    if (is_key_down(DOM_PK_ARROW_DOWN))
        player->vely -= dt * 0.008f;
    if (is_key_down(DOM_PK_ARROW_LEFT))
        player->velx -= dt * 0.003f;
    if (is_key_down(DOM_PK_ARROW_RIGHT))
        player->velx += dt * 0.001f;

    // clamp speed
    player->velx = std::clamp(player->velx, 0.f, 0.55f);
    player->vely = std::clamp(player->vely, -0.3f, 0.3f);

    // limit Y within the game area
    player->y = std::clamp(player->y + player->vely * dt, 0.f, float(STREET_HEIGHT));

    // Camera trick: the player's X speed moves all games objects to the left.
    // Also wrap background pictures infinitely
    for(Object &o : scene) {
        if (o.tag == TAG_ROAD || o.tag == TAG_ENEMY)
            o.x -= player->velx * dt;
        if (o.tag == TAG_ROAD && o.x < -images[IMG_ROAD].width)
            o.x += 2*images[IMG_ROAD].width;
    }

    // spawn enemy cars
    spawnTimer -= 2.f * player->velx * dt;
    if (spawnTimer < 0.f && scene.size() < 15 + score / 10000) {
        spawnTimer = rnd(0.f, std::min(2500.f, 25.f + 22000000.f / score));
        scene.push_back(
        {
            .x = GAME_WIDTH * 1.5f, .y = rnd(0.f, float(STREET_HEIGHT)),
            .img = IMG_CAR1 + rnd(0, 8), .tag = TAG_ENEMY,.mass = 1.f,
            .velx = rnd(0.15f, 0.45f), .vely = rnd(-0.07f, 0.07f)
        });
    }

    // move enemies
    for(size_t i = 0; i < scene.size(); ++i)
    {
        auto& obj = scene[i];
        if (obj.tag != TAG_ENEMY)
            continue;
        // move fw
        obj.x += obj.velx * dt;
        // move vertically, say within the street
        obj.y = std::clamp(obj.y + obj.vely * dt, 0.f, float(STREET_HEIGHT));
        // mirror Y speed if car collides to curb
        if ((obj.y <= 0 && obj.vely < 0) || (obj.y >= STREET_HEIGHT && obj.vely > 0))
            obj.vely = -obj.vely;
        // remove cars that go out of the screen
        if (std::fabs(obj.x) > 2*GAME_WIDTH)
            remove_sprite_at_index(i--);
    }

    // …
} // line 411 

void enter_title()
{
    scene.clear();
    scene.push_back({ .x=0.f, .y=0.f, .img = IMG_TITLE });
    currentRoom = update_title;
}
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/

void enter_game()
{
    scene.clear();
    scene.push_back({ .x=0.f, .y=0.f, .img=IMG_ROAD, .tag=TAG_ROAD });
    scene.push_back({ .x=4096.f, .y=0.f, .img=IMG_ROAD, .tag=TAG_ROAD });
    scene.push_back({ .x=100.f, .y=120.f, .img=IMG_BATMAN, .tag=TAG_PLAYER, .mass=0.05f, .velx=0.05f });
    scene.push_back({ .x=0.f, .y=314.f, .img=IMG_SCOREBAR });
    scene.push_back({ .x=380.f, .y=330.f, .img=IMG_LIFE, .tag=TAG_LIFE1 });
    scene.push_back({ .x=440.f, .y=330.f, .img=IMG_LIFE, .tag=TAG_LIFE2 });
    scene.push_back({ .x=500.f, .y=330.f, .img=IMG_LIFE, .tag=TAG_LIFE3 });

    currentRoom = update_game;

    spawnTimer = 0.f;
    score = 0.f;
    // … 
}

int main() // line 445
{
    init_webgl();
    emscripten_request_animation_frame_loop(&game_tick, 0);

    // testImage = create_texture();
    // load_image(testImage, "title.png", &testImageWidth, &testImageHeight);

    for(auto& img : images)
    {
        img.glTexture = create_texture();
        if (img.url)
            load_image(img.glTexture, img.url, &img.width, &img.height);
    }

    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, 0, 0, key_handler);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, 0, 0, key_handler);

    enter_title();
} // line 461
