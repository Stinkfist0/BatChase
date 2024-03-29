// cspell:disable
#include <emscripten/html5.h>
#include <emscripten/em_math.h>
#include <emscripten/dom_pk_codes.h>
#include <webgl/webgl2.h>

#include <algorithm>
#include <vector>
#include <array>
#include <map>
#include <cstdio>
#include <functional>

constexpr int GAME_WIDTH = 569;
constexpr int GAME_HEIGHT = 388;
constexpr int STREET_HEIGHT = 160;

// these functions are implemented in the JS library file
extern "C"
{
void load_image(GLuint glTexture, const char* url, int* width, int* height);
void load_font(int fontId, const char* url);
bool upload_unicode_char_to_texture(int id, int ch, int size);
void preload_audio(int audioId, const char* url);
void play_audio(int audioId, EM_BOOL loop);
}

// [min, max[
template <typename T, typename std::enable_if<std::is_arithmetic<T>::value>::type* = nullptr>
T random(T min, T max) { return min + T(emscripten_math_random() * (max - min)); }

float sign(float x) { return x > 0.f ? 1.f : (x < 0.f ? -1.f : 0.f); }

struct Image
{
    const char* url{};
    GLuint glTexture{};
    int width{};
    int height{};
};

// In the same order as images array
enum ImageId
{
    IMG_TEXT = 0,
    IMG_TITLE,
    IMG_ENDSCREEN,
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
    IMG_NUM_ELEMS,
    IMG_NUM_CARS = IMG_CAR8 - IMG_CAR1 + 1,
};
std::array<Image, IMG_NUM_ELEMS> images{{
    {},
    { "title.png" },
    { "endscreen.png" },
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
static_assert(IMG_NUM_ELEMS == images.size());

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

constexpr int MaxTextLength = 64;

struct Object
{
    float x{}, y{};
    ImageId img{};
    Tag tag{};
    // if img == IMG_TEXT
    char text[MaxTextLength];
    float r{}, g{}, b{}, a{};
    int fontId{}, fontSize{}, spacing{};
    // else
    float mass{}, velx{}, vely{};
};

std::vector<Object> scene;

int find_sprite_index(Tag tag)
{
    for(size_t i = 0; i < scene.size(); ++i)
        if (scene[i].tag == tag)
            return i;
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

enum AudioId
{
    AUDIO_BG_MUSIC,
    AUDIO_COLLISION1,
    AUDIO_COLLISION2,
    AUDIO_COLLISION3,
    AUDIO_COLLISION4,
    AUDIO_COLLISION5,
    AUDIO_COLLISION6,
    AUDIO_COLLISION7,
    AUDIO_COLLISION8,
    AUDIO_NUM_COLLISIONS = AUDIO_COLLISION8 - AUDIO_COLLISION1 + 1,
    AUDIO_NUMELEMS
};

std::array<const char *, AUDIO_NUMELEMS> audioUrls{
{
    "batman.mp3",
    "c1.wav",
    "c2.wav",
    "c3.wav",
    "c4.wav",
    "c5.wav",
    "c6.wav",
    "c7.wav",
    "c8.wav" 
}};

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

void draw_image(GLuint glTexture, float x, float y, float width, float height, float r = 1.f, float g = 1.f, float b = 1.f, float a = 1.f)
{
    const float pixelWidth = 2.f / GAME_WIDTH;
    const float pixelHeight = 2.f / GAME_HEIGHT;
    float spriteMatrix[16] = {
        width * pixelWidth, 0, 0, 0,
        0, height * pixelHeight, 0, 0,
        0, 0, 1, 0,
        (int)x * pixelWidth - 1.f, (int)y * pixelHeight - 1.f, 0, 1};

    glUniformMatrix4fv(matrixPosition, 1, 0, spriteMatrix);
    glUniform4f(colorPosition, r, g, b, a);
    glBindTexture(GL_TEXTURE_2D, glTexture);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

enum { FONT_C64 = 0 };

// (fontId, unicodeChar, size) -> GL texture
std::map<std::tuple<int, int, int>, Image> glyphs;

Image* find_or_cache_font_char(int fontId, int unicodeChar, int size)
{
    auto t = std::make_tuple(fontId, unicodeChar, size);
    auto iter = glyphs.find(t);
    if (iter != glyphs.end())
        return &iter->second;

    Image i = { .glTexture = create_texture(), .width = size, .height = size };
    if (upload_unicode_char_to_texture(fontId, unicodeChar, size))
    {
        glyphs[t] = i;
        return &glyphs[t];
    }

    return 0;
}

void draw_text(
    float x, float y, float r, float g, float b, float a,
    const char *str, int fontId, int size, float spacing)
{
    for(; *str; ++str)
    {
        Image *i = find_or_cache_font_char(fontId, *str, size);
        if (i)
            draw_image(i->glTexture, x, y, i->width, i->height, r, g, b, a);
        x += spacing;
    }
}

void ResizeCanvas(double windowInnerWidth, double windowInnerHeight)
{
    double scale = std::min(windowInnerWidth / GAME_WIDTH,  windowInnerHeight / GAME_HEIGHT);
    emscripten_set_element_css_size("canvas", scale * GAME_WIDTH, scale * GAME_HEIGHT);
    emscripten_set_canvas_element_size("canvas", GAME_WIDTH, GAME_HEIGHT);
}

EM_BOOL ResizeHandler(int /* eventType */, const EmscriptenUiEvent* uiEvent, void* /* userData */)
{
    ResizeCanvas(uiEvent->windowInnerWidth, uiEvent->windowInnerHeight);
    return EM_FALSE;
}

void init_webgl()
{
    EM_ASM(document.body.style = 'margin: 0px; overflow: hidden; background: #787878;');
    EM_ASM(document.querySelector('canvas').style['imageRendering'] = 'pixelated');

    ResizeCanvas(EM_ASM_DOUBLE(return window.innerWidth), EM_ASM_DOUBLE(return window.innerHeight));

    EmscriptenWebGLContextAttributes attrs;
    emscripten_webgl_init_context_attributes(&attrs);
    attrs.alpha = EM_FALSE;
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
}

// test code: test image
//GLuint testImage;
//int testImageWidth, testImageHeight;

// Get center point of a sprite
void get_center_pos(const Object& o, float &cx, float &cy)
{
    cx = o.x + images[o.img].width / 2.f;
    cy = o.y + images[o.img].height / 2.f;
}
// Laskee kahden spriten X- ja Y-leikkauksen
void get_overlap_amount(const Object& a, const Object& b, float& x, float& y)
{
    x = std::min(a.x + images[a.img].width - b.x, b.x + images[b.img].width - a.x);
    y = std::min(a.y + images[a.img].height - b.y, b.y + images[b.img].height - a.y);
}

float lastHitTime;
int lives;
float spawnTimer, score;
float gameStartTime, highscore = 5000;
std::function<void(float /*t*/, float /*dt*/)> currentRoom;

uint8_t keysOld[0x10000], keysNow[0x10000];
bool touchInput, touchDown, touchStarted;

EM_BOOL game_tick(double t, void * /* userData */)
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

    for(auto& obj : scene)
    {
        if (obj.img == IMG_TEXT)
        {
            draw_text(obj.x, obj.y, obj.r, obj.g, obj.b, obj.a, obj.text, obj.fontId, obj.fontSize, obj.spacing);
        }
        else
        {
            const auto& img = images[obj.img];
            draw_image(img.glTexture, obj.x, obj.y, img.width, img.height);
        }
    }

    touchStarted = false; // TODO temp. hack
    memcpy(keysOld, keysNow, sizeof(keysOld));
    return EM_TRUE; // continue the loop
}

EM_BOOL KeyHandler(int eventType, const EmscriptenKeyboardEvent* keyEvent, void* /* userData */)
{
    uint16_t code = (uint16_t)emscripten_compute_dom_pk_code(keyEvent->code);
    keysNow[code] = (eventType == EMSCRIPTEN_EVENT_KEYDOWN) ? 1 : 0;
    return EM_FALSE; // don't suppress the key event
}

bool is_key_pressed(DOM_PK_CODE_TYPE code) { return keysNow[code] && !keysOld[code]; }

bool is_key_down(DOM_PK_CODE_TYPE code) { return keysNow[code]; }

void EnterTitle();
void EnterGame();
void EnterEndScreen();

EM_BOOL TouchHandler(int eventType, const EmscriptenTouchEvent* /* touchEvent */, void* /* userData */)
{
    touchInput = true;
    touchStarted = (eventType == EMSCRIPTEN_EVENT_TOUCHSTART);
    switch (eventType)
    {
    case EMSCRIPTEN_EVENT_TOUCHSTART:
    case EMSCRIPTEN_EVENT_TOUCHMOVE:
        touchDown = true;
        break;
    case EMSCRIPTEN_EVENT_TOUCHEND:
    case EMSCRIPTEN_EVENT_TOUCHCANCEL:
        touchDown = false;
        break;
    }

    return EM_FALSE;
}

void update_title(float /* t */, float /* dt */)
{
    if (touchStarted || is_key_pressed(DOM_PK_ENTER) || is_key_pressed(DOM_PK_SPACE))
        EnterGame();
}

void UpdateEndSreen(float /* t */, float /* dt */)
{
    if (touchStarted || is_key_pressed(DOM_PK_ENTER) || is_key_pressed(DOM_PK_SPACE))
        EnterTitle();
}

void update_game(float t, float dt)
{
    auto* player = find_sprite(TAG_PLAYER);

    // slow down (friction)
    player->vely -= sign(player->vely) * std::min(std::fabsf(player->vely), 0.004f * dt);

    // increase speed
    if (is_key_down(DOM_PK_ARROW_UP))
        player->vely += dt * 0.008f;
    if (is_key_down(DOM_PK_ARROW_DOWN))
        player->vely -= dt * 0.008f;
    if (is_key_down(DOM_PK_ARROW_LEFT) || (touchInput && !touchDown))
        player->velx -= dt * 0.003f;
    if (is_key_down(DOM_PK_ARROW_RIGHT) || (touchInput && touchDown))
        player->velx += dt * 0.001f;

    // clamp speed
    player->velx = std::clamp(player->velx, 0.f, 0.55f);
    player->vely = std::clamp(player->vely, -0.3f, 0.3f);

    // limit Y within the game area
    player->y = std::clamp(player->y + player->vely * dt, 0.f, float(STREET_HEIGHT));

    // Camera trick: the player's X speed moves all games objects to the left.
    // Also wrap background pictures infinitely
    for(Object& o : scene)
    {
        if (o.tag == TAG_ROAD || o.tag == TAG_ENEMY)
            o.x -= player->velx * dt;
        if (o.tag == TAG_ROAD && o.x < -images[IMG_ROAD].width)
            o.x += 2 * images[IMG_ROAD].width;
    }

    // spawn enemy cars
    spawnTimer -= 2.f * player->velx * dt;
    if (spawnTimer < 0.f && scene.size() < 15 + score / 10000)
    {
        spawnTimer = random(0.f, std::min(2500.f, 25.f + 22000000.f / score));
        auto randomCarImg = (ImageId)(IMG_CAR1 + random(0, (int)IMG_NUM_CARS));
        scene.push_back(
        {
            .x = GAME_WIDTH * 1.5f, .y = random(0.f, float(STREET_HEIGHT)),
            .img =  randomCarImg, .tag = TAG_ENEMY,.mass = 1.f,
            .velx = random(0.15f, 0.45f), .vely = random(-0.07f, 0.07f)
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
        if (std::fabs(obj.x) > 2 * GAME_WIDTH)
            remove_sprite_at_index(i--);
    }

    bool player_collided = false;
    for (size_t i = 1; i < scene.size(); ++i)
    {
        if (scene[i].tag != TAG_ENEMY && scene[i].tag != TAG_PLAYER)
            continue;

        for (size_t j = 0; j < i; ++j)
        {
            if (scene[j].tag != TAG_ENEMY && scene[j].tag != TAG_PLAYER)
                continue;
            Object &a = scene[i], &b = scene[j];
            float a_cx, a_cy, b_cx, b_cy, x_overlap, y_overlap;
            get_overlap_amount(a, b, x_overlap, y_overlap);
            if (x_overlap > 0.f && y_overlap > 0.f)
            {
                // SAT
                get_center_pos(a, a_cx, a_cy);
                get_center_pos(b, b_cx, b_cy);
                float xdir = sign(b_cx - a_cx) * x_overlap * 0.5f;
                float ydir = sign(b_cy - a_cy) * y_overlap * 0.5f;
                float xveldiff = 2.f * (b.velx - a.velx) / (a.mass + b.mass);
                float yveldiff = 2.f * (b.vely - a.vely) / (a.mass + b.mass);
                if (x_overlap <= y_overlap) // X-suuntainen
                {
                    a.x -= xdir;
                    b.x += xdir; // Erota autot X
                    if (xdir * xveldiff <= 0.f)
                    {
                        a.velx += b.mass * xveldiff;
                        b.velx -= a.mass * xveldiff;
                        if (a.tag == TAG_PLAYER || b.tag == TAG_PLAYER)
                            player_collided = true;
                    }
                }
                else // Y-akselin suuntainen törmäys
                {
                    a.y -= ydir;
                    b.y += ydir; // Erota autot Y
                    if (ydir * yveldiff <= 0.f)
                    {
                        a.vely += b.mass * yveldiff;
                        b.vely -= a.mass * yveldiff;
                        if (a.tag == TAG_PLAYER || b.tag == TAG_PLAYER)
                            player_collided = true;
                    }
                }
            }
        }
    }

    if (player_collided)
    {
        play_audio(AUDIO_COLLISION1 + random(0, (int)AUDIO_NUM_COLLISIONS), EM_FALSE);
        if (t - lastHitTime > 500)
        {
            lastHitTime = t;
            remove_sprite((Tag)(TAG_LIFE1 + --lives));
            if (lives <= 0)
            {
                EnterEndScreen();
                return;
            }
        }
    }

    // päivitä pelaajan pisteet ja piste-ennätys
    score += player->velx * dt;
    highscore = std::max(score, highscore);

    // kirjoita uusi pistetilanne merkkijonoksi
    std::snprintf(find_sprite(TAG_SCORE)->text, MaxTextLength, "%06d0", (int)score/10);

    // kirjoita piste-ennätys merkkijonoksi ja vilkuta tekstiä puna-valkoisena jos ennätys on meidän
    Object *o = find_sprite(TAG_HIGH_SCORE);
    std::snprintf(o->text, MaxTextLength, "%06d0", (int)highscore/10);
    o->g = o->b = (highscore == score && fmod(t, 1000.f) < 500.f) ? 0 : 1;

    // päivitä peliaika mm:ss -muodossa
    int gameSeconds = (int)((emscripten_performance_now() - gameStartTime) / 1000.0);
    std::snprintf(find_sprite(TAG_MINUTES)->text, MaxTextLength, "%02d", gameSeconds / 60);
    std::snprintf(find_sprite(TAG_SECONDS)->text, MaxTextLength, "%02d", gameSeconds % 60);
}

Object create_text(float x, float y, Tag tag)
{
    return { .x=x, .y=y, .img=IMG_TEXT, .tag=tag, .r=1.f, .g=1.f, .b=1.f, .a=1.f, .fontId=FONT_C64, .fontSize=20, .spacing=15 };
}

void EnterTitle()
{
    scene.clear();
    scene.push_back({ .x=0.f, .y=0.f, .img = IMG_TITLE });
    currentRoom = update_title;
}

constexpr int TopLeftToBottomLeft(int y) { return GAME_HEIGHT - y; }

void EnterEndScreen()
{
    scene.clear();
    scene.push_back({ .x=0.f, .y=0.f, .img = IMG_ENDSCREEN });
    scene.push_back(create_text(359, TopLeftToBottomLeft(294) - 13, TAG_SCORE));
    std::snprintf(find_sprite(TAG_SCORE)->text, MaxTextLength, "%06d0", (int)score/10);

    currentRoom = UpdateEndSreen;
}

void EnterGame()
{
    scene.clear();
    scene.push_back({ .x=0.f, .y=0.f, .img=IMG_ROAD, .tag=TAG_ROAD });
    scene.push_back({ .x=4096.f, .y=0.f, .img=IMG_ROAD, .tag=TAG_ROAD });
    scene.push_back({ .x=100.f, .y=120.f, .img=IMG_BATMAN, .tag=TAG_PLAYER, .mass=0.05f, .velx=0.05f });
    scene.push_back({ .x=0.f, .y=314.f, .img=IMG_SCOREBAR });
    scene.push_back({ .x=380.f, .y=330.f, .img=IMG_LIFE, .tag=TAG_LIFE1 });
    scene.push_back({ .x=440.f, .y=330.f, .img=IMG_LIFE, .tag=TAG_LIFE2 });
    scene.push_back({ .x=500.f, .y=330.f, .img=IMG_LIFE, .tag=TAG_LIFE3 });

    scene.push_back(create_text(165.f, 364.f, TAG_SCORE));
    scene.push_back(create_text(165.f, 332.f, TAG_HIGH_SCORE));
    scene.push_back(create_text(465.f, 364.f, TAG_MINUTES));
    scene.push_back(create_text(510.f, 364.f, TAG_SECONDS));

    currentRoom = update_game;

    gameStartTime = emscripten_performance_now();
    lastHitTime = 0.f;
    lives = 3;
    spawnTimer = 0.f;
    score = 0.f;
}

int main()
{
    init_webgl();
    emscripten_request_animation_frame_loop(&game_tick, nullptr);

    // testImage = create_texture();
    // load_image(testImage, "title.png", &testImageWidth, &testImageHeight);

    for(size_t i = 0; i < audioUrls.size(); ++i)
        preload_audio(i, audioUrls[i]);

    load_font(FONT_C64, "c64.ttf");

    play_audio(AUDIO_BG_MUSIC, EM_TRUE);

    for(auto& img : images)
    {
        img.glTexture = create_texture();
        if (img.url)
            load_image(img.glTexture, img.url, &img.width, &img.height);
    }

    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr, EM_FALSE, KeyHandler);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr, EM_FALSE, KeyHandler);

    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_FALSE, ResizeHandler);

    emscripten_set_touchstart_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr, EM_FALSE, TouchHandler);
    emscripten_set_touchmove_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr, EM_FALSE, TouchHandler);
    emscripten_set_touchend_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr, EM_FALSE, TouchHandler);
    emscripten_set_touchcancel_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr, EM_FALSE, TouchHandler);

    EnterTitle();
}
