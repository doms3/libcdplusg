#pragma once

struct cdplusg_portaudio_context;

struct cdplusg_portaudio_context * cdplusg_portaudio_context_initialize (const char *audio_filename, double scale);
void cdplusg_portaudio_context_destroy (struct cdplusg_portaudio_context *context);
