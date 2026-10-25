#pragma once
#ifdef __cplusplus
extern "C" {
#endif
extern char scp_error_text[256];
extern int scp_popup_requested;
void scp_audio_mix(short *output,unsigned frames);
unsigned scp_audio_active_voices(void);
unsigned scp_platform_best(void);
void scp_platform_update_best(void);
void scp_platform_save_best(void);
void scp_checkpoint(const char *stage);
#ifdef __cplusplus
}
#endif
