/* Calls the real bootstrap wrappers with fake SDL/payload callbacks, not a game DLL. */
#include "../runtime/sdl_proxy.c"
static int failures, polls, events, steps, draw_at, swap_at;
static int real_poll(void *event) {
    int value = ++polls;
    memcpy(event, &value, 4);
    return value < 4;
}
static int filter(const void *event) {
    int value = 0;
    memcpy(&value, event, 4);
    events++;
    return value < 3;
}
static void pump(void) { steps++; }
static void draw(void *window) {
    if (window != (void *)123) failures++;
    draw_at = ++steps;
}
static void swap(void *window) {
    if (window != (void *)123) failures++;
    swap_at = ++steps;
}
static BOOL CALLBACK skip_init(PINIT_ONCE once, PVOID p, PVOID *c) {
    (void)once;
    (void)p;
    (void)c;
    return TRUE;
}
int main(void) {
    InitOnceExecuteOnce(&g_init_once, skip_init, NULL, NULL);
    g_real_poll_event = real_poll;
    g_ddcoop_event = filter;
    g_ddcoop_poll = pump;
    int value = 0;
    if (!SDL_PollEvent(&value) || value != 3 || polls != 3 || events != 3 || steps != 1) failures++;
    if (SDL_PollEvent(&value) || polls != 4 || events != 3 || steps != 2) failures++;
    g_real_swap = swap;
    g_ddcoop_draw = draw;
    SDL_GL_SwapWindow((void *)123);
    if (draw_at != 3 || swap_at != 4) failures++;
    g_ddcoop_draw = NULL;
    SDL_GL_SwapWindow((void *)123);
    if (swap_at != 5) failures++;
    printf("SDL bootstrap: consumed panel input, remaining game input, pump, draw-before-swap and "
           "absent-payload fallback: %s (%d failures)\n",
           failures ? "FAILED" : "OK", failures);
    return failures ? 1 : 0;
}
