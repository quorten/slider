/* Definitions of GTK+ widget signal handlers.

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
 * Definitions of GTK+ widget signal handlers.
 *
 * Most of the functions in this file are not deserving of very heavy
 * documentation.  All of the signal handlers are connected to their
 * signals in interface.c, and the function calling form of each
 * signal handler is dictated by the signal that it is connected to.
 * Therefore, it is mostly unnecessary to document the signal
 * handlers.  However, the parameter @a user_data is documented
 * whenever it is expected to be a non-NULL value in any of the signal
 * handlers.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include <math.h>
#include <string.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include "wv_editors.h"

static const gchar *mult_dlg_text; /* Stores the number entered this dialog */
/** Contains the maximum y-point that is set during wave rendering.  */
double max_ypt;

/**
 * Signal handler for the "expose_event" event sent to ::wave_render.
 *
 * This signal handler renders the waveform within the drawable area.
 */
gboolean
wavrnd_expose (GtkWidget * widget, GdkEventExpose * event, gpointer user_data)
{
  unsigned i;
  gint last_ypt, win_width, win_height;
  GdkGC *gc;
  GdkColor color;
  double *ypts;
  float most_fnd_frq;
  float next_most_fnd_frq;
  float least_fnd_frq;

  /* At the start of an expose handler, a clip region of event->area
   * is set on the window, and event->area has been cleared to the
   * widget's background color. The docs for
   * gdk_window_begin_paint_region() give more details on how this
   * works.
   */

  /* It would be a bit more efficient to keep these
   * GC's around instead of recreating on each expose, but
   * this is the lazy/slow way.
   */

  gc = gdk_gc_new (widget->window);
  color.red = 0x0000;
  color.green = 0x0000;
  color.blue = 0x0000;
  gdk_gc_set_rgb_fg_color (gc, &color);

  /* Find the highest fundamental frequency */
  most_fnd_frq = wv_all_freqs->d[0].fund_freq;
  for (i = 0; i < wv_all_freqs->len; i++)
    most_fnd_frq = MAX (wv_all_freqs->d[i].fund_freq, most_fnd_frq);
  /* Find the next highest fundamental frequency */
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
  /* Find the lowest fundamental frequency */
  least_fnd_frq = wv_all_freqs->d[0].fund_freq;
  for (i = 0; i < wv_all_freqs->len; i++)
    least_fnd_frq = MIN (wv_all_freqs->d[i].fund_freq, least_fnd_frq);

  /* If there is an interference pattern, you should be able to scroll
     the x-axis up to one second in time.  */

  /* Set the default zoom so that the highest frequency harmonic's
     wavelength is at least 20 pixels wide.  */

  win_width = widget->allocation.width;
  win_height = widget->allocation.height;
  max_ypt = 0;
  {
    float xmax;
    xmax = most_fnd_frq - next_most_fnd_frq;
    /* When the frequency difference is greater than 100%, then the
       display window should only reach to the extent of the component
       with the longest wavelength.  */
    if (xmax / next_most_fnd_frq > 1.00)
      xmax = least_fnd_frq;
    ypts = render_waves (win_width, xmax);
  }

  last_ypt = win_height / 2;
  /* NB: widget->allocation.width >= 1 */
  for (i = 0; i < (unsigned) win_width; i++)
    {
      gint ypt;
      ypt = (gint) (ypts[i] / max_ypt * win_height / 2);
      ypt = win_height / 2 - ypt;
      if (last_ypt < ypt)
	last_ypt++;
      else if (last_ypt > ypt)
	last_ypt--;
      gdk_draw_line (widget->window, gc, i, last_ypt, i, ypt);
      last_ypt = ypt;
    }

  g_free (ypts);
  g_object_unref (gc);

  /* return TRUE because we've handled this event, so no
   * further processing is required.
   */
  return TRUE;
}

void
b_save_clicked (GtkButton * button, gpointer user_data)
{
  GtkWidget *dialog;

  dialog = gtk_file_chooser_dialog_new (_("Save File"),
					GTK_WINDOW (main_window),
					GTK_FILE_CHOOSER_ACTION_SAVE,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
					NULL);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
      char *filename;

      filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
      save_ss_project (filename);
      g_free (filename);
    }

  gtk_widget_destroy (dialog);
}

void
b_export_clicked (GtkButton * button, gpointer user_data)
{
  GtkWidget *dialog;

  dialog = gtk_file_chooser_dialog_new (_("Export File"),
					GTK_WINDOW (main_window),
					GTK_FILE_CHOOSER_ACTION_SAVE,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
					NULL);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
      char *filename;

      filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
      export_ss_project (filename);
      g_free (filename);
    }

  gtk_widget_destroy (dialog);
}

void
b_play_clicked (GtkButton * button, gpointer user_data)
{
  GtkWidget *dialog;
  dialog = gtk_message_dialog_new (GTK_WINDOW (main_window),
				   GTK_DIALOG_DESTROY_WITH_PARENT,
				   GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
			   _("Sorry, but playback is not yet supported."));
  gtk_window_set_title (GTK_WINDOW (dialog), _("Not implemented"));
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

void
b_stop_clicked (GtkButton * button, gpointer user_data)
{
  GtkWidget *dialog;
  dialog = gtk_message_dialog_new (GTK_WINDOW (main_window),
				   GTK_DIALOG_DESTROY_WITH_PARENT,
				   GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
			   _("Sorry, but playback is not yet supported."));
  gtk_window_set_title (GTK_WINDOW (dialog), _("Not implemented"));
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

/**
 * Fundamental set combo box signal handler.
 *
 * When the user selects a different fundamental set, all of the
 * widgets related to the current fundamental set are destroyed and
 * new widgets are created for the fundamental set that they have
 * selected.
 */
void
cb_fund_set_changed (GtkComboBox * combobox, gpointer user_data)
{
  /* We must make sure that we only process this signal when the user
     clicks in the combo box, not when the combobox gets changed
     automatically.  */
  unsigned fund_set_test;
  fund_set_test = gtk_combo_box_get_active (combobox);
  if (fund_set_test >= wv_all_freqs->len)
    return;
  unselect_fund_freq (g_fund_set);
  g_fund_set = fund_set_test;
  select_fund_freq (g_fund_set);
}

/**
 * Signal handler sent when ::main_window is resized.
 *
 * Even though this signal handler is supposed to be sent whenever the
 * main window is resized, there are some circumstances when this
 * signal fails to get sent, at least on Windows.  Since this is a
 * signal handler for the configure event, this signal handler also
 * gets sent when the window is moved but not resized.  These are
 * known bugs.
 */
gboolean
wv_edit_div_configure (GtkWidget * widget,
		       GdkEventConfigure * event, gpointer user_data)
{
  /* Set the divider position to preserve the previous ratio.
     If this is the first time, set the ratio to a predefined value.  */
  static gboolean div_set = FALSE;
  static gfloat div_ratio;
  static gint last_win_height;

  if (div_set == FALSE)
    {
      div_ratio = 0.5f;
      div_set = TRUE;
    }
  else
    {
      gint div_pos;
      div_pos = gtk_paned_get_position (GTK_PANED (widget));
      div_ratio = (float) div_pos / last_win_height;
    }

  gtk_paned_set_position (GTK_PANED (widget),
			  (gint) (widget->allocation.height * div_ratio));
  last_win_height = widget->allocation.height;

  return FALSE;
}

/**
 * "1st Harmonic Drop" signal handler.
 *
 * This function will never get called since this feature is not yet
 * supported.
 */
void
harmc_one_drop_clicked (GtkButton * button, gpointer user_data)
{
}

void
mult_amps_clicked (GtkButton * button, gpointer user_data)
{
  GtkWidget *dialog;
  dialog = create_mult_amps_dialog ();
  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
      float new_amplitude;
      if (sscanf(mult_dlg_text, "%g", &new_amplitude) > 0)
	mult_amplitudes (new_amplitude, dialog);
    }
  gtk_widget_destroy (dialog);
}

void
fundset_add_clicked (GtkButton * button, gpointer user_data)
{
  unsigned new_fund;
  Wv_Data *second_harmonic;
  add_fund_freq ();
  new_fund = wv_all_freqs->len - 1;
  add_harmonic (new_fund);
  second_harmonic = &wv_all_freqs->d[new_fund].harmonics->d[0];
  add_wv_editor (new_fund, 0, second_harmonic);
  /* Select the new fundamental set */
  unselect_fund_freq (g_fund_set);
  g_fund_set = new_fund;
  select_fund_freq (g_fund_set);
  /* Add a combo box entry */
  {
    unsigned cur_fund;
    gchar cb_text[11];
    cur_fund = wv_all_freqs->len;
    sprintf (cb_text, "%u", cur_fund);
    gtk_combo_box_append_text (GTK_COMBO_BOX (cb_fund_set), cb_text);
    gtk_combo_box_set_active (GTK_COMBO_BOX (cb_fund_set), new_fund);
  }

  gtk_widget_queue_draw (wave_render);
}

void
fundset_remove_clicked (GtkButton * button, gpointer user_data)
{
  unselect_fund_freq (g_fund_set);
  remove_fund_freq (g_fund_set);
  /* Always remove the last label so that there are not any numerical
     jumps which don't actually exist in the internal data.  */
  gtk_combo_box_remove_text (GTK_COMBO_BOX (cb_fund_set), wv_all_freqs->len);
  if (g_fund_set > 0)
    g_fund_set--;
  else
    g_fund_set = 0;
  select_fund_freq (g_fund_set);
  gtk_combo_box_set_active (GTK_COMBO_BOX (cb_fund_set), g_fund_set);

  gtk_widget_queue_draw (wave_render);
}

/**
 * Signal handler for when the user hits enter in the fundamental
 * frequency edit box.
 *
 * This function updates the internal value of the fundamental
 * frequency.  */
void
fndfrq_mntisa_activate (GtkEntry * entry, gpointer user_data)
{
  Wv_Editor_Data *cur_data;
  cur_data = &wv_all_freqs->d[g_fund_set].fund_editor;
  wv_all_freqs->d[g_fund_set].fund_freq =
    sci_notation_get_value (entry, GTK_SPIN_BUTTON (cur_data->fndfrq_exp));

  gtk_widget_queue_draw (wave_render);
}

/**
 * Signal handler for when the user navigates away from the
 * fundamental frequency edit box.
 *
 * This function updates the internal value of the fundamental
 * frequency.
 */
gboolean
fndfrq_mntisa_focus_out (GtkEntry * entry,
			 GdkEventFocus * event, gpointer user_data)
{
  fndfrq_mntisa_activate (entry, user_data);
  return FALSE;
}

/**
 * Signal handler for when the user changes the exponent of the
 * fundamental frequency.
 *
 * This function updates the internal value of the fundamental
 * frequency.
 */
void
fndfrq_exp_value_changed (GtkSpinButton * spinbutton, gpointer user_data)
{
  Wv_Editor_Data *cur_data;
  cur_data = &wv_all_freqs->d[g_fund_set].fund_editor;
  wv_all_freqs->d[g_fund_set].fund_freq =
    sci_notation_get_value (GTK_ENTRY (cur_data->fndfrq_mntisa), spinbutton);

  gtk_widget_queue_draw (wave_render);
}

/**
 * Signal handler for when the user hits enter in the fundamental
 * amplitude edit box.
 *
 * This function updates the internal value of the fundamental
 * amplitude.
 */
void
fndamp_mntisa_activate (GtkEntry * entry, gpointer user_data)
{
  Wv_Editor_Data *cur_data;
  cur_data = &wv_all_freqs->d[g_fund_set].fund_editor;
  wv_all_freqs->d[g_fund_set].amplitude =
    sci_notation_get_value (entry, GTK_SPIN_BUTTON (cur_data->amp_exp));

  gtk_widget_queue_draw (wave_render);
}

/**
 * Signal handler for when the user navigates away from the
 * fundamental amplitude edit box.
 *
 * This function updates the internal value of the fundamental
 * amplitude.
 */
gboolean
fndamp_mntisa_focus_out (GtkEntry * entry,
			 GdkEventFocus * event, gpointer user_data)
{
  fndamp_mntisa_activate (entry, user_data);
  return FALSE;
}

/**
 * Signal handler for when the user changes the exponent of the
 * fundamental amplitude.
 *
 * This function updates the internal value of the fundamental
 * amplitude.
 */
void
fndamp_exp_value_changed (GtkSpinButton * spinbutton, gpointer user_data)
{
  Wv_Editor_Data *cur_data;
  cur_data = &wv_all_freqs->d[g_fund_set].fund_editor;
  wv_all_freqs->d[g_fund_set].amplitude =
    sci_notation_get_value (GTK_ENTRY (cur_data->amp_mntisa), spinbutton);

  gtk_widget_queue_draw (wave_render);
}

/**
 * Signal handler called when the selected harmonic is changed.
 *
 * @param user_data pointer to the parent wave editor window structure
 * within <code>wv_all_freqs->d[g_fund_set].wv_editors</code>
 */
void
harmc_sel_changed (GtkComboBox * combobox, gpointer user_data)
{
  Wv_Editor_Data *cur_editor;
  gint new_sel;
  cur_editor = (Wv_Editor_Data *) user_data;
  new_sel = gtk_combo_box_get_active (combobox);
  cur_editor->data = &wv_all_freqs->d[g_fund_set].harmonics->d[new_sel];

  sci_notation_set_values (GTK_ENTRY (cur_editor->amp_mntisa),
			   GTK_SPIN_BUTTON (cur_editor->amp_exp),
			   cur_editor->data->amplitude);
}

/**
 * Signal handler called when the add harmonic button is clicked.
 *
 * @param user_data pointer to the parent wave editor window structure
 * within <code>wv_all_freqs->d[g_fund_set].wv_editors</code>
 */
void
harmc_add_clicked (GtkButton * button, gpointer user_data)
{
  Wv_Editor_Data *cur_data;
  cur_data = (Wv_Editor_Data *) user_data;
  add_harmonic (g_fund_set);
  {
    gchar cb_text[11];
    unsigned i;
    unsigned num_harmcs;
    num_harmcs = wv_all_freqs->d[g_fund_set].harmonics->len;
    sprintf (cb_text, "%u",
	wv_all_freqs->d[g_fund_set].harmonics->d[num_harmcs-1].harmc_num);
    for (i = 0; i < wv_all_freqs->d[g_fund_set].wv_editors->len; i++)
      {
	gtk_combo_box_append_text (GTK_COMBO_BOX
	   (wv_all_freqs->d[g_fund_set].wv_editors->d[i]->harmc_sel), cb_text);
      }
    gtk_combo_box_set_active (GTK_COMBO_BOX (cur_data->harmc_sel),
			      wv_all_freqs->d[g_fund_set].harmonics->len - 1);
  }

  gtk_widget_queue_draw (wave_render);
}

/**
 * Signal handler called when the remove harmonic button is clicked.
 *
 * @param user_data pointer to the parent wave editor window structure
 * within <code>wv_all_freqs->d[g_fund_set].wv_editors</code>
 */
void
harmc_remove_clicked (GtkButton * button, gpointer user_data)
{
  Wv_Editor_Data *cur_editor;
  unsigned harmc_idx;
  unsigned swap_idx;
  unsigned i;
  cur_editor = (Wv_Editor_Data *) user_data;
  harmc_idx = cur_editor->data->group_idx;

  if (harmc_idx > 0)
    swap_idx = harmc_idx - 1;
  else
    swap_idx = 0;

  /* Note: The following if clause was added to fix a bug with memory
     reallocation corruption (MinGW GCC 3.4.5 and GTK+ 2.10.11.0).  I
     do not quite understand the predicament, but this following
     construct should be cleaned up once the reason is understood.  */
  if (wv_all_freqs->d[g_fund_set].harmonics->len != 1)
    {
      for (i = 0; i < wv_all_freqs->d[g_fund_set].wv_editors->len; i++)
	{
	  Wv_Editor_Data *iter_editor;
	  Wv_Data *iter_data;
	  iter_editor = wv_all_freqs->d[g_fund_set].wv_editors->d[i];
	  iter_data = iter_editor->data;
	  gtk_combo_box_remove_text (GTK_COMBO_BOX
				     (iter_editor->harmc_sel), harmc_idx);
	  if (iter_data->group_idx == harmc_idx)
	    gtk_combo_box_set_active (GTK_COMBO_BOX (iter_editor->harmc_sel),
				      swap_idx);
	}
    }
  remove_harmonic (g_fund_set, harmc_idx);

  gtk_widget_queue_draw (wave_render);
}

/**
 * Signal handler called when the add harmonic window button is
 * clicked.
 *
 * @param user_data pointer to the parent wave editor window structure
 * within <code>wv_all_freqs->d[g_fund_set].wv_editors</code>
 */
void
harmc_win_add_clicked (GtkButton * button, gpointer user_data)
{
  Wv_Editor_Data *cur_editor;
  unsigned new_ed_idx;
  GtkWidget *new_widget;
  cur_editor = (Wv_Editor_Data *) user_data;
  /* Add the new editor after the current editor.  */
  new_ed_idx = cur_editor->index + 1;
  add_wv_editor (g_fund_set, new_ed_idx, cur_editor->data);
  wv_all_freqs->d[g_fund_set].wv_editors->d[new_ed_idx]->widget =
    create_wvedit_holder (FALSE, new_ed_idx);
  new_widget = wv_all_freqs->d[g_fund_set].wv_editors->d[new_ed_idx]->widget;
  gtk_box_pack_start (GTK_BOX (wave_edit_cntr), new_widget, FALSE, FALSE, 0);
  /* We must not let the reordered child to go before the fundamental
     editor, so we add one to the index.  */
  gtk_box_reorder_child (GTK_BOX (wave_edit_cntr), new_widget,
			 new_ed_idx + 1);

  if (wv_all_freqs->d[g_fund_set].wv_editors->len == 2)
    {
      gtk_widget_set_sensitive ((wv_all_freqs->d[g_fund_set].wv_editors->d[0]->
				 harmc_win_rm_btn), TRUE);
    }
}

/**
 * Signal handler called when the remove harmonic window button is
 * clicked.
 *
 * @param user_data pointer to the parent wave editor window structure
 * within <code>wv_all_freqs->d[g_fund_set].wv_editors</code>
 */
void
harmc_win_remove_clicked (GtkButton * button, gpointer user_data)
{
  Wv_Editor_Data *cur_editor;
  cur_editor = (Wv_Editor_Data *) user_data;

  remove_wv_editor (g_fund_set, cur_editor->index);
  if (wv_all_freqs->d[g_fund_set].wv_editors->len == 1)
    {
      gtk_widget_set_sensitive ((wv_all_freqs->d[g_fund_set].wv_editors->d[0]->
				 harmc_win_rm_btn), FALSE);
    }
}

/**
 * Updates the value of a harmonic's amplitude.
 *
 * @param user_data pointer to the parent wave editor window structure
 * within <code>wv_all_freqs->d[g_fund_set].wv_editors</code>
 */
void
amp_mntisa_activate (GtkEntry * entry, gpointer user_data)
{
  Wv_Editor_Data *cur_editor;
  cur_editor = (Wv_Editor_Data *) user_data;
  cur_editor->data->amplitude =
    sci_notation_get_value (entry, GTK_SPIN_BUTTON (cur_editor->amp_exp));

  gtk_widget_queue_draw (wave_render);
}

/**
 * Updates the value of a harmonic's amplitude.
 *
 * @param user_data pointer to the parent wave editor window structure
 * within <code>wv_all_freqs->d[g_fund_set].wv_editors</code>
 */
gboolean
amp_mntisa_focus_out (GtkEntry * entry,
		      GdkEventFocus * event, gpointer user_data)
{
  amp_mntisa_activate (entry, user_data);
  return FALSE;
}

/**
 * Updates the value of a harmonic's amplitude.
 *
 * @param user_data pointer to the parent wave editor window structure
 * within <code>wv_all_freqs->d[g_fund_set].wv_editors</code>
 */
void
amp_exp_value_changed (GtkSpinButton * spinbutton, gpointer user_data)
{
  Wv_Editor_Data *cur_editor;
  cur_editor = (Wv_Editor_Data *) user_data;
  cur_editor->data->amplitude =
    sci_notation_get_value (GTK_ENTRY (cur_editor->amp_mntisa), spinbutton);

  gtk_widget_queue_draw (wave_render);
}

/**
 * Gets the value of a scientific notation control pair.
 *
 * @param mntisa_widget the mantissa widget
 * @param exp_widget the exponent widget
 * @return the value of the scientific notation control pair
 */
float
sci_notation_get_value (GtkEntry * mntisa_widget, GtkSpinButton * exp_widget)
{
  const gchar *mntisa_string;
  int exp_num;
  char *sci_string;
  float sci_value;
  mntisa_string = gtk_entry_get_text (mntisa_widget);
  exp_num = gtk_spin_button_get_value_as_int (exp_widget);
  sci_string = (char *) g_malloc (strlen (mntisa_string) + 11 + 1);
  sprintf (sci_string, "%se%+i", mntisa_string, exp_num);
  sscanf (sci_string, "%e", &sci_value);
  g_free (sci_string);
  return sci_value;
}

/**
 * Sets the value of a scientific notation control pair.
 *
 * @param mntisa_widget the mantissa widget
 * @param exp_widget the exponent widget
 * @param value the floating point value that the control pair should
 * represent
 */
void
sci_notation_set_values (GtkEntry * mntisa_widget,
			 GtkSpinButton * exp_widget, float value)
{
  char sci_string[64];
  char *e_pos; /* Position of the letter 'e' in the scientific
		  notation string */
  int exp_num;
  sprintf (sci_string, "%e", value);
  e_pos = strchr (sci_string, (int)'e');
  *e_pos = '\0'; /* Break the string in half.  */
  e_pos++;
  sscanf (e_pos, "%i", &exp_num);
  gtk_entry_set_text (mntisa_widget, sci_string);
  gtk_spin_button_set_value (exp_widget, (gdouble) exp_num);
}

/**
 * Adds a precision slider to a precision slider group.
 * @param user_data a pointer to the Slide_Data structure
 */
void
precslid_add_clicked (GtkButton * button, gpointer user_data)
{
  Slide_Data *cur_slider;
  Wv_Editor_Data *cur_editor;
  gboolean fund_editor;
  unsigned index;
  cur_slider = (Slide_Data *) user_data;
  fund_editor = cur_slider->fund_assoc;
  index = cur_slider->parent_index;
  add_prec_slider (fund_editor, index);

  if (fund_editor == TRUE)
    {
      cur_editor = &wv_all_freqs->d[g_fund_set].fund_editor;
      if (index == 0 && cur_editor->freq_sliders->len == 2)
	gtk_widget_set_sensitive (cur_editor->freq_slid_rm_btn, TRUE);
      else if (index == 0)
	return;
    }
  else
    cur_editor = wv_all_freqs->d[g_fund_set].wv_editors->d[index];
  if (cur_editor->amp_sliders->len == 2)
    gtk_widget_set_sensitive (cur_editor->amp_slid_rm_btn, TRUE);
}

/**
 * Deletes a precision slider from a precision slider group.
 * @param user_data a pointer to the Slide_Data structure
 */
void
precslid_remove_clicked (GtkButton * button, gpointer user_data)
{
  Slide_Data *cur_slider;
  Wv_Editor_Data *cur_editor;
  gboolean fund_editor;
  unsigned index;
  cur_slider = (Slide_Data *) user_data;
  fund_editor = cur_slider->fund_assoc;
  index = cur_slider->parent_index;
  remove_prec_slider (fund_editor, index);

  if (fund_editor == TRUE)
    {
      cur_editor = &wv_all_freqs->d[g_fund_set].fund_editor;
      if (index == 0 && cur_editor->freq_sliders->len == 1)
	gtk_widget_set_sensitive (cur_editor->freq_slid_rm_btn, FALSE);
      else if (index == 0)
	return;
    }
  else
    cur_editor = wv_all_freqs->d[g_fund_set].wv_editors->d[index];
  if (cur_editor->amp_sliders->len == 1)
    gtk_widget_set_sensitive (cur_editor->amp_slid_rm_btn, FALSE);
}

/**
 * Signal handler called when a slider in a precision slider group is
 * changed.
 * @param user_data a pointer to the Slide_Data structure
 */
void
precslid_value_changed (GtkHScrollbar * scrollbar, gpointer user_data)
{
  Slide_Data *cur_slider;
  Wv_Editor_Data *cur_editor;
  gdouble new_value;
  gdouble value_mult;
  GtkWidget *cur_mntisa;
  GtkWidget *cur_exp;
  float store_value; /* Value to store in whatever is actually being
			modified */

  cur_slider = (Slide_Data *) user_data;
  new_value = gtk_range_get_value (GTK_RANGE (cur_slider->widget));
  /* Because of range and precision conversions, we must perform
     multiplications the way we do in the code to prevent unusual
     scroll-bar behavior.  */
  /* Note that the base of the exponent calculation below should be
     user configurable or calculated from the length of the scroll
     bars.  */
  value_mult = pow(100, -((double) cur_slider->index));

  if (cur_slider->fund_assoc == TRUE)
    {
      cur_editor = &wv_all_freqs->d[g_fund_set].fund_editor;
      if (cur_slider->parent_index == 0)
	{
	  cur_mntisa = cur_editor->fndfrq_mntisa;
	  cur_exp = cur_editor->fndfrq_exp;
	}
      else
	{
	  cur_mntisa = cur_editor->amp_mntisa;
	  cur_exp = cur_editor->amp_exp;
	}
    }
  else
    {
      unsigned index;
      index = cur_slider->parent_index;
      cur_editor =
	wv_all_freqs->d[g_fund_set].wv_editors->d[index];
      cur_mntisa = cur_editor->amp_mntisa;
      cur_exp = cur_editor->amp_exp;
    }

  {
    /* The sliders should primarily change the mantissa in the display
       rather than change the direct floating point value.  */
    const gchar *mntisa_string;
    float sci_value;
    gchar *new_mntisa;
    mntisa_string = gtk_entry_get_text (GTK_ENTRY (cur_mntisa));
    sscanf (mntisa_string, "%f", &sci_value);

    sci_value += (float) (new_value * value_mult) -
      (float) (cur_slider->last_value * value_mult);
    /* Never allocate "big data" on the stack.  That includes "fixed
       length" strings.  */
    new_mntisa = (gchar *) g_malloc (140);
    sprintf (new_mntisa, "%f", sci_value);
    gtk_entry_set_text (GTK_ENTRY (cur_mntisa), new_mntisa);
    g_free (new_mntisa);

    store_value =
      sci_notation_get_value (GTK_ENTRY (cur_mntisa),
			      GTK_SPIN_BUTTON (cur_exp));
  }

  if (cur_slider->fund_assoc == TRUE)
    {
      cur_editor = &wv_all_freqs->d[g_fund_set].fund_editor;
      if (cur_slider->parent_index == 0)
	  wv_all_freqs->d[g_fund_set].fund_freq = store_value;
      else
	  wv_all_freqs->d[g_fund_set].amplitude = store_value;
    }
  else
    {
      cur_editor->data->amplitude = store_value;
    }

  cur_slider->last_value = new_value;
  gtk_widget_queue_draw (wave_render);
}

/**
 * Signal handler for the GtkEntry within the "Multiply Amplitudes"
 * dialog box.
 */
void
mult_amp_entry_activate (GtkEntry * entry, gpointer user_data)
{
  mult_dlg_text = gtk_entry_get_text (entry);
}

/**
 * Signal handler for the GtkEntry within the "Multiply Amplitudes"
 * dialog box.
 */
gboolean
mult_amp_entry_focus_out (GtkEntry * entry,
			  GdkEventFocus * event, gpointer user_data)
{
  mult_amp_entry_activate (entry, user_data);
  return FALSE;
}
