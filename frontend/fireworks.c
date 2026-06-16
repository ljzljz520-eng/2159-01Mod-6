#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <emscripten.h>
#include <emscripten/html5.h>

#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080
#define MAX_FIREWORKS 30
#define MAX_PARTICLES 300
#define COUNTDOWN_SECONDS 3
#define MAX_THEMES 4

typedef enum {
    STATE_COUNTDOWN,
    STATE_RUNNING,
    STATE_CANCELLED
} AppState;

typedef struct {
    const char *name;
    int bg_r, bg_g, bg_b;
    int trail_alpha;
    float hue_min, hue_max;
} Theme;

Theme themes[MAX_THEMES] = {
    {"夜空", 0, 0, 20, 25, 0.0f, 360.0f},
    {"黄昏", 40, 20, 60, 20, 0.0f, 60.0f},
    {"海洋", 0, 30, 60, 25, 180.0f, 280.0f},
    {"森林", 10, 40, 20, 25, 90.0f, 180.0f}
};

typedef struct {
    float x, y;
    float vx, vy;
    int r, g, b, a;
    float life;
    float max_life;
    int active;
} Particle;

typedef struct {
    float x, y;
    float targetX, targetY;
    float vx, vy;
    int r, g, b;
    int exploded;
    int active;
    Particle particles[MAX_PARTICLES];
} Firework;

SDL_Window *window;
SDL_Renderer *renderer;
Firework fireworks[MAX_FIREWORKS];
int screen_width = 0;
int screen_height = 0;

AppState app_state = STATE_COUNTDOWN;
int countdown_remaining = COUNTDOWN_SECONDS;
float countdown_timer = 0.0f;
int current_theme = 0;
double performance_start_time = 0.0;
int first_launch = 1;

EM_JS(void, js_remove_buttons, (), {
    const controls = document.getElementById('countdown-controls');
    if (controls) {
        controls.remove();
    }
});

EM_JS(void, js_log_performance_start, (double timestamp), {
    console.log("[演出日志] 正式演出开始时间戳:", timestamp, "ms");
    console.log("[演出日志] ISO时间:", new Date().toISOString());
    console.log("[演出日志] 性能时间戳:", performance.now(), "ms");
});

EM_JS(void, js_update_countdown, (int num, int theme_idx), {
    const themeNames = ['夜空', '黄昏', '海洋', '森林'];
    const el = document.getElementById('countdown-display');
    if (el) {
        el.textContent = num > 0 ? num : 'GO!';
        el.classList.add('pulse');
        setTimeout(function() { el.classList.remove('pulse'); }, 500);
    }
    const themeEl = document.getElementById('current-theme');
    if (themeEl) {
        themeEl.textContent = '当前主题: ' + themeNames[theme_idx || 0];
    }
});

EM_JS(void, js_init_buttons, (), {
    if (window.__countdownButtonsInitialized) return;
    window.__countdownButtonsInitialized = true;
    
    const container = document.createElement('div');
    container.style.cssText = `
        position: fixed;
        top: 20px;
        right: 20px;
        z-index: 1000;
        display: flex;
        gap: 10px;
        font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
    `;
    container.id = 'countdown-controls';
    
    const cancelBtn = document.createElement('button');
    cancelBtn.textContent = '取消 (ESC)';
    cancelBtn.style.cssText = `
        padding: 10px 20px;
        background: rgba(220, 53, 69, 0.9);
        color: white;
        border: none;
        border-radius: 6px;
        cursor: pointer;
        font-size: 14px;
        backdrop-filter: blur(10px);
        transition: all 0.2s;
    `;
    cancelBtn.onmouseenter = function() { cancelBtn.style.background = 'rgba(220, 53, 69, 1)'; };
    cancelBtn.onmouseleave = function() { cancelBtn.style.background = 'rgba(220, 53, 69, 0.9)'; };
    cancelBtn.id = 'cancel-btn';
    cancelBtn.onclick = function() {
        if (typeof Module._cancel_performance === 'function') {
            Module._cancel_performance();
        }
    };
    
    const themeBtn = document.createElement('button');
    themeBtn.textContent = '切换主题 (T)';
    themeBtn.style.cssText = `
        padding: 10px 20px;
        background: rgba(13, 110, 253, 0.9);
        color: white;
        border: none;
        border-radius: 6px;
        cursor: pointer;
        font-size: 14px;
        backdrop-filter: blur(10px);
        transition: all 0.2s;
    `;
    themeBtn.onmouseenter = function() { themeBtn.style.background = 'rgba(13, 110, 253, 1)'; };
    themeBtn.onmouseleave = function() { themeBtn.style.background = 'rgba(13, 110, 253, 0.9)'; };
    themeBtn.id = 'theme-btn';
    themeBtn.onclick = function() {
        if (typeof Module._switch_theme === 'function') {
            Module._switch_theme();
        }
    };
    
    container.appendChild(cancelBtn);
    container.appendChild(themeBtn);
    document.body.appendChild(container);
    
    console.log("[倒计时控制器] 按钮已创建并绑定");
});

void cancel_performance();
void switch_theme();

float random_float(float min, float max) {
    return min + (float)rand() / ((float)RAND_MAX / (max - min));
}

// Convert HSL to RGB
void hsl_to_rgb(float h, float s, float l, int *r, int *g, int *b) {
    float c = (1.0f - fabsf(2.0f * l - 1.0f)) * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = l - 0.5f * c;
    float r_temp, g_temp, b_temp;

    if (h >= 0 && h < 60) { r_temp = c; g_temp = x; b_temp = 0; }
    else if (h >= 60 && h < 120) { r_temp = x; g_temp = c; b_temp = 0; }
    else if (h >= 120 && h < 180) { r_temp = 0; g_temp = c; b_temp = x; }
    else if (h >= 180 && h < 240) { r_temp = 0; g_temp = x; b_temp = c; }
    else if (h >= 240 && h < 300) { r_temp = x; g_temp = 0; b_temp = c; }
    else { r_temp = c; g_temp = 0; b_temp = x; }

    *r = (int)((r_temp + m) * 255);
    *g = (int)((g_temp + m) * 255);
    *b = (int)((b_temp + m) * 255);
}

void init_particle(Particle *p, float x, float y, int r, int g, int b) {
    p->x = x;
    p->y = y;
    float angle = random_float(0, M_PI * 2);
    // Increased speed range for bigger explosion
    float speed = random_float(2.0f, 6.0f);
    p->vx = cos(angle) * speed;
    p->vy = sin(angle) * speed;
    p->r = r;
    p->g = g;
    p->b = b;
    p->a = 255;
    p->life = random_float(40, 80);
    p->max_life = p->life;
    p->active = 1;
}

void init_firework(Firework *f) {
    f->x = random_float(100, screen_width - 100);
    f->y = screen_height;
    f->targetX = f->x + random_float(-100, 100);
    f->targetY = random_float(100, screen_height * 0.4);
    
    float angle = atan2(f->targetY - f->y, f->targetX - f->x);
    float speed = random_float(10.0f, 15.0f);
    
    f->vx = cos(angle) * speed;
    f->vy = sin(angle) * speed;
    
    Theme *t = &themes[current_theme];
    float h = random_float(t->hue_min, t->hue_max);
    hsl_to_rgb(h, 1.0f, 0.5f, &f->r, &f->g, &f->b);
    
    f->exploded = 0;
    f->active = 1;
}

void launch_first_round() {
    int launched = 0;
    for (int i = 0; i < MAX_FIREWORKS && launched < 5; i++) {
        if (!fireworks[i].active) {
            init_firework(&fireworks[i]);
            launched++;
        }
    }
    first_launch = 0;
}

void start_performance() {
    app_state = STATE_RUNNING;
    js_remove_buttons();
    
    EM_ASM({
        const overlay = document.getElementById('countdown-overlay');
        if (overlay) {
            overlay.style.transition = 'opacity 0.5s ease-out';
            overlay.style.opacity = '0';
            setTimeout(function() {
                if (overlay) overlay.classList.add('hidden');
            }, 500);
        }
    });
    
    performance_start_time = emscripten_get_now();
    printf("[演出开始] 正式演出开始时间戳: %.3f ms (%.3f s)\n", 
           performance_start_time, performance_start_time / 1000.0);
    launch_first_round();
}

void draw_countdown() {
    Theme *t = &themes[current_theme];
    
    SDL_SetRenderDrawColor(renderer, t->bg_r, t->bg_g, t->bg_b, 255);
    SDL_Rect rect = {0, 0, screen_width, screen_height};
    SDL_RenderFillRect(renderer, &rect);
    
    SDL_RenderPresent(renderer);
}

EM_BOOL handle_keydown(int eventType, const EmscriptenKeyboardEvent *keyEvent, void *userData) {
    if (eventType == EMSCRIPTEN_EVENT_KEYDOWN) {
        if (app_state == STATE_COUNTDOWN) {
            if (strcmp(keyEvent->key, "Escape") == 0) {
                cancel_performance();
                return 1;
            } else if (strcmp(keyEvent->key, "t") == 0 || strcmp(keyEvent->key, "T") == 0) {
                switch_theme();
                return 1;
            }
        }
    }
    return 0;
}

void update_countdown(float dt) {
    countdown_timer += dt;
    if (countdown_timer >= 1000.0f) {
        countdown_timer -= 1000.0f;
        countdown_remaining--;
        printf("[倒计时] %d\n", countdown_remaining);
        js_update_countdown(countdown_remaining, current_theme);
        if (countdown_remaining <= 0) {
            start_performance();
            js_log_performance_start(performance_start_time);
        }
    }
}

void update(float dt) {
    if (app_state == STATE_COUNTDOWN) {
        update_countdown(dt);
        return;
    }
    
    if (app_state != STATE_RUNNING) {
        return;
    }

    if (rand() % 10 == 0) {
        for (int i = 0; i < MAX_FIREWORKS; i++) {
            if (!fireworks[i].active) {
                init_firework(&fireworks[i]);
                break;
            }
        }
    }

    for (int i = 0; i < MAX_FIREWORKS; i++) {
        if (!fireworks[i].active) continue;

        Firework *f = &fireworks[i];

        if (!f->exploded) {
            f->x += f->vx;
            f->y += f->vy;
            f->vy += 0.15; // Gravity on rocket
            
            if (f->vy >= 0 || f->y <= f->targetY) { // Explode at peak or target
                f->exploded = 1;
                // Burst
                for (int j = 0; j < MAX_PARTICLES; j++) {
                    init_particle(&f->particles[j], f->x, f->y, f->r, f->g, f->b);
                }
            }
        } else {
            int active_particles = 0;
            for (int j = 0; j < MAX_PARTICLES; j++) {
                if (f->particles[j].active) {
                    active_particles++;
                    Particle *p = &f->particles[j];
                    
                    p->x += p->vx;
                    p->y += p->vy;
                    p->vx *= 0.96; // Air resistance
                    p->vy *= 0.96;
                    p->vy += 0.15; // Gravity
                    
                    p->life -= 1.0f;
                    
                    // Fade out
                    float alpha_ratio = p->life / p->max_life;
                    p->a = (int)(alpha_ratio * 255);
                    
                    if (p->life <= 0) {
                        p->active = 0;
                    }
                }
            }
            if (active_particles == 0) {
                f->active = 0;
            }
        }
    }
}

void draw() {
    Theme *t = &themes[current_theme];
    
    if (app_state == STATE_COUNTDOWN) {
        draw_countdown();
        return;
    }
    
    if (app_state == STATE_CANCELLED) {
        SDL_SetRenderDrawColor(renderer, t->bg_r, t->bg_g, t->bg_b, 255);
        SDL_Rect rect = {0, 0, screen_width, screen_height};
        SDL_RenderFillRect(renderer, &rect);
        SDL_RenderPresent(renderer);
        return;
    }
    
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, t->bg_r, t->bg_g, t->bg_b, t->trail_alpha);
    SDL_Rect rect = {0, 0, screen_width, screen_height};
    SDL_RenderFillRect(renderer, &rect);

    for (int i = 0; i < MAX_FIREWORKS; i++) {
        if (!fireworks[i].active) continue;
        
        Firework *f = &fireworks[i];
        
        if (!f->exploded) {
            // Draw rising firework (rocket)
            SDL_SetRenderDrawColor(renderer, f->r, f->g, f->b, 255);
            SDL_Rect r = {(int)f->x - 2, (int)f->y - 2, 4, 4};
            SDL_RenderFillRect(renderer, &r);
        } else {
            // Draw explosion particles
            for (int j = 0; j < MAX_PARTICLES; j++) {
                if (f->particles[j].active) {
                    Particle *p = &f->particles[j];
                    SDL_SetRenderDrawColor(renderer, p->r, p->g, p->b, p->a);
                    // Draw slightly larger particles for better visibility
                    SDL_Rect p_rect = {(int)p->x - 1, (int)p->y - 1, 3, 3};
                    SDL_RenderFillRect(renderer, &p_rect);
                }
            }
        }
    }

    SDL_RenderPresent(renderer);
}

float last_time = 0.0f;

void main_loop() {
    float current_time = (float)emscripten_get_now();
    float dt = current_time - last_time;
    last_time = current_time;
    
    if (dt > 100.0f) dt = 16.67f;
    
    update(dt);
    draw();
}

EMSCRIPTEN_KEEPALIVE
void cancel_performance() {
    app_state = STATE_CANCELLED;
    js_remove_buttons();
    
    EM_ASM({
        const display = document.getElementById('countdown-display');
        const hint = document.getElementById('countdown-hint');
        const theme = document.getElementById('current-theme');
        if (display) display.textContent = '已取消';
        if (hint) hint.textContent = "";
        if (theme) theme.textContent = "";
        setTimeout(function() {
            if (display) display.remove();
            if (hint) hint.remove();
            if (theme) theme.remove();
        }, 2000);
    });
    
    printf("[演出取消] 用户取消了烟花表演\n");
}

EMSCRIPTEN_KEEPALIVE
void switch_theme() {
    current_theme = (current_theme + 1) % MAX_THEMES;
    printf("[主题切换] 当前主题: %s\n", themes[current_theme].name);
    js_update_countdown(countdown_remaining, current_theme);
}

int main() {
    srand(time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    double w, h;
    emscripten_get_element_css_size("#canvas", &w, &h);
    screen_width = (int)w;
    screen_height = (int)h;

    SDL_CreateWindowAndRenderer(screen_width, screen_height, 0, &window, &renderer);
    
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, 1, handle_keydown);
    
    js_init_buttons();
    js_update_countdown(COUNTDOWN_SECONDS, current_theme);
    
    printf("[初始化] 烟花表演已就绪，开始 %d 秒倒计时\n", COUNTDOWN_SECONDS);
    printf("[倒计时] %d\n", COUNTDOWN_SECONDS);
    
    for (int i = 0; i < MAX_FIREWORKS; i++) {
        fireworks[i].active = 0;
    }

    last_time = (float)emscripten_get_now();
    emscripten_set_main_loop(main_loop, 0, 1);

    return 0;
}
