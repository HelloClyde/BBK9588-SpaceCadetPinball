#ifndef SCP_RUNTIME_H
#define SCP_RUNTIME_H
#ifdef __cplusplus
extern "C" {
#endif
extern char scp_runtime_log[8192];
void scp_runtime_log_reset(void);
void scp_runtime_init(void);
void scp_runtime_fini(void);
void scp_runtime_release_all(void);
/* Mount immutable DAT/FONT/WAV resources. Returns 1 on success, 0 on failure.
 * Close all borrowed FILE views before unmounting the archive. */
int scp_resource_mount(const char *path);
void scp_resource_unmount(void);
int scp_on_stack(void *top,int (*entry)(void));
/* Store the checkpoint on the firmware caller stack, not the temporary engine
 * stack. Locals changed after setjmp and read on recovery must be volatile. */
int scp_setjmp(void *buffer) __attribute__((returns_twice));
void scp_longjmp(void *buffer,int result) __attribute__((noreturn));
void scp_runtime_panic(const char *message) __attribute__((noreturn));
#ifdef __cplusplus
}
#endif
#endif
