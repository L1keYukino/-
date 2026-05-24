#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle
typedef struct VimEngine VimEngine;

// Callback types
typedef void (*vim_state_cb)(int old_state, int new_state, const char* detail);
typedef void (*vim_asr_cb)(const char* text, int is_partial, float confidence);
typedef void (*vim_llm_cb)(const char* formatted_text);
typedef void (*vim_error_cb)(int code, const char* message);
typedef void (*vim_audio_cb)(float db);

// Lifecycle
VimEngine* vim_create(const char* config_path);
void       vim_destroy(VimEngine* e);
void       vim_start(VimEngine* e);
void       vim_stop(VimEngine* e);

// Recording
void       vim_begin_record(VimEngine* e);
void       vim_end_record(VimEngine* e);

// Callbacks
void       vim_set_state_callback(VimEngine* e, vim_state_cb cb);
void       vim_set_asr_callback(VimEngine* e, vim_asr_cb cb);
void       vim_set_llm_callback(VimEngine* e, vim_llm_cb cb);
void       vim_set_error_callback(VimEngine* e, vim_error_cb cb);
void       vim_set_audio_callback(VimEngine* e, vim_audio_cb cb);

// Queries
int        vim_is_recording(VimEngine* e);
int        vim_is_processing(VimEngine* e);
float      vim_audio_peak(VimEngine* e);

// Hotkey support (returns 0 on quit, polls messages)
int        vim_poll_events(VimEngine* e);

// Get last LLM output into caller-provided buffer. Returns length or 0.
int         vim_get_output(VimEngine* e, char* buf, int buf_size);

#ifdef __cplusplus
}
#endif
