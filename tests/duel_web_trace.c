/* Test protocol: run the same trace through the native and Wasm bridge. */
#include "duel_clock.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int web_init(uint32_t seed, int mode);
int web_handle(int event, double now_us);
int web_key(int button, double now_us);
int web_tick(double now_us);
void web_pause(double now_us);
void web_home(double now_us);
void web_exit(void);
int web_set_fixture(int phase, uint32_t target_ms, uint32_t elapsed0, uint32_t elapsed1,
                    int score0, int score1, int starter, int current, int winner,
                    int auto0, int auto1, double now_us);
double web_state(unsigned field);

#define STATE_COUNT 31

int main(void)
{
    assert(web_init(1, 0));
    char line[512];
    while (fgets(line, sizeof(line), stdin)) {
        unsigned seed;
        int mode, event, button;
        double now_us;
        int phase, starter, current, winner, auto0, auto1, score0, score1;
        uint32_t target_ms, elapsed0, elapsed1;

        switch (line[0]) {
        case 'I':
            assert(sscanf(line + 1, "%u %d", &seed, &mode) == 2);
            assert(web_init(seed, mode));
            break;
        case 'E':
            assert(sscanf(line + 1, "%d %lf", &event, &now_us) == 2);
            web_handle(event, now_us);
            break;
        case 'K':
            assert(sscanf(line + 1, "%d %lf", &button, &now_us) == 2);
            web_key(button, now_us);
            break;
        case 'T':
            assert(sscanf(line + 1, "%lf", &now_us) == 1);
            web_tick(now_us);
            break;
        case 'P':
            if (sscanf(line + 1, "%lf", &now_us) == 1) {
                web_pause(now_us);
            } else {
                web_pause(0.0);
            }
            break;
        case 'H':
            if (sscanf(line + 1, "%lf", &now_us) == 1) {
                web_home(now_us);
            } else {
                web_home(0.0);
            }
            break;
        case 'X':
            web_exit();
            break;
        case 'R':
            assert(sscanf(line + 1, "%d %u %u %u %d %d %d %d %d %d %d %lf",
                          &phase, &target_ms, &elapsed0, &elapsed1,
                          &score0, &score1, &starter, &current, &winner,
                          &auto0, &auto1, &now_us) == 12);
            assert(web_set_fixture(phase, target_ms, elapsed0, elapsed1,
                                   score0, score1, starter, current, winner,
                                   auto0, auto1, now_us));
            break;
        case 'S':
            for (unsigned i = 0; i < STATE_COUNT; i++) {
                double d = web_state(i);
                assert(fwrite(&d, sizeof(d), 1, stdout) == 1);
            }
            fflush(stdout);
            break;
        case '\n':
        case '#':
            break;
        default:
            fprintf(stderr, "Unknown command: %s\n", line);
            abort();
        }
    }
    return 0;
}
