/* Functions for the wave editors.

Copyright (C) 2011, 2012 Andrew Makousky
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
 * Not only does this file contain functions for the wave editors, it
 * also contains user interface helper functions and functions that
 * prepare for file handling.  However, some of the file handling code
 * was moved to file_business.c to solve problems with different
 * Microsoft runtime versions when this program is compiled with
 * Microsoft Visual Studio.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include <math.h>
#include <locale.h>
#include <string.h>

#include "interface.h"
#include "callbacks.h"
#include "file_business.h"
#include "support.h"
#include "wv_editors.h"

/**
 * An array of all of the fundamental frequency sets.
 *
 * Inside of each element is one array that references each of the
 * windows for a fundamental frequency and another array that
 * references all harmonics.
 */
Wv_Fund_Freq_array *wv_all_freqs;

/** Current fundamental set.  */
unsigned g_fund_set;

/* Even though the original application was planned to have a command
   called '1st Harmonic Drop', it turns out that the original idea is
   only possible if all higher harmonics are also harmonics of each
   other.  The idea was to remove the fundamental frequency and change
   the first harmonic to be the new fundamental frequency.  */

/**
 * Initializes ::wv_all_freqs.
 *
 * This function should only be called whenever a new file is loaded.
 * Since Sound Studio currently only supports loading one file at a
 * time, this function is only called during program startup.
 */
void
init_wv_editors (void)
{
  wv_all_freqs = (Wv_Fund_Freq_array *)
    g_array_new (FALSE, FALSE, sizeof (Wv_Fund_Freq));
}

/**
 * Frees ::wv_all_freqs.
 *
 * All dynamically allocated data within ::wv_all_freqs is also freed.
 */
void
free_wv_editors (void)
{
  unsigned i;
  for (i = 0; i < wv_all_freqs->len; i++)
    {
      unsigned j;
      g_array_free ((GArray *) wv_all_freqs->d[i].harmonics, TRUE);
      free_slider_data (wv_all_freqs->d[i].fund_editor.freq_sliders);
      g_array_free ((GArray *) wv_all_freqs->d[i].fund_editor.freq_sliders,
		    TRUE);
      free_slider_data (wv_all_freqs->d[i].fund_editor.amp_sliders);
      g_array_free ((GArray *) wv_all_freqs->d[i].fund_editor.amp_sliders,
		    TRUE);
      for (j = 0; j < wv_all_freqs->d[i].wv_editors->len; j++)
	{
	  free_slider_data (wv_all_freqs->d[i].wv_editors->d[j]->amp_sliders);
	  g_array_free ((GArray *) wv_all_freqs->d[i].
			wv_editors->d[j]->amp_sliders, TRUE);
	  g_slice_free1 (sizeof (Wv_Editor_Data),
			 wv_all_freqs->d[i].wv_editors->d[j]);
	}
      g_array_free ((GArray *) wv_all_freqs->d[i].wv_editors, TRUE);
    }
  g_array_free ((GArray *) wv_all_freqs, TRUE);
}

/**
 * Frees a wave editor window's slider data.
 *
 * Frees all precision sliders associated with a wave editor window.
 * @param sliders the slider data to free
 */
void
free_slider_data (Slide_Data_Ptr_array *sliders)
{
  unsigned i;
  for (i = 0; i < sliders->len; i++)
    {
      g_slice_free1 (sizeof (Slide_Data), sliders->d[i]);
      sliders->d[i] = NULL;
    }
}

/**
 * Adds a wave editor window.
 *
 * Inserts a wave editor window before the given index, viewing the
 * given data.
 * @param fund_freq the fundamental frequency set to work with
 * @param index zero-based index of the wave editor window to insert
 * before
 * @param last_wv_data the data that the new wave editor window will
 * be viewing, typically that of the wave editor window given by index
 */
void
add_wv_editor (unsigned fund_freq, unsigned index, Wv_Data *last_wv_data)
{
  Wv_Editor_Data *cur_editor;
  cur_editor = (Wv_Editor_Data *) g_slice_alloc (sizeof (Wv_Editor_Data));
  g_array_insert_val ((GArray *) wv_all_freqs->d[fund_freq].wv_editors,
		      index, cur_editor);
  cur_editor->widget = NULL;
  cur_editor->index = index;
  cur_editor->data = last_wv_data;
  /* Frequency sliders are used only for the fundamental
     frequency.  */
  cur_editor->freq_sliders = (Slide_Data_Ptr_array *)
    g_array_new (FALSE, FALSE, sizeof (Slide_Data_Ptr));
  cur_editor->freq_sliders->len = 0;
  cur_editor->freq_sliders->d = NULL;
  cur_editor->amp_sliders = (Slide_Data_Ptr_array *)
    g_array_new (FALSE, FALSE, sizeof (Slide_Data_Ptr));

  /* Recalculate all the indexes after the new editor.  */
  {
    unsigned i;
    if (index != 0)
      i = index - 1;
    else
      i = 0;
    for (; i < wv_all_freqs->d[fund_freq].wv_editors->len; i++)
      {
	Wv_Editor_Data *cur_editor;
	unsigned j;
	cur_editor = wv_all_freqs->d[fund_freq].wv_editors->d[i];
	cur_editor->index = i;
	for (j = 0; j < cur_editor->amp_sliders->len; j++)
	  {
	    cur_editor->amp_sliders->d[j]->parent_index = i;
	  }
      }
  }
}

/**
 * Deletes a wave editor window.
 *
 * Any associated GtkWidgets will be destroyed when the wave editor
 * window is removed.
 * @param fund_freq the fundamental frequency set to work with
 * @param index the index of the wave editor window to remove
 */
void
remove_wv_editor (unsigned fund_freq, unsigned index)
{
  Wv_Editor_Data *cur_editor;
  cur_editor = wv_all_freqs->d[fund_freq].wv_editors->d[index];
  free_slider_data (cur_editor->freq_sliders);
  g_array_free ((GArray *) cur_editor->freq_sliders, TRUE);
  free_slider_data (cur_editor->amp_sliders);
  g_array_free ((GArray *) cur_editor->amp_sliders, TRUE);
  if (cur_editor->widget != NULL)
    gtk_widget_destroy (cur_editor->widget);
  g_slice_free1 (sizeof (Wv_Editor_Data), cur_editor);
  g_array_remove_index ((GArray *) wv_all_freqs->d[fund_freq].wv_editors,
			index);

  /* Recalculate all the indexes after the deleted editor.  */
  {
    unsigned i;
    if (index != 0)
      i = index - 1;
    else
      i = 0;
    for (; i < wv_all_freqs->d[fund_freq].wv_editors->len; i++)
      {
	Wv_Editor_Data *cur_editor;
	unsigned j;
	cur_editor = wv_all_freqs->d[fund_freq].wv_editors->d[i];
	cur_editor->index = i;
	for (j = 0; j < cur_editor->amp_sliders->len; j++)
	  {
	    cur_editor->amp_sliders->d[j]->parent_index = i;
	  }
      }
  }
}

/**
 * Adds a harmonic.
 *
 * The new harmonic is always added onto the end of the harmonic list
 * of the current fundamental frequency set.
 * @param fund_freq the fundamental frequency set to work with
 */
void
add_harmonic (unsigned fund_freq)
{
  unsigned index;
  Wv_Data *cur_harmonic;
  Wv_Data *array_base;
  unsigned i;

  index = wv_all_freqs->d[fund_freq].harmonics->len;

  /* Due to the possibility that changing the array can cause the
     array's base address to be moved, all of the wave editor data
     pointers must be rebased.  */
  array_base = wv_all_freqs->d[fund_freq].harmonics->d;
  for (i = 0; i < wv_all_freqs->d[fund_freq].wv_editors->len; i++)
    wv_all_freqs->d[fund_freq].wv_editors->d[i]->data =
      (Wv_Data *) (wv_all_freqs->d[fund_freq].wv_editors->d[i]->data -
		   array_base);

  g_array_set_size ((GArray *) wv_all_freqs->d[fund_freq].harmonics,
		    index + 1);

  array_base = wv_all_freqs->d[fund_freq].harmonics->d;
  for (i = 0; i < wv_all_freqs->d[fund_freq].wv_editors->len; i++)
    wv_all_freqs->d[fund_freq].wv_editors->d[i]->data = array_base +
      (unsigned) (wv_all_freqs->d[fund_freq].wv_editors->d[i]->data);

  cur_harmonic = &(wv_all_freqs->d[fund_freq].harmonics->d[index]);
  cur_harmonic->amplitude = 1.0;

  if (index != 0)
    {
      cur_harmonic->harmc_num =
	wv_all_freqs->d[fund_freq].harmonics->d[index-1].harmc_num + 1;
    }
  else
    cur_harmonic->harmc_num = 2;
  cur_harmonic->group_idx = index;
}

/**
 * Deletes a harmonic.
 *
 * Whenever a harmonic is not removed from the end of the list, any
 * references to later harmonics will have to be changed.
 * @param fund_freq the fundamental frequency set to work
 * with @param index zero-based index of the harmonic to remove
 */
void
remove_harmonic (unsigned fund_freq, unsigned index)
{
  unsigned i;
  Wv_Data *array_base;

  /* Due to the possibility that changing the array can cause the
     array's base address to be moved, all of the wave editor data
     pointers must be rebased.  */
  array_base = wv_all_freqs->d[fund_freq].harmonics->d;
  for (i = 0; i < wv_all_freqs->d[fund_freq].wv_editors->len; i++)
    wv_all_freqs->d[fund_freq].wv_editors->d[i]->data =
      (Wv_Data *) (wv_all_freqs->d[fund_freq].wv_editors->d[i]->data -
		   array_base);

  g_array_remove_index ((GArray *) wv_all_freqs->d[fund_freq].harmonics,
			index);

  array_base = wv_all_freqs->d[fund_freq].harmonics->d;
  for (i = 0; i < wv_all_freqs->d[fund_freq].wv_editors->len; i++)
    wv_all_freqs->d[fund_freq].wv_editors->d[i]->data = array_base +
      (unsigned) (wv_all_freqs->d[fund_freq].wv_editors->d[i]->data);

  if (wv_all_freqs->d[fund_freq].harmonics->len == 0)
    {
      /* Remove all editor windows */
      while (wv_all_freqs->d[fund_freq].wv_editors->len > 0)
	    remove_wv_editor (fund_freq, 0);
    }

  /* Recalculate group indices as necessary */
  for (i = index; i < wv_all_freqs->d[fund_freq].harmonics->len; i++)
      wv_all_freqs->d[fund_freq].harmonics->d[i].group_idx = i;
}

/**
 * Adds a fundamental frequency set.
 *
 * The new fundamental frequency set is added to the end of
 * ::wv_all_freqs.  All of the members of the new fundamental
 * frequency set are initialized with default values.  All the widget
 * fields are set to NULL.
 */
void
add_fund_freq (void)
{
  unsigned index;
  index = wv_all_freqs->len;
  g_array_set_size ((GArray *) wv_all_freqs, index + 1);
  wv_all_freqs->d[index].fund_freq = 440.0;
  wv_all_freqs->d[index].amplitude = 1.0;
  wv_all_freqs->d[index].fund_editor.widget = NULL;
  wv_all_freqs->d[index].harmonics = (Wv_Data_array *)
    g_array_new (FALSE, FALSE, sizeof (Wv_Data));

  /* Initialize the fundamental frequency editor window */
  wv_all_freqs->d[index].fund_editor.widget = NULL;
  wv_all_freqs->d[index].fund_editor.data = NULL;
  wv_all_freqs->d[index].fund_editor.freq_sliders = (Slide_Data_Ptr_array *)
    g_array_new (FALSE, FALSE, sizeof (Slide_Data_Ptr));
  wv_all_freqs->d[index].fund_editor.amp_sliders = (Slide_Data_Ptr_array *)
    g_array_new (FALSE, FALSE, sizeof (Slide_Data_Ptr));

  wv_all_freqs->d[index].wv_editors = (Wv_Editor_Data_Ptr_array *)
    g_array_new (FALSE, FALSE, sizeof (Wv_Editor_Data_Ptr));
}

/**
 * Deletes a fundamental frequency set.
 *
 * Deletes the requested fundamental frequency set from
 * ::wv_all_freqs.  This function calls remove_wv_editor() to destroy
 * the associated wave editor windows first before destroying the
 * fundamental frequency set's data structures.
 * @param index the zero-based index of the fundamental frequency set
 * to remove
 */
void
remove_fund_freq (unsigned index)
{
  g_array_free ((GArray *) wv_all_freqs->d[index].harmonics, TRUE);
  free_slider_data (wv_all_freqs->d[index].fund_editor.freq_sliders);
  g_array_free ((GArray *) wv_all_freqs->d[index].fund_editor.freq_sliders,
		TRUE);
  free_slider_data (wv_all_freqs->d[index].fund_editor.amp_sliders);
  g_array_free ((GArray *) wv_all_freqs->d[index].fund_editor.amp_sliders,
		TRUE);
  /* Destroy the members of the wave editors array first */
  while (wv_all_freqs->d[index].wv_editors->len > 0)
    remove_wv_editor (index, 0);
  g_array_free ((GArray *) wv_all_freqs->d[index].wv_editors, TRUE);
  g_array_remove_index ((GArray *) wv_all_freqs, index);
}

/**
 * Restores the values of the precision sliders.
 *
 * During reselection, widgets get destroyed and previous widgets need
 * to get recreated.  This function should be called to restore the
 * values of precision sliders during such an event.
 */
void
restore_prec_sliders (void)
{
  GtkWidget *hscrollbar;
  GtkWidget *parent_box;
  Slide_Data *sd_block;
  unsigned i;
  unsigned j;

  parent_box = wv_all_freqs->d[g_fund_set].fund_editor.freq_sliders_vbox;
  for (j = 0; j < wv_all_freqs->d[g_fund_set].fund_editor.
	 freq_sliders->len; j++)
    {
      gdouble last_value;
      sd_block = wv_all_freqs->d[g_fund_set].fund_editor.freq_sliders->d[j];
      last_value = sd_block->last_value;
      /* Setting an adjustment with a non-zero page size is deprecated.  */
      hscrollbar =
	gtk_hscrollbar_new (GTK_ADJUSTMENT
		 (gtk_adjustment_new (last_value, 0, 21, 0.1, 1.0, 1.0)));
      gtk_widget_show (hscrollbar);
      sd_block->widget = hscrollbar;
      gtk_box_pack_start (GTK_BOX (parent_box), hscrollbar, TRUE, TRUE, 0);
      g_signal_connect ((gpointer) hscrollbar, "value_changed",
		G_CALLBACK (precslid_value_changed), (gpointer) sd_block);
    }

  parent_box = wv_all_freqs->d[g_fund_set].fund_editor.amp_sliders_vbox;
  for (j = 0; j < wv_all_freqs->d[g_fund_set].fund_editor.
	 amp_sliders->len; j++)
    {
      gdouble last_value;
      sd_block = wv_all_freqs->d[g_fund_set].fund_editor.amp_sliders->d[j];
      last_value = sd_block->last_value;
      /* Setting an adjustment with a non-zero page size is deprecated.  */
      hscrollbar =
	gtk_hscrollbar_new (GTK_ADJUSTMENT
		(gtk_adjustment_new (last_value, 0, 21, 0.1, 1.0, 1.0)));
      gtk_widget_show (hscrollbar);
      sd_block->widget = hscrollbar;
      gtk_box_pack_start (GTK_BOX (parent_box), hscrollbar, TRUE, TRUE, 0);
      g_signal_connect ((gpointer) hscrollbar, "value_changed",
		G_CALLBACK (precslid_value_changed), (gpointer) sd_block);
    }

  for (i = 0; i < wv_all_freqs->d[g_fund_set].wv_editors->len; i++)
    {
      parent_box = wv_all_freqs->d[g_fund_set].wv_editors->d[i]->
	amp_sliders_vbox;
      for (j = 0;
	   j < wv_all_freqs->d[g_fund_set].wv_editors->d[i]->amp_sliders->len;
	   j++)
	{
	  gdouble last_value;
	  sd_block =
	    wv_all_freqs->d[g_fund_set].wv_editors->d[i]->amp_sliders->d[j];
	  last_value = sd_block->last_value;
	  /* Setting an adjustment with a non-zero page size is deprecated.  */
	  hscrollbar =
	    gtk_hscrollbar_new (GTK_ADJUSTMENT
		(gtk_adjustment_new (last_value, 0, 21, 0.1, 1.0, 1.0)));
	  gtk_widget_show (hscrollbar);
	  sd_block->widget = hscrollbar;
	  gtk_box_pack_start (GTK_BOX (parent_box), hscrollbar, TRUE, TRUE, 0);
	  g_signal_connect ((gpointer) hscrollbar, "value_changed",
		    G_CALLBACK (precslid_value_changed), (gpointer) sd_block);
	}
    }
}

/**
 * Selects a fundamental frequency set.
 *
 * Whenever a new file is loaded or ::g_fund_set changes, this
 * function needs to be called to update the user interface.  Make
 * sure you call unselect_fund_freq() if necessary.
 * @param fund_freq the zero-based index of new fundamental frequency
 * set to select
 */
void
select_fund_freq (unsigned fund_freq)
{
  /* Create all the editor widgets */
  static gboolean combo_init = FALSE;
  gboolean sliders_init;
  unsigned i;
  g_fund_set = fund_freq;

  /* Check to see if reselection is necessary.  */
  if (wv_all_freqs->d[g_fund_set].fund_editor.freq_sliders->len == 0 &&
      wv_all_freqs->d[g_fund_set].fund_editor.amp_sliders->len == 0)
    sliders_init = TRUE;
  else
    sliders_init = FALSE;

  wv_all_freqs->d[fund_freq].fund_editor.widget =
    create_wvedit_holder (TRUE, fund_freq);
  gtk_box_pack_start (GTK_BOX (wave_edit_cntr),
	      wv_all_freqs->d[fund_freq].fund_editor.widget, FALSE, FALSE, 0);

  for (i = 0; i < wv_all_freqs->d[fund_freq].wv_editors->len; i++)
    {
      Wv_Editor_Data *cur_editor;
      cur_editor = wv_all_freqs->d[fund_freq].wv_editors->d[i];
      /* The values of all the widget fields will be set in the
	 following function.  */
      cur_editor->widget = create_wvedit_holder (FALSE, i);
      gtk_box_pack_start (GTK_BOX (wave_edit_cntr), cur_editor->widget,
			  FALSE, FALSE, 0);
    }

  if (combo_init == FALSE)
    {
      combo_init = TRUE;
      /* Add combo box entries */
      for (i = 0; i < wv_all_freqs->len; i++)
	{
	  unsigned cur_fund;
	  gchar cb_text[11];
	  cur_fund = i + 1;
	  sprintf (cb_text, "%u", cur_fund);
	  gtk_combo_box_append_text (GTK_COMBO_BOX (cb_fund_set), cb_text);
	  if (cur_fund == 1)
	    gtk_combo_box_set_active (GTK_COMBO_BOX (cb_fund_set), 0);
	}
    }

  if (sliders_init == FALSE)
    restore_prec_sliders ();
}

/**
 * Unselects a fundamental frequency set.
 *
 * This function must be called if a fundamental frequency set is
 * already selected.
 * @param fund_freq the zero-based index of new fundamental frequency
 * set to select
 */
void
unselect_fund_freq (unsigned fund_freq)
{
  /* Destroy all the editor widgets */
  unsigned i;
  gtk_widget_destroy (wv_all_freqs->d[fund_freq].fund_editor.widget);
  wv_all_freqs->d[fund_freq].fund_editor.widget = NULL;
  for (i = 0; i < wv_all_freqs->d[fund_freq].wv_editors->len; i++)
    {
      gtk_widget_destroy (wv_all_freqs->d[fund_freq].wv_editors->d[i]->widget);
      wv_all_freqs->d[fund_freq].wv_editors->d[i]->widget = NULL;
    }
}

/**
 * Saves a Sound Studio project file.
 *
 * @param filename the file name to save to
 */
void
save_ss_project (char *filename)
{
  FILE *fp;
  char *locale_temp;
  char *last_locale;

  fp = fopen (filename, "w");
  if (fp == NULL)
    return;

  fputs (
"# This is a Sound Studio project file.  If you change this identifier,\n"
"# you will not be able to open this file in Sound Studio.\n"
"#\n"
"# A harmonic is specified as a pair of numbers.  The first number is\n"
"# the harmonic number, and the second is the amplitude.\n", fp);

  /* The project file's contents are written in English to prevent
     compatibility problems with Sound Studio running in different
     languages.  */
  locale_temp = setlocale (LC_NUMERIC, NULL);
  last_locale = (char *) g_malloc (strlen (locale_temp) + 1);
  strcpy (last_locale, locale_temp);
  setlocale (LC_NUMERIC, "C");

  /* "libintl" overrides the default *printf functions, so to prevent
     problems with passing file pointers between different versions of
     the Microsoft C runtime, all fprintf functions must be declared
     in a separate source file.  */
  do_save_printing (fp);

  setlocale (LC_NUMERIC, last_locale);
  g_free (last_locale);
  fclose (fp);
}

/**
 * Exports a Sound Studio project.
 *
 * The exported file is represented as a series of waveforms defined
 * only by frequency and amplitude in the format of a Nyquist Lisp
 * script.  This function is intended to be used as an efficient way
 * to import Sound Studio sounds into other applications.
 * @param filename the name of the file to export to
 */
void
export_ss_project (char *filename)
{
  FILE* fp;
  char *locale_temp;
  char *last_locale;

  fp = fopen (filename, "w");
  if (fp == NULL)
    return;

  fputs (_( \
"; This is a Nyquist Lisp file.\n" \
"; You can use this with Audacity to generate a waveform.\n" \
"; To do this, select a portion of time on a track, then go to \"Effect\n" \
"; > Nyquist Prompt...\" and paste the contents of this file in.\n"), fp);

  /* The script's contents are written in English since that is the
     standard in computer programming.  */
  locale_temp = setlocale (LC_NUMERIC, NULL);
  last_locale = (char *) g_malloc (strlen (locale_temp) + 1);
  strcpy (last_locale, locale_temp);
  setlocale (LC_NUMERIC, "C");

  fputs ("(sum", fp);
  /* "libintl" overrides the default *printf functions, so to prevent
     problems with passing file pointers between different versions of
     the Microsoft C runtime, all fprintf functions must be declared
     in a separate source file.  */
  do_export_printing (fp);
  fputs (")\n", fp);

  setlocale (LC_NUMERIC, last_locale);
  g_free (last_locale);
  fclose (fp);
}

/**
 * Loads a Sound Studio project file.
 *
 * Opens, reads, and parses a Sound Studio project file.  Sound Studio
 * project files are plain text files, typically saved with the .txt
 * extension.  Unfortunately, this code does not yet have syntax error
 * checking, as the original goal of this program was to write a
 * suitable program quickly.
 * @param filename the name of the file to load
 */
void
load_ss_project (char *filename)
{
  FILE *fp;
  char *locale_temp;
  char *last_locale;
  unsigned cur_fund;
  char temp_buffer[100];

  fp = fopen (filename, "r");
  if (fp == NULL)
    return;

  /* The project file's contents are written in English to prevent
     compatibility problems with Sound Studio running in different
     languages.  */
  locale_temp = setlocale (LC_NUMERIC, NULL);
  last_locale = (char *) g_malloc (strlen (locale_temp) + 1);
  strcpy (last_locale, locale_temp);
  setlocale (LC_NUMERIC, "C");

  /* This code currently does not do error checking.  This must be
     fixed!  */
  while (fscanf (fp, "# %99[^\n]\n", temp_buffer));
  fscanf (fp, "\nFundamental %u\n", &cur_fund);
  while (!feof (fp))
    {
      unsigned i, j;
      gboolean next_fundamental;
      i = cur_fund - 1;
      add_fund_freq ();
      add_wv_editor (wv_all_freqs->len - 1, 0,
		     &wv_all_freqs->d[i].harmonics->d[0]);
      fscanf (fp, "Frequency: %g\n", &wv_all_freqs->d[i].fund_freq);
      fscanf (fp, "Amplitude: %g\n", &wv_all_freqs->d[i].amplitude);
      fscanf (fp, "Harmonics:");

      j = 0;
      next_fundamental = FALSE;
      do
	{
	  int scan_status;
	  add_harmonic (i);
	  scan_status = fscanf (fp, " %u, %g;",
				&wv_all_freqs->d[i].harmonics->d[j].harmc_num,
				&wv_all_freqs->d[i].harmonics->d[j].amplitude);
	  if (scan_status <= 0) /* No harmonics read */
	    remove_harmonic (i, wv_all_freqs->d[i].harmonics->len - 1);
	  if (fscanf (fp, "\nFundamental %u\n", &cur_fund) > 0)
	      next_fundamental = TRUE;
	  else if (feof (fp))
	    break;
	  j++;
	} while (next_fundamental == FALSE);
      fscanf (fp, "\n");
    }
  setlocale (LC_NUMERIC, last_locale);
  g_free (last_locale);
  fclose (fp);
}

/**
 * Creates a new Sound Studio project.
 *
 * Since this program currently only supports loading files at
 * startup, this function should only be called at the start of the
 * program.
 */
void
new_ss_project (void)
{
  Wv_Data *second_harmonic;
  add_fund_freq ();
  add_harmonic (0);
  second_harmonic = &wv_all_freqs->d[0].harmonics->d[0];
  add_wv_editor (0, 0, second_harmonic);
}

/**
 * Renders a composite waveform.
 *
 * Renders a waveform composed of all the fundamental frequency sets.
 * @param num_samples the number of points to plot
 * @param x_max this is not actually the maximum x-axis extent, but it
 * specifies a frequency of a waveform whose entire cycle should be
 * fit into the window.  In other words, the largest x-value is equal
 * to the reciprocal of @a x_max.
 * @return a malloc'ed array of rendered points, which must be freed.
 */
double *
render_waves (unsigned num_samples, float x_max)
{
  double *ypts;
  unsigned i;

  ypts = (double *) g_malloc (sizeof (double) * num_samples);

  for (i = 0; i < num_samples; i++)
    ypts[i] = 0.0; /* Don't use memset ().  That will not set the
		      actual value to "0.0".  */

  for (i = 0; i < wv_all_freqs->len; i++)
    plot_waveform (num_samples, x_max, ypts, i, TRUE);

  max_ypt = 0;
  for (i = 0; i < num_samples; i++)
    max_ypt = MAX(ABS(ypts[i]), max_ypt);

  return ypts;
}

/**
 * Plots a single fundamental frequency set.
 *
 * @param num_samples the number of points to plot
 * @param x_max this is not actually the maximum x-axis extent, but it
 * specifies a frequency of a waveform whose entire cycle should be
 * fit into the window.  In other words, the largest x-value is equal
 * to the reciprocal of @a x_max.
 * @param ypts the array that will hold the plotted points, which must
 * be sufficiently allocated
 * @param fund_freq the fundamental frequency set to work with
 * @param screen_render if TRUE, chooses the starting sample one value
 * higher than normal in order to render a full waveform in a
 * "pixel-perfect" manner.
 */
void
plot_waveform (unsigned num_samples, float x_max, double *ypts,
	       unsigned fund_freq, gboolean screen_render)
{
  unsigned i;
  unsigned j;
  unsigned ofs; /* offset */

  if (screen_render == TRUE)
    ofs = 1;
  else
    ofs = 0;

  for (i = 0; i < num_samples; i++)
    {
      float freq_mult;
      freq_mult = wv_all_freqs->d[fund_freq].fund_freq / x_max;
      ypts[i] += sin ((double) (i + ofs) / num_samples * 2 * G_PI *
		      freq_mult) * wv_all_freqs->d[fund_freq].amplitude;
      for (j = 0; j < wv_all_freqs->d[fund_freq].harmonics->len; j++)
	{
	  unsigned harmc_num;
	  harmc_num = wv_all_freqs->d[fund_freq].harmonics->d[j].harmc_num;
	  ypts[i] += sin ((double) (i + ofs) * harmc_num / num_samples *
			  2 * G_PI * freq_mult) *
	    wv_all_freqs->d[fund_freq].harmonics->d[j].amplitude;
	}
    }
}

/**
 * Adjusts the maximum value of the composite waveform.
 *
 * Multiplies all waveform amplitudes by a calculated constant so that
 * the maximum displacement of the composite waveform is equal to
 * @a new_amplitude.
 * @param new_amplitude the new maximum displacement (amplitude)
 * @param last_dialog the window which an error message dialog should
 * be overlayed on
 */
void
mult_amplitudes (float new_amplitude, GtkWidget * last_dialog)
{
  unsigned i;
  double max_height;
  unsigned *num_samples;
  unsigned total_samples;
  max_height = 0;

  /* Find the amplitude (maximum displacement) */
  /* To do that for a fundamental set, try every critical point of the
     highest frequency wave of the fundamental set.  */
  /* For non-harmonic frequencies, find the maximum of each
     fundamental set and add those maximums to find the maximum
     displacement.  */

  /* If there are any fundamental sets which are actually harmonics
     of each other, that is an error on the user's part.  */
  {
    gboolean fund_set_error;
    fund_set_error = FALSE;
    for (i = 0; i < wv_all_freqs->len - 1; i++)
      {
	unsigned j;
	for (j = i + 1; j < wv_all_freqs->len; j++)
	  {
	    double quotient;
	    double whole_num;
	    quotient = (wv_all_freqs->d[i].fund_freq /
			wv_all_freqs->d[j].fund_freq);
	    if (modf (quotient, &whole_num) == 0.0)
	      {
		fund_set_error = TRUE;
		break;
	      }
	  }
      }

    if (fund_set_error == TRUE)
      {
	GtkWidget *dialog;
	gchar *msg_string;
	msg_string = _("You must not have any fundamental frequencies " \
		       "that are harmonics of each other.");
	dialog = gtk_message_dialog_new (GTK_WINDOW (last_dialog),
			 GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR,
			 GTK_BUTTONS_CLOSE, msg_string);
	gtk_window_set_title (GTK_WINDOW (dialog), _("User error"));
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	return;
      }
  }

  /* Make sure that the total number of samples do not get too big.
     Right now, I just have to use brute force to compute the maximum
     displacement of a harmonic series because I haven't yet taken the
     time to learn the math on how to use Laplace transforms or the
     like.  */
  num_samples = (unsigned *) g_malloc (sizeof (unsigned) * wv_all_freqs->len);
  total_samples = 0;
  for (i = 0; i < wv_all_freqs->len; i++)
    {
      unsigned max_harmonic;
      unsigned j;
      max_harmonic = 1;
      for (j = 0; j < wv_all_freqs->d[i].harmonics->len; j++)
	{
	  max_harmonic = MAX (wv_all_freqs->d[i].harmonics->d[j].harmc_num,
			      max_harmonic);
	}
      /* Just use an arbitrary brute force number pick.  */
      num_samples[i] = max_harmonic * 200;
      total_samples += num_samples[i];
    }

  if (total_samples > 1000)
    {
      GtkWidget *dialog;
      gchar *str_buffer;
      gchar *msg_string;
      /* Translators: this message is only shown if the total number
	 of points exceeds 1000.  */
      msg_string = _("Are you sure you want to sample %u points?");
      str_buffer = (gchar *) g_malloc (strlen (msg_string) + 10 + 1);
      sprintf (str_buffer, msg_string, total_samples);
      dialog = gtk_message_dialog_new (GTK_WINDOW (last_dialog),
			       GTK_DIALOG_DESTROY_WITH_PARENT,
			       GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
			       str_buffer);
      gtk_window_set_title (GTK_WINDOW (dialog), _("Confirm"));
      if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_NO)
	{
	  gtk_widget_destroy (dialog);
	  g_free (str_buffer);
	  g_free (num_samples);
	  return;
	}
      gtk_widget_destroy (dialog);
      g_free (str_buffer);
    }

  /* Compute the height at the peaks.  */
  for (i = 0; i < wv_all_freqs->len; i++)
    {
      unsigned j;
      float x_max;
      double *ypts;

      x_max = (float) wv_all_freqs->d[i].fund_freq;
      ypts = (double *) g_malloc (sizeof (double) * num_samples[i]);

      for (j = 0; j < num_samples[i]; j++)
	ypts[j] = 0.0; /* Don't use memset ().  That will not set the
			  actual value to "0.0".  */

      plot_waveform (num_samples[i], x_max, ypts, i, FALSE);

      max_ypt = 0;
      for (j = 0; j < num_samples[i]; j++)
	max_ypt = MAX(ABS(ypts[j]), max_ypt);
      g_free (ypts);
      max_height += max_ypt;
    }
  g_free (num_samples);

  /* We are ready to normalize the amplitudes.  */
  {
    float amp_mult_factor;
    amp_mult_factor = (float)(new_amplitude / max_height);
    for (i = 0; i < wv_all_freqs->len; i++)
      {
	unsigned j;
	wv_all_freqs->d[i].amplitude *= amp_mult_factor;
	for (j = 0; j < wv_all_freqs->d[i].harmonics->len; j++)
	  {
	    wv_all_freqs->d[i].harmonics->d[j].amplitude *=
	      amp_mult_factor;
	  }
      }
  }

  /* Update the user interface */
  unselect_fund_freq (g_fund_set);
  select_fund_freq (g_fund_set);
}
