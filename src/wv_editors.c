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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <locale.h>
#include <string.h>

#include <gtk/gtk.h>

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
Wv_Fund_Freq_array *wv_all_freqs = NULL;

/** Current fundamental set.  */
unsigned g_fund_set = 0;

/** Keeps track of whether the fundamental set selection combo box
    was initialized or not.  */
static gboolean combo_init = FALSE;

/* Even though the original application was planned to have a command
   called '1st Harmonic Drop', it turns out that the original idea is
   only possible if all higher harmonics are also harmonics of each
   other.  The idea was to remove the fundamental frequency and change
   the first harmonic to be the new fundamental frequency.  */

/**
 * Initializes ::wv_all_freqs.
 *
 * This function should only be called whenever a new file is loaded.
 * Since Slider currently only supports loading one file at a time,
 * this function is only called during program startup.
 */
void
init_wv_editors (void)
{
  wv_all_freqs = (Wv_Fund_Freq_array *)
    g_array_new (FALSE, FALSE, sizeof (Wv_Fund_Freq));
  g_fund_set = 0;
  combo_init = FALSE;
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
  if (cb_fund_set != NULL)
    {
      for (i = 0; i < wv_all_freqs->len; i++)
	gtk_combo_box_remove_text (GTK_COMBO_BOX (cb_fund_set), 0);
    }
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
  wv_all_freqs = NULL;
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
 * given data.  The coresponding widgets are not created until
 * create_wvedit_holder() is called.
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
	  cur_editor->amp_sliders->d[j]->parent_index = i;
      }
  }
}

/**
 * Deletes a wave editor window.
 *
 * Any associated GTK+ widgets will be destroyed when the wave editor
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
	  cur_editor->amp_sliders->d[j]->parent_index = i;
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
 * @param fund_freq the fundamental frequency set to work with
 * @param index zero-based index of the harmonic to remove
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
      /* Remove all editor windows.  */
      while (wv_all_freqs->d[fund_freq].wv_editors->len > 0)
	    remove_wv_editor (fund_freq, 0);
    }

  /* Recalculate group indices as necessary.  */
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
  wv_all_freqs->d[index].phase_pos = 0.0;
  wv_all_freqs->d[index].fund_editor.widget = NULL;
  wv_all_freqs->d[index].harmonics = (Wv_Data_array *)
    g_array_new (FALSE, FALSE, sizeof (Wv_Data));

  /* Initialize the fundamental frequency editor window.  */
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
  /* Destroy the members of the wave editors array first.  */
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

  if (!combo_init)
    {
      combo_init = TRUE;
      /* Add combo box entries.  */
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

  if (!sliders_init)
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
  /* Destroy all the editor widgets.  */
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
 * Saves a Slider Wave Editor project file.
 *
 * @param filename the file name to save to
 * @return TRUE if save was successful, FALSE otherwise
 */
gboolean
save_sliw_project (char *filename)
{
  FILE *fp;
  char *locale_temp;
  char *last_locale;

  fp = fopen (filename, "w");
  if (fp == NULL)
    goto error;

  fputs (
"# This is a Slider Wave Editor project file.  Comments are only\n"
"# allowed at the beginning of a project file, and they are not\n"
"# preserved during file loading and saving in Slider.\n"
"#\n"
"# A harmonic is specified as a pair of numbers.  The first number is\n"
"# the harmonic number, and the second is the amplitude.\n", fp);

  /* The project file's contents are written in English to prevent
     compatibility problems with Slider running in different
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
  if (fclose (fp) == EOF)
    goto error;
  return TRUE;

 error:
  {
    GtkWidget *dialog;
    dialog = gtk_message_dialog_new_with_markup
      (GTK_WINDOW (main_window),
       GTK_DIALOG_DESTROY_WITH_PARENT,
       GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
       _("<b><big>An error occurred while saving your " \
	 "file.</big></b>\n\n%s"),
       strerror(errno));
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    return FALSE;
  }
}

/**
 * Exports a Slider Wave Editor project.
 *
 * The exported file is represented as a series of waveforms defined
 * only by frequency and amplitude in the format of a Nyquist Lisp
 * script.  This function is intended to be used as an efficient way
 * to import Slider sounds into other applications.
 * @param filename the name of the file to export to
 */
void
export_sliw_project (char *filename)
{
  FILE* fp;
  char *locale_temp;
  char *last_locale;

  fp = fopen (filename, "w");
  if (fp == NULL)
    goto error;

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
  if (fclose (fp) == EOF)
    goto error;
  return;

 error:
  {
    GtkWidget *dialog;
    dialog = gtk_message_dialog_new_with_markup
      (GTK_WINDOW (main_window),
       GTK_DIALOG_DESTROY_WITH_PARENT,
       GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
       _("<b><big>An error occurred while exporting your " \
	 "file.</big></b>\n\n%s"),
       strerror(errno));
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    return;
  }
}

/**
 * Loads a Slider Wave Editor project file.
 *
 * Opens, reads, and parses a Slider project file.  Slider project
 * files are plain text files, typically saved with the .txt
 * extension.  Note that parse error checking in this function is
 * minimal, and non well-formed documents can result in false
 * successful return status with incorrect data.  @param filename the
 * name of the file to load @return TRUE on successful load, FALSE on
 * error.
 */
gboolean
load_sliw_project (char *filename)
{
  gboolean retval = FALSE;
  FILE *fp;
  char *locale_temp;
  char *last_locale;
  unsigned cur_fund;

  fp = fopen (filename, "r");
  if (fp == NULL)
    {
      GtkWidget *dialog;
      dialog = gtk_message_dialog_new_with_markup
	(NULL,
	 GTK_DIALOG_DESTROY_WITH_PARENT,
	 GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
	 _("<b><big>Your file could not be opened.</big></b>\n\n" \
	   "%s"),
	 strerror(errno));
      gtk_window_set_title (GTK_WINDOW (dialog), _("Slider Wave Editor"));
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      return FALSE;
    }

  /* The project file's contents are written in English to prevent
     compatibility problems with Slider running in different
     languages.  */
  locale_temp = setlocale (LC_NUMERIC, NULL);
  last_locale = (char *) g_malloc (strlen (locale_temp) + 1);
  strcpy (last_locale, locale_temp);
  setlocale (LC_NUMERIC, "C");

  { /* First skip the comments.  */
    int ch;
    while ((ch = getc (fp)) == '#')
      while ((ch = getc (fp)) != EOF && ch != '\n');
    if (ch == EOF)
      goto cleanup;
  }

  if (fscanf (fp, "\nFundamental %u\n", &cur_fund) != 1)
    goto cleanup;
  while (!feof (fp))
    {
      unsigned i, j;
      gboolean next_fundamental;
      char test_buf[11];
      i = cur_fund - 1;
      add_fund_freq ();
      add_wv_editor (wv_all_freqs->len - 1, 0,
		     &wv_all_freqs->d[i].harmonics->d[0]);
      if (fscanf (fp, "Frequency: %g\n", &wv_all_freqs->d[i].fund_freq) != 1 ||
	  fscanf (fp, "Amplitude: %g\n", &wv_all_freqs->d[i].amplitude) != 1)
	goto cleanup;
      if (fscanf (fp, "%10c", test_buf) != 1)
	goto cleanup;
      test_buf[10] = '\0';
      if (strcmp(test_buf, "Harmonics:"))
	goto cleanup;

      j = 0;
      next_fundamental = FALSE;
      do
	{
	  int scan_status;
	  add_harmonic (i);
	  scan_status = fscanf (fp, " %u, %g;",
				&wv_all_freqs->d[i].harmonics->d[j].harmc_num,
				&wv_all_freqs->d[i].harmonics->d[j].amplitude);
	  if (scan_status != 2) /* No harmonics read */
	    remove_harmonic (i, wv_all_freqs->d[i].harmonics->len - 1);
	  if (fscanf (fp, "\nFundamental %u\n", &cur_fund) == 1)
	      next_fundamental = TRUE;
	  else if (feof (fp))
	    break;
	  j++;
	} while (!next_fundamental);
      fscanf (fp, "\n");
    }
  retval = TRUE;
 cleanup:
  if (!retval)
    {
      GtkWidget *dialog;
      dialog = gtk_message_dialog_new_with_markup
	(NULL,
	 GTK_DIALOG_DESTROY_WITH_PARENT,
	 GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
	 _("<b><big>A syntax error was found in your file.</big></b>\n\n" \
	   "A blank template will be loaded instead."));
      gtk_window_set_title (GTK_WINDOW (dialog), _("Slider Wave Editor"));
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      return FALSE;
    }
  setlocale (LC_NUMERIC, last_locale);
  g_free (last_locale);
  fclose (fp);
  return retval;
}

/**
 * Creates a new Slider Wave Editor project.
 *
 * Since this program currently only supports loading files at
 * startup, this function should only be called at the start of the
 * program.
 */
void
new_sliw_project (void)
{
  Wv_Data *second_harmonic;
  add_fund_freq ();
  add_harmonic (0);
  second_harmonic = &wv_all_freqs->d[0].harmonics->d[0];
  add_wv_editor (0, 0, second_harmonic);
}

/**
 * Calculates the maximum frequency extent of the interesting parts of
 * a composite waveform.
 *
 * This function calculates the reciprocal of the maximum time extent
 * needed to view the interesting parts of the current sound effect in
 * ::wv_all_freqs.
 * @return the reciprocal of the time extent
 */
float calc_freq_extent (void)
{
  unsigned i;
  float most_fnd_frq;
  float next_most_fnd_frq;
  float least_fnd_frq;
  float freq_ext;

  /* Find the highest fundamental frequency.  */
  most_fnd_frq = wv_all_freqs->d[0].fund_freq;
  for (i = 0; i < wv_all_freqs->len; i++)
    most_fnd_frq = MAX (wv_all_freqs->d[i].fund_freq, most_fnd_frq);
  /* Find the next highest fundamental frequency.  */
  next_most_fnd_frq = wv_all_freqs->d[0].fund_freq;
  for (i = 0; i < wv_all_freqs->len; i++)
    {
      if (most_fnd_frq != wv_all_freqs->d[i].fund_freq)
	{
	  if (next_most_fnd_frq == most_fnd_frq)
	    next_most_fnd_frq = wv_all_freqs->d[i].fund_freq;
	  else
	    next_most_fnd_frq = MAX (wv_all_freqs->d[i].fund_freq,
				      next_most_fnd_frq);
	}
    }
  /* If two different fundamental frequencies happen to be equal, set
     next_most_fnd_frq to zero.  */
  if (next_most_fnd_frq == most_fnd_frq)
    next_most_fnd_frq = 0;
  /* Find the lowest fundamental frequency.  */
  least_fnd_frq = wv_all_freqs->d[0].fund_freq;
  for (i = 0; i < wv_all_freqs->len; i++)
    least_fnd_frq = MIN (wv_all_freqs->d[i].fund_freq, least_fnd_frq);

  freq_ext = most_fnd_frq - next_most_fnd_frq;
  /* When the frequency difference is greater than 100%, then the
     display window should only reach to the extent of the component
     with the longest wavelength.  */
  if (freq_ext / next_most_fnd_frq > 1.00)
    freq_ext = least_fnd_frq;
  return freq_ext;
}

/**
 * Renders a composite waveform.
 *
 * Renders a waveform composed of all the fundamental frequency sets.
 * @param ypts the array that will hold the rendered samples, which
 * must be sufficiently allocated
 * @param num_samples the number of points to plot
 * @param x_max the maximum x-axis extent for rendering
 * @return a malloc'ed array of rendered points, which must be freed.
 */
void
render_waves (float * ypts, unsigned num_samples, float x_max)
{
  float pre_max_ypt = 0.0;
  unsigned i;
  for (i = 0; i < num_samples; i++)
    ypts[i] = 0.0; /* Don't use memset ().  That will not set the
		      actual value to "0.0".  */

  for (i = 0; i < wv_all_freqs->len; i++)
    plot_waveform (ypts, num_samples, x_max, i, 1);

  for (i = 0; i < num_samples; i++)
    pre_max_ypt = MAX(ABS(ypts[i]), pre_max_ypt);
  max_ypt = pre_max_ypt;
}

/**
 * Plots a single fundamental frequency set.
 *
 * @param ypts the array that will hold the plotted points, which must
 * be sufficiently allocated.  The plotted waveform will be added to
 * the current values.
 * @param num_samples the number of points to plot
 * @param x_max the maximum x-axis extent for rendering
 * @param fund_freq_idx the fundamental frequency set to work with
 * @param ofs offset in samples from the beginning of the sine wave
 * cycle
 */
void
plot_waveform (float * ypts, unsigned num_samples, float x_max,
	       unsigned fund_freq_idx, unsigned ofs)
{
  float inv_num_samp = 1.0 / num_samples;
  float fund_freq = wv_all_freqs->d[fund_freq_idx].fund_freq;
  float fund_amplitude = wv_all_freqs->d[fund_freq_idx].amplitude;
  Wv_Data *harmonics = wv_all_freqs->d[fund_freq_idx].harmonics->d;
  unsigned num_harmonics = wv_all_freqs->d[fund_freq_idx].harmonics->len;
  unsigned i;
  unsigned j;

  for (i = 0; i < num_samples; i++)
    {
      float freq_mult;
      freq_mult = fund_freq * x_max;
      ypts[i] += sinf ((float) (i + ofs) * inv_num_samp * 2 * G_PI *
		       freq_mult) * fund_amplitude;
      for (j = 0; j < num_harmonics; j++)
	{
	  unsigned harmc_num = harmonics[j].harmc_num;
	  ypts[i] += sinf ((float) (i + ofs) * harmc_num * inv_num_samp *
			   2 * G_PI * freq_mult) *
	    harmonics[j].amplitude;
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
  float max_height;
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
  /* NOTE: The current calculation mechanism does not require this to
     be true anymore.  */
  /* {
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

    if (fund_set_error)
      {
	GtkWidget *dialog;
	gchar *msg_string;
	msg_string = _("You must not have any fundamental frequencies " \
		       "that are harmonics of each other.");
	dialog = gtk_message_dialog_new (GTK_WINDOW (last_dialog),
			 GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR,
			 GTK_BUTTONS_CLOSE, msg_string);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	return;
      }
  } */

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
      dialog = gtk_message_dialog_new_with_markup
	(GTK_WINDOW (last_dialog),
	 GTK_DIALOG_DESTROY_WITH_PARENT,
	 GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
	 _("<b><big>Are you sure you want to sample %u " \
	   "points?</big></b>\n\nThis may take a while, and Slider " \
	   "will be unresponsive until the computation completes."),
	 total_samples);
      if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_NO)
	{
	  gtk_widget_destroy (dialog);
	  g_free (num_samples);
	  return;
	}
      gtk_widget_destroy (dialog);
    }

  /* Compute the height at the peaks.  */
  for (i = 0; i < wv_all_freqs->len; i++)
    {
      unsigned j;
      float x_max;
      float *ypts;

      x_max = 1.0 / wv_all_freqs->d[i].fund_freq;
      ypts = (float *) g_malloc (sizeof (float) * num_samples[i]);

      for (j = 0; j < num_samples[i]; j++)
	ypts[j] = 0.0; /* Don't use memset ().  That will not set the
			  actual value to "0.0".  */

      plot_waveform (ypts, num_samples[i], x_max, i, 0);

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

  /* Update the user interface.  */
  unselect_fund_freq (g_fund_set);
  select_fund_freq (g_fund_set);
}
