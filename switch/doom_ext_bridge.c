/* doom_ext_bridge.c — Switch entry point for sillydoom.
 *
 * Hardened for homebrew release:
 *   - Console mode for startup diagnostics (print visible)
 *   - Framebuffer mode only when game is ready to render
 *   - atexit cleanup for all resources
 *   - Crash handler with SD card logging
 *   - Graceful handling of missing WAD, SD errors, OOM
 */

#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>

#include "ext.h"
#include "isilly.h"

extern const isilly_ext_desc_t *isilly_ext_doom_engine_init(void);
extern void switch_platform_init(void);
extern void switch_platform_init_console(void);
extern int  switch_platform_init_fb(uint32_t width, uint32_t height);
extern void switch_platform_destroy(void);

/* SPE stubs */
int spe_kernel_registry_init(void) { return 0; }
int spe_registry_dispatch(void *ctx, uint32_t opcode) { (void)ctx; (void)opcode; return -1; }
int spe_registry_dispatch_chain2(void *ctx, uint32_t op1, uint32_t op2) { (void)ctx; (void)op1; (void)op2; return -1; }

__attribute__((constructor))
static void doom_ext_register(void) {
    isilly_ext_register_external("doom_engine", isilly_ext_doom_engine_init);
}

/* ── Crash logging ──────────────────────────────────────────────── */

#define CRASHLOG_PATH "sdmc:/switch/sillydoom/crash.log"

static void write_crashlog(const char *reason) {
    FILE *f = fopen(CRASHLOG_PATH, "w");
    if (!f) return;
    fprintf(f, "sillydoom crash: %s\n", reason);
    fclose(f);
}

static volatile int g_step = 0;

static void crash_handler(int sig) {
    const char *name = sig == 11 ? "SIGSEGV" : sig == 10 ? "SIGBUS" :
                       sig == 6 ? "SIGABRT" : "UNKNOWN";
    char buf[128];
    snprintf(buf, sizeof(buf), "signal %d (%s) at step %d", sig, name, g_step);
    write_crashlog(buf);

    /* Try to show on console — may not work in FB mode */
    consoleInit(NULL);
    printf("\n  sillydoom crashed: %s\n", name);
    printf("  step: %d\n", g_step);
    printf("  crash.log written to SD\n\n");
    printf("  Press + to exit.\n");
    consoleUpdate(NULL);

    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break;
        consoleUpdate(NULL);
    }
    _exit(128 + sig);
}

/* ── Helpers ─────────────────────────────────────────────────────── */

static char *read_file_full(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size < 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)size + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)size, f);
    fclose(f);
    if (got != (size_t)size) { free(buf); return NULL; }
    buf[size] = 0;
    if (out_size) *out_size = (size_t)size;
    return buf;
}

static void show_error_and_wait(const char *fmt, ...) {
    consoleInit(NULL);
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n\nPress + to exit.\n");
    consoleUpdate(NULL);

    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break;
        consoleUpdate(NULL);
    }
    consoleExit(NULL);
}

/* ── Cleanup ─────────────────────────────────────────────────────── */

static void cleanup_on_exit(void) {
    switch_platform_destroy();
}

/* ── Main ────────────────────────────────────────────────────────── */

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    /* Install crash handler + cleanup */
    signal(SIGSEGV, crash_handler);
    signal(SIGBUS,  crash_handler);
    signal(SIGABRT, crash_handler);
    signal(SIGFPE,  crash_handler);
    atexit(cleanup_on_exit);

    /* Step 1: Mount romfs */
    g_step = 1;
    Result rc = romfsInit();
    if (R_FAILED(rc)) {
        show_error_and_wait("sillydoom: romfsInit failed (0x%x)\n"
                            "The NRO may be corrupted. Re-download it.", (unsigned)rc);
        return 1;
    }

    /* Step 2: Set up SD card directory for config/saves/WAD */
    g_step = 2;
    mkdir("sdmc:/switch", 0755);
    int sd_ok = (mkdir("sdmc:/switch/sillydoom", 0755) == 0 || errno == EEXIST);
    if (sd_ok) {
        struct stat sb;
        sd_ok = (stat("sdmc:/switch/sillydoom", &sb) == 0);
    }
    if (!sd_ok) {
        show_error_and_wait("sillydoom: cannot access sdmc:/switch/sillydoom/\n"
                            "Check SD card is inserted and writable.");
        romfsExit();
        return 2;
    }
    setenv("HOME", "sdmc:/switch/sillydoom", 1);

    /* Step 3: Init Switch platform (pad input, center-clamp) */
    g_step = 3;
    switch_platform_init();

    /* Step 4: chdir to romfs for script-relative paths */
    g_step = 4;
    if (chdir("romfs:/") != 0) {
        show_error_and_wait("sillydoom: chdir(romfs:/) failed.");
        romfsExit();
        return 3;
    }

    /* Step 5: Load entry script */
    g_step = 5;
    size_t script_size = 0;
    char *script = read_file_full("src/main_switch.is", &script_size);
    if (!script) {
        show_error_and_wait("sillydoom: cannot read src/main_switch.is\n"
                            "NRO romfs may be corrupted.");
        romfsExit();
        return 4;
    }

    /* Step 6: Create isilly session */
    g_step = 6;
    isilly_session_t sess = isilly_session_create();
    if (!sess) {
        free(script);
        show_error_and_wait("sillydoom: isilly_session_create failed.\n"
                            "Out of memory?");
        romfsExit();
        return 5;
    }

    /* Step 7: Init framebuffer for rendering.
     * We init FB AFTER session create but BEFORE eval so the
     * present_callback has a valid framebuffer. The script's
     * print() goes to stdout which isn't visible in FB mode,
     * but error paths use show_error_and_wait (console mode). */
    g_step = 7;
    if (!switch_platform_init_fb(1280, 720)) {
        free(script);
        isilly_session_destroy(sess);
        show_error_and_wait("sillydoom: framebuffer init failed.\n"
                            "Display error — try rebooting your Switch.");
        romfsExit();
        return 6;
    }

    /* Step 8: Run the script (enters DOOM game loop, never returns normally) */
    g_step = 8;
    char *diag = NULL;
    int eval_rc = isilly_session_eval(sess, script, script_size, &diag);
    free(script);

    /* If we get here, the script exited (error, no WAD, or user quit) */
    if (eval_rc != 0) {
        const char *msg = diag ? diag : "(no diagnostic)";

        /* Write to crash log */
        char logbuf[512];
        snprintf(logbuf, sizeof(logbuf), "script failed (rc=%d): %s", eval_rc, msg);
        write_crashlog(logbuf);

        /* Switch back to console to show error */
        switch_platform_destroy();
        show_error_and_wait("sillydoom error (rc=%d):\n%s\n\n"
                            "If no WAD found, place DOOM.WAD in:\n"
                            "  sdmc:/switch/sillydoom/DOOM.WAD",
                            eval_rc, msg);
    }
    if (diag) free(diag);

    isilly_session_destroy(sess);
    romfsExit();
    return eval_rc;
}
