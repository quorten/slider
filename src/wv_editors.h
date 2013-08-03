/* Functions for the wave editors.

Copyright (C) 2011, 2012, 2013 Andrew Makousky
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

/**
 * @file
 * Functions for the wave editors.
 *
 * Not only does this module contain functions for the wave editors,
 * it also contains user interface helper functions and functions that
 * prepare for file handling.  However, some of the file handling code
 * was moved to file_business.h to solve problems with different
 * Microsoft runtime versions when this program is compiled with
 * Microsoft Visual Studio.
 */

#ifndef WV_EDITORS_H
#define WV_EDITORS_H

#include "gawrapper.h"

typedef struct _Wv_Data Wv_Data;
typedef struct _Wv_Editor_Data Wv_Editor_Data;
typedef struct _Wv_Fund_Freq Wv_Fund_Freq;
typedef struct _Slide_Data Slide_Data;

/**
 * Data for a single harmonic.
 */
struct _Wv_Data
{
  unsigned harmc_num; /**< Harmonic number */
  float amplitude;
  unsigned group_idx; /**< Index into the allocated array */
};

GA_WTYPE(Wv_Data);

/**
 * Reference structure for a scrollbar in a slider group.
 *
 * A pointer to this data structure is passed to the signal handlers
 * that handle the GtkHScrollbar widgets.
 */
struct _Slide_Data
{
  GtkWidget *widget; /**< Scrollbar widget */
  unsigned index; /**< Index into the owning Slide_Data_Ptr_array */
  /**
   * Used to calculate the value being adjusted
   *
   * This is the formula used: base + last_value * scale
   */
  gdouble base;
  gdouble last_value; /**< Previous scrollbar value */
  gboolean fund_assoc; /**< Is this associated with @a fund_editor?  */
  /** Index into @a wv_editors.  If @a fund_assoc is TRUE, this is
   * zero for @a freq_sliders and one for @a amp_sliders.  */
  unsigned parent_index; 
};

typedef Slide_Data* Slide_Data_Ptr;
GA_WTYPE(Slide_Data_Ptr);

/**
 * Data for a wave editor window.
 *
 * A wave editor window is a window located in the bottom pane of
 * Slider's user interface that is used to adjust parameters of the
 * wave.  This structure can contain information for either the window
 * used for the fundamental frequency or the windows used for the
 * harmonics.
 */
struct _Wv_Editor_Data
{
  GtkWidget *widget;
  unsigned index; /**< Index into the allocated array */
  Wv_Data *data;
  GtkWidget *fndfrq_mntisa;
  GtkWidget *fndfrq_exp;
  GtkWidget *amp_mntisa;
  GtkWidget *amp_exp;
  GtkWidget *harmc_sel;
  Slide_Data_Ptr_array *freq_sliders;
  GtkWidget *freq_slid_rm_btn;
  GtkWidget *freq_sliders_vbox;
  Slide_Data_Ptr_array *amp_sliders;
  GtkWidget *amp_slid_rm_btn;
  GtkWidget *amp_sliders_vbox;
  GtkWidget *harmc_win_rm_btn;
};

typedef Wv_Editor_Data* Wv_Editor_Data_Ptr;
GA_WTYPE(Wv_Editor_Data_Ptr);

/**
 * Collection of all data for a fundamental frequency set.
 */
struct _Wv_Fund_Freq
{
  float fund_freq; /**< The fundamental's frequency in Hertz */
  float amplitude;
  /**
   * Current phase position in audio playback.
   *
   * Ranges from 0.0 to 1.0, with 0.0 indicated playback is at the
   * beginning of the fundamental frequency's sine wave and 1.0
   * indicating playback is at the end of one period respectively.
   * Phase positions of different fundamentals will get off from their
   * expected positions when the user changes the frequency of
   * fundamentals in real time, which is why each fundamental's phase
   * position must be tracked separately.
   */
  float phase_pos;
  Wv_Data_array *harmonics;
  /** Fundamental frequency editor */
  Wv_Editor_Data fund_editor;
  /**
   * Used for tracking of editor windows.
   *
   * This field is an array of pointers rather than an array of
   * structures so that the addresses of each structure will not
   * change when the array changes.  This is important because the
   * signal handlers need a constant address for their @a
   * user_data.
   */
  Wv_Editor_Data_Ptr_array *wv_editors;
};

GA_WTYPE(Wv_Fund_Freq);

extern Wv_Fund_Freq_array *wv_all_freqs;
extern unsigned g_fund_set;

void init_wv_editors (void);
void free_wv_editors (void);
void free_slider_data (Slide_Data_Ptr_array *sliders);
void add_wv_editor (unsigned fund_freq, unsigned index,
		    Wv_Data *last_wv_data);
void remove_wv_editor (unsigned fund_freq, unsigned index);
void add_harmonic (unsigned fund_freq);
void remove_harmonic (unsigned fund_freq, unsigned index);
void add_fund_freq (void);
void remove_fund_freq (unsigned index);
void restore_prec_sliders (void);
void select_fund_freq (unsigned fund_freq);
void unselect_fund_freq (unsigned fund_freq);
gboolean save_sliw_project (char *filename);
void export_sliw_project (char *filename);
gboolean load_sliw_project (char *filename);
void new_sliw_project (void);
float calc_freq_extent (void);
void render_waves (float * ypts, unsigned num_samples, float x_max);
void plot_waveform (float * ypts, unsigned num_samples, float x_max,
		    unsigned fund_freq_idx, unsigned ofs);
void mult_amplitudes (float new_amplitude, GtkWidget * last_dialog);

#endif /* not WV_EDITORS_H */
