#pragma once

struct cdplusg_portaudio_context;

struct cdplusg_portaudio_context * cdplusg_portaudio_context_initialize (const char *audio_filename, double scale);
unsigned int cdplusg_portaudio_context_get_elapsed_time_ms (struct cdplusg_portaudio_context *context);
void cdplusg_portaudio_context_restart (struct cdplusg_portaudio_context *context);
void cdplusg_portaudio_context_pause (struct cdplusg_portaudio_context *context);
void cdplusg_portaudio_context_resume (struct cdplusg_portaudio_context *context);
void cdplusg_portaudio_context_toggle_playback (struct cdplusg_portaudio_context *context);
void cdplusg_portaudio_context_destroy (struct cdplusg_portaudio_context *context);
