/* Audio playback interface glue.

Copyright (C) 2013 Andrew Makousky
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
DAMAGE.  */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <gtk/gtk.h>

#include "audio.h"
#include "interface.h"
#include "support.h"
#include "wv_editors.h"
#include "callbacks.h"

gboolean audio_playing = FALSE;
float agc_volume = 0.5;
static unsigned sample_rate;

static int
audio_process (float * out, unsigned long frames_per_buffer)
{
  float time_elap;
  unsigned i;

  /* Zero the buffer.  */
  for (i = 0; i < frames_per_buffer; i++)
    out[i] = 0.0;

  if (!audio_playing)
    return 0;

  /* Render the waveform.  */
  time_elap = (float) frames_per_buffer / sample_rate;
  for (i = 0; i < wv_all_freqs->len; i++)
    {
      Wv_Fund_Freq *cur_fund = &wv_all_freqs->d[i];
      unsigned start = (unsigned) (sample_rate * cur_fund->phase_pos /
  				   cur_fund->fund_freq);
      float intpart;
      plot_waveform (out, frames_per_buffer, time_elap, i, start);
      cur_fund->phase_pos +=
  	cur_fund->fund_freq * time_elap;
      /* NB: MSVC complains with false stack corruption alarms when
	 this code is compiled in the Debug configuration.  Comment this
	 line out for debugging with MSVC if you must, but don't leave it
	 commented out for a release binary.  */
      cur_fund->phase_pos = modff (cur_fund->phase_pos, &intpart);
    }

  { /* Normalize the waveform and clip near the desired maximum
       amplitude.  */
    float agc_clip_volume = agc_volume * 1.25;
    for (i = 0; i < frames_per_buffer; i++)
      {
	out[i] *= agc_volume / max_ypt;
	out[i] = ((out[i] > 0) ?
		  MIN (out[i], agc_clip_volume) :
		  MAX (out[i], -agc_clip_volume));
      }
  }

  return 0;
}

#ifdef USE_PORTAUDIO

#include <portaudio.h>

static PaStream *audio_stream = NULL;

static int
portaudio_callback (const void * input_buffer, void * output_buffer,
		    unsigned long frames_per_buffer,
		    const PaStreamCallbackTimeInfo * time_info,
		    PaStreamCallbackFlags status_flags,
		    void * user_data);

static void
pa_error (PaError err)
{
  GtkWidget *dialog;
  dialog = gtk_message_dialog_new_with_markup
    (GTK_WINDOW (main_window),
     GTK_DIALOG_DESTROY_WITH_PARENT,
     GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
     _("<b><big>An audio error occurred.</big></b>\n\n%s"),
     Pa_GetErrorText (err));
  gtk_window_set_title (GTK_WINDOW (dialog), _("Slider Wave Editor"));
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

void
audio_init (void)
{
  PaError err;
  PaStreamParameters output_params;
#ifndef G_OS_WIN32
  err = PaJack_SetClientName ("slider");
  if (err != paNoError) goto error;
#endif
  err = Pa_Initialize ();
  if (err != paNoError) goto error;

  { /* Pick an audio device.  Prefer JACK if it is available.  */
    PaDeviceIndex num_devs;
    const PaDeviceInfo *dev;
    const PaHostApiInfo *api;
    PaDeviceIndex default_dev;
    PaDeviceIndex jack_dev = (PaDeviceIndex)-1;
    PaDeviceIndex i;
    PaTime latency;

    fputs ("\nPortAudio Device Info\n", stdout);
    fputs ("*********************\n\n", stdout);

    num_devs = Pa_GetDeviceCount ();
    default_dev = Pa_GetDefaultOutputDevice ();
    dev = Pa_GetDeviceInfo (default_dev);
    api = Pa_GetHostApiInfo (dev->hostApi);
    latency = dev->defaultLowOutputLatency;
    sample_rate = (unsigned) dev->defaultSampleRate;
    fputs ("Default Audio Device:\n", stdout);
    printf ("Name: %s, API: %s\n", dev->name, api->name);

    fputs ("\nAudio Devices:\n", stdout);
    for (i = 0; i < num_devs; i++)
      {
	dev = Pa_GetDeviceInfo (i);
	api = Pa_GetHostApiInfo (dev->hostApi);
	printf ("%i. Name: %s, API: %s\n", i, dev->name, api->name);
	if (!strcmp (Pa_GetHostApiInfo (dev->hostApi)->name,
		     "JACK Audio Connection Kit") &&
	    jack_dev == -1)
	  {
	    jack_dev = i;
	    latency = dev->defaultLowOutputLatency;
	    sample_rate = (unsigned) dev->defaultSampleRate;
	  }
      }
    if (jack_dev != -1)
      fputs ("\nUsing first JACK audio device.\n", stdout);
    else
      fputs ("\nUsing default audio device.\n", stdout);

    output_params.device = (jack_dev != -1) ? jack_dev : default_dev;
    output_params.channelCount = 1;
    output_params.sampleFormat = paFloat32;
    output_params.suggestedLatency = latency;
    output_params.hostApiSpecificStreamInfo = NULL;
  }

  err = Pa_OpenStream (&audio_stream,
		       NULL, /* no input channels */
		       &output_params,
		       sample_rate,
		       paFramesPerBufferUnspecified,
		       paNoFlag,
		       portaudio_callback,
		       NULL);
  if (err != paNoError) goto error;
  return;
 error:
  Pa_Terminate ();
  audio_stream = NULL;
  pa_error (err);
}

static int
portaudio_callback (const void * input_buffer, void * output_buffer,
		    unsigned long frames_per_buffer,
		    const PaStreamCallbackTimeInfo * time_info,
		    PaStreamCallbackFlags status_flags,
		    void * user_data)
{
  float *out = (float *) output_buffer;
  (void) input_buffer; /* Prevent unused variable warning.  */
  return audio_process (out, frames_per_buffer);
}

void
audio_play (void)
{
  PaError err;
  if (audio_stream == NULL || audio_playing)
    return;

  { /* Zero the phase positions.  */
    unsigned i;
    for (i = 0; i < wv_all_freqs->len; i++)
 	wv_all_freqs->d[i].phase_pos = 0.0;
  }

  err = Pa_StartStream (audio_stream);
  if (err != paNoError)
    pa_error (err);
  audio_playing = TRUE;
}

void
audio_stop (void)
{
  PaError err;
  if (audio_stream == NULL || !audio_playing)
    return;
  err = Pa_StopStream (audio_stream);
  if (err != paNoError)
    pa_error (err);
  audio_playing = FALSE;
}

void
audio_shutdown (void)
{
  PaError err = paNoError;
  if (audio_stream != NULL)
    {
      err = Pa_CloseStream (audio_stream);
      audio_stream = NULL;
    }
  Pa_Terminate ();
  if (err != paNoError)
    pa_error (err);
}

#endif /* USE_PORTAUDIO */

#ifdef USE_JACK

#include <jack/jack.h>

static jack_client_t *jack_client;
static jack_port_t *output_port;
static gboolean jack_gone = FALSE;
static gboolean jack_cleaned = FALSE;

static int jack_samples_changed (jack_nframes_t nframes, void * arg);
static int jack_process (jack_nframes_t nframes, void * arg);
static void jack_shutdown (void * arg);
static void jack_cleanup (void);

void
audio_init (void)
{
  char *error_desc = NULL;
  const char **ports;
  const char *client_name = "slider";
  const char *server_name = NULL;
  jack_options_t options = JackNullOption;
  jack_status_t status;

  jack_client = jack_client_open (client_name, options, &status, server_name);
  if (jack_client == NULL)
    {
      error_desc = _("Failed to open session with JACK.");
      if (status & JackServerFailed)
	error_desc = _("Unable to connect to JACK server.");
      goto error;
    }
  jack_set_process_callback (jack_client, jack_process, 0);
  jack_on_shutdown (jack_client, jack_shutdown, 0);
  sample_rate = jack_get_sample_rate (jack_client);
  jack_set_sample_rate_callback (jack_client, jack_samples_changed, 0);
  output_port = jack_port_register (jack_client, "output",
				    JACK_DEFAULT_AUDIO_TYPE,
				    JackPortIsOutput, 0);
  if (output_port == NULL)
    {
      error_desc = _("No more output ports available.");
      goto error;
    }
  if (jack_activate (jack_client))
    {
      error_desc = _("Cannot activate client.");
      goto error;
    }
  ports = jack_get_ports (jack_client, NULL, NULL,
			  JackPortIsPhysical | JackPortIsInput);

  /* Connect to the physical output ports by default.  */
  if (ports != NULL)
    jack_connect (jack_client, jack_port_name (output_port), ports[0]);
  free (ports);
  return;

 error:
  {
    GtkWidget *dialog;
    dialog = gtk_message_dialog_new_with_markup
      (GTK_WINDOW (main_window),
       GTK_DIALOG_DESTROY_WITH_PARENT,
       GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
       _("<b><big>An error occurred while initializing " \
	 "JACK.</big></b>\n\n%s"),
       error_desc);
    gtk_window_set_title (GTK_WINDOW (dialog), _("Slider Wave Editor"));
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
  }
}

static int
jack_samples_changed (jack_nframes_t nframes, void * arg)
{
  sample_rate = nframes;
  return 0;
}

static int
jack_process (jack_nframes_t nframes, void * arg)
{
  float *out = jack_port_get_buffer (output_port, nframes);
  return audio_process (out, nframes);
}

static void
jack_shutdown (void * arg)
{
  jack_gone = TRUE;
}

void
audio_play (void)
{
  /* Check if the connection to JACK was terminated.  */
  if (jack_gone)
    {
      jack_cleanup ();
      return;
    }
  if (audio_playing)
    return;

  { /* Zero the phase positions.  */
    unsigned i;
    for (i = 0; i < wv_all_freqs->len; i++)
 	wv_all_freqs->d[i].phase_pos = 0.0;
  }
  audio_playing = TRUE;
}

void
audio_stop (void)
{
  audio_playing = FALSE;
}

void
audio_shutdown (void)
{
  if (!jack_cleaned)
    jack_client_close (jack_client);
}

/**
 * Cleanup JACK if JACK connection was terminated.
 */
static void
jack_cleanup (void)
{
  GtkWidget *dialog;
  if (jack_cleaned)
    return;
  dialog = gtk_message_dialog_new_with_markup
    (GTK_WINDOW (main_window),
     GTK_DIALOG_DESTROY_WITH_PARENT,
     GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
     _("<b><big>Slider was disconnected from JACK.</big></b>\n\n" \
       "You will have to save your work and restart " \
       "Slider to reconnect."));
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
  jack_client_close (jack_client);
  audio_playing = FALSE;
  jack_cleaned = TRUE;
}

#endif /* USE_JACK */

#if !defined(USE_PORTAUDIO) && !defined(USE_JACK)

void audio_init (void) {}

void
audio_play (void)
{
  GtkWidget *dialog;
  dialog = gtk_message_dialog_new (GTK_WINDOW (main_window),
				   GTK_DIALOG_DESTROY_WITH_PARENT,
				   GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
	   _("Sorry, but playback is not supported in this version."));
  gtk_window_set_title (GTK_WINDOW (dialog), _("Not implemented"));
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

void audio_stop (void) { audio_play (); }
void audio_shutdown (void) {}

#endif /* !defined(USE_PORTAUDIO) && !defined(USE_JACK) */
