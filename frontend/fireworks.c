#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <SDL2/SDL.h>
#include <emscripten.h>
#include <emscripten/html5.h>

#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080
#define MAX_FIREWORKS 30
#define MAX_PARTICLES 300

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
    f->targetX = f->x + random_float(-100, 100); // Slightly angled shot
    f->targetY = random_float(100, screen_height * 0.4); // Explode higher
    
    float angle = atan2(f->targetY - f->y, f->targetX - f->x);
    float speed = random_float(10.0f, 15.0f); // Faster launch
    
    f->vx = cos(angle) * speed;
    f->vy = sin(angle) * speed;
    
    // Generate vibrant colors using HSL
    float h = random_float(0, 360);
    hsl_to_rgb(h, 1.0f, 0.5f, &f->r, &f->g, &f->b);
    
    f->exploded = 0;
    f->active = 1;
}

void update() {
    // Determine screen size dynamically if needed
    // emscripten_get_element_css_size can be called here if canvas resizes, 
    // but for performance we might want to check less frequently or listen to resize events.
    // For now we assume size is set at init or updated via JS if needed.

    // Increased launch frequency (approx every 10 frames = 6 launches/sec at 60fps)
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
    // Create trail effect with semi-transparent black
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 25); // Lower alpha = longer trails
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

void main_loop() {
    update();
    draw();
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
    
    // Initialize fireworks pool
    for (int i = 0; i < MAX_FIREWORKS; i++) {
        fireworks[i].active = 0;
    }

    emscripten_set_main_loop(main_loop, 0, 1);

    return 0;
}
