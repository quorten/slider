/* GTK+ widget signal handlers.

Copyright (C) 2011, 2012, 2013, 2017 Andrew Makousky
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
#include <math.h>
#include <string.h>

#include <gtk/gtk.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include "wv_editors.h"
#include "audio.h"

/** Stores the number entered the "Multiply Amplitudes" dialog.  */
static const gchar *mult_dlg_text;

/** Keeps track of whether the user has unsaved changes in the current
    file.  */
gboolean file_modified = FALSE;
/** Keeps track of the last visited folder in the GTK+ file chooser
    for convenience for the user.  */
gchar *last_folder = NULL;
/** Keeps track of the currently loaded filename.  */
gchar *loaded_fname = NULL;
/** Contains the maximum y-point that is set during wave rendering.  */
float max_ypt;

void
manual_win_destroy (GtkObject * object, gpointer user_data)
{
  manual_window = NULL;
}

/**
 * Verify that audio playback is stopped and reset the GUI
 * accordingly.
 */
static void
gui_audio_stop (void)
{
  if (!audio_playing)
    return;
  audio_stop ();
  if (!audio_playing)
    {
      gtk_button_set_label (GTK_BUTTON (b_play), _("Play"));
      gtk_button_set_image (GTK_BUTTON (b_play), play_image);
    }
}

/**
 * Ask the user if they want to save their file before continuing.
 *
 * Audio playback will be turned off when calling this function.
 *
 * @return TRUE to continue, FALSE to cancel
 */
gboolean check_save (gboolean closing)
{
  GtkWidget *dialog;
  const gchar *prompt_msg;
  const gchar *continue_msg;
  GtkResponseType result;

  /* We don't want audio playing when it's time to ask a serious
     question.  */
  gui_audio_stop ();

  if (!file_modified)
    return TRUE;

  if (closing)
    {
      prompt_msg = _("<b><big>Save changes to the current file before " \
       "closing?</big></b>\n\nIf you close without saving, " \
       "your changes will be discarded.");
      continue_msg = _("Close _without saving");
    }
  else
    {
      prompt_msg = _("<b><big>Save changes to the current file before " \
       "continuing?</big></b>\n\nIf you continue without saving, " \
       "your changes will be discarded.");
      continue_msg = _("Continue _without saving");
    }

  dialog = gtk_message_dialog_new (GTK_WINDOW (main_window),
				   GTK_DIALOG_DESTROY_WITH_PARENT,
				   GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE,
				   NULL);
  gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog), prompt_msg);
  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
		  continue_msg, GTK_RESPONSE_CLOSE,
		  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		  GTK_STOCK_SAVE, GTK_RESPONSE_YES,
		  NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_YES);
  result = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  if (result == GTK_RESPONSE_CLOSE)
    return TRUE;
  else if (result == GTK_RESPONSE_CANCEL)
    return FALSE;
  else if (result == GTK_RESPONSE_YES)
      return save_as ();
  /* Unknown response?  Do nothing.  */
  return FALSE;
}

/**
 * Save a file with a specific name from the GUI.
 */
gboolean
save_as (void)
{
  GtkFileFilter *filter = gtk_file_filter_new ();
  GtkWidget *dialog =
    gtk_file_chooser_dialog_new (_("Save File"),
				 GTK_WINDOW (main_window),
				 GTK_FILE_CHOOSER_ACTION_SAVE,
				 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				 GTK_STOCK_SAVE_AS, GTK_RESPONSE_ACCEPT,
				 NULL);
  gtk_file_filter_set_name (filter, _("Slider Project Files"));
  gtk_file_filter_add_pattern (filter, "*.sliw");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);
  if (last_folder != NULL)
    {
      gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog),
					   last_folder);
    }

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
      gboolean result;
      gchar *filename =
	gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
      unsigned filename_len = strlen (filename);
      g_free (last_folder);
      last_folder = gtk_file_chooser_get_current_folder
	                   (GTK_FILE_CHOOSER (dialog));
      gtk_widget_destroy (dialog);
      if (filename_len <= 5 ||
	  strcmp(&filename[filename_len-5], ".sliw"))
	{
	  filename = g_realloc (filename,
				sizeof(gchar) * (filename_len + 6));
	  strcpy(&filename[filename_len], ".sliw");
	}
      result = save_sliw_project (filename);
      if (!result)
	{
	  g_free (filename);
	  return FALSE;
	}
      file_modified = FALSE;
      g_free(loaded_fname); loaded_fname = NULL;
      loaded_fname = filename;
      return TRUE;
    }
  else
    gtk_widget_destroy (dialog);
  return FALSE;
}

/**
 * Open a file from the GUI.
 */
void
open_file (void)
{
  GtkFileFilter *filter = gtk_file_filter_new ();
  GtkWidget *dialog =
    gtk_file_chooser_dialog_new (_("Open File"),
				 GTK_WINDOW (main_window),
				 GTK_FILE_CHOOSER_ACTION_OPEN,
				 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				 GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				 NULL);
  gtk_file_filter_set_name (filter, _("Slider Project Files"));
  gtk_file_filter_add_pattern (filter, "*.sliw");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);
  if (last_folder != NULL)
    {
      gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog),
					   last_folder);
    }

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
      gchar *filename =
	gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
      g_free (last_folder);
      last_folder = gtk_file_chooser_get_current_folder
	                   (GTK_FILE_CHOOSER (dialog));
      gtk_widget_destroy (dialog);
      g_free(loaded_fname); loaded_fname = NULL;
      unselect_fund_freq (g_fund_set);
      free_wv_editors ();
      init_wv_editors ();
      if (!load_sliw_project (filename))
	{
	  free_wv_editors ();
	  g_free (filename);
	  init_wv_editors ();
	  new_sliw_project ();
	}
      else
	loaded_fname = filename;
      select_fund_freq (g_fund_set);
    }
  else
    gtk_widget_destroy (dialog);
}

gboolean
main_window_delete (GtkWidget * widget, gpointer user_data)
{
  if (check_save (TRUE))
    {
      cb_fund_set = NULL;
      return FALSE;
    }
  return TRUE;
}

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
  float *ypts;

  if (G_UNLIKELY (wr_gc == NULL))
    {
      wr_gc = gdk_gc_new (wave_render->window);
      gdk_gc_set_rgb_fg_color (wr_gc, &wr_foreground);
    }

  win_width = widget->allocation.width;
  win_height = widget->allocation.height;
  ypts = (float *) g_malloc (sizeof (float) * win_width);
  render_waves (ypts, win_width, 1.0 / calc_freq_extent ());

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
      gdk_draw_line (widget->window, wr_gc, i, last_ypt, i, ypt);
      last_ypt = ypt;
    }

  g_free (ypts);

  return TRUE;
}

void
b_save_as_clicked (GtkButton * button, gpointer user_data)
{
  gui_audio_stop ();
  save_as ();
}

void
activate_action (GtkAction * action)
{
  const gchar *name = gtk_action_get_name (action);
  const gchar *typename = G_OBJECT_TYPE_NAME (action);
  if (!strcmp (name, "New"))
    {
      if (!check_save (FALSE))
	return;
      file_modified = FALSE;
      g_free (loaded_fname); loaded_fname = NULL;
      unselect_fund_freq (g_fund_set);
      free_wv_editors ();
      init_wv_editors ();
      new_sliw_project ();
      select_fund_freq (g_fund_set);
    }
  else if (!strcmp (name, "Open"))
    {
      if (!check_save (FALSE))
	return;
      open_file ();
    }
  else if (!strcmp (name, "Save"))
    {
      if (loaded_fname != NULL &&
	  save_sliw_project (loaded_fname))
	file_modified = FALSE;
      else
	{
	  gui_audio_stop ();
	  save_as ();
	}
    }
  else if (!strcmp (name, "SaveAs"))
    {
      gui_audio_stop ();
      save_as ();
    }
  else if (!strcmp (name, "Export"))
    {
      GtkWidget *dialog;
      gui_audio_stop ();
      dialog = gtk_file_chooser_dialog_new (_("Export Nyquist File"),
			     GTK_WINDOW (main_window),
			     GTK_FILE_CHOOSER_ACTION_SAVE, NULL, NULL);
      gtk_dialog_add_buttons (GTK_DIALOG (dialog),
			      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			      _("_Export"), GTK_RESPONSE_ACCEPT,  NULL);

      if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
	{
	  char *filename =
	    gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
	  gtk_widget_destroy (dialog);
	  export_sliw_project (filename);
	  g_free (filename);
	}
      else
	gtk_widget_destroy (dialog);
    }
  else if (!strcmp (name, "Preferences"))
    ;
  else if (!strcmp (name, "Quit"))
    {
      if (!check_save (TRUE))
	return;
      cb_fund_set = NULL;
      gtk_widget_destroy (main_window);
    }
  else if (!strcmp (name, "Play"))
    {
      audio_play ();
      if (audio_playing)
	{
	  gtk_button_set_label (GTK_BUTTON (b_play), _("Stop"));
	  gtk_button_set_image (GTK_BUTTON (b_play), stop_image);
	}
    }
  else if (!strcmp (name, "Stop"))
    {
      audio_stop ();
      if (!audio_playing)
	{
	  gtk_button_set_label (GTK_BUTTON (b_play), _("Play"));
	  gtk_button_set_image (GTK_BUTTON (b_play), play_image);
	}
    }
  else if (!strcmp (name, "Manual"))
    display_manual ();
  else if (!strcmp (name, "About"))
    display_about_box ();
}

void
b_play_clicked (GtkButton * button, gpointer user_data)
{
  if (!audio_playing)
    {
      audio_play ();
      if (audio_playing)
	{
	  gtk_button_set_label (button, _("Stop"));
	  gtk_button_set_image (button, stop_image);
	}
    }
  else
    {
      audio_stop ();
      if (!audio_playing)
	{
	  gtk_button_set_label (button, _("Play"));
	  gtk_button_set_image (button, play_image);
	}
    }
}

void
agc_vol_changed (GtkRange * range, gpointer user_data)
{
  agc_volume = (float) gtk_range_get_value (range);
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
 * Signal handler sent when the main edit area changes.
 *
 * This signal handler approximately preserves the ratio between the
 * two divided panes when the main window size changes.  In GTK+,
 * widgets that aren't windows in the window system are not sent
 * configure events, but rather "size-request" and "size-allocate"
 * events.  The signal handler's weren't quite as well documented in
 * the GTK+ documentation as they could have been.
 */
void
wv_edit_div_allocate (GtkWidget * widget,
		      GtkAllocation * allocation, gpointer user_data)
{
  /* Set the divider position to preserve the previous ratio.
     If this is the first time, set the ratio to a predefined value.  */
  static gboolean div_set = FALSE;
  static gfloat div_ratio;
  static gint last_win_height;
  gint div_pos;

  if (G_UNLIKELY (!div_set))
    {
      div_ratio = 0.5f;
      div_set = TRUE;
    }
  else if (last_win_height == allocation->height)
    {
      div_pos = gtk_paned_get_position (GTK_PANED (widget));
      div_ratio = (gfloat) div_pos / last_win_height;
      return;
    }

  div_pos = (gint) ((gfloat) allocation->height * div_ratio);
  gtk_paned_set_position (GTK_PANED (widget), div_pos);
  last_win_height = allocation->height;
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
  GtkWidget *dialog = create_mult_amps_dialog ();
  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
      float new_amplitude;
      if (sscanf(mult_dlg_text, "%g", &new_amplitude) > 0)
	{
	  file_modified = TRUE;
	  mult_amplitudes (new_amplitude, dialog);
	}
    }
  gtk_widget_destroy (dialog);
}

void
fundset_add_clicked (GtkButton * button, gpointer user_data)
{
  unsigned new_fund;
  Wv_Data *second_harmonic;
  file_modified = TRUE;
  add_fund_freq ();
  new_fund = wv_all_freqs->len - 1;
  add_harmonic (new_fund);
  second_harmonic = &wv_all_freqs->d[new_fund].harmonics->d[0];
  add_wv_editor (new_fund, 0, second_harmonic);
  /* Select the new fundamental set.  */
  unselect_fund_freq (g_fund_set);
  g_fund_set = new_fund;
  select_fund_freq (g_fund_set);
  /* Add a combo box entry.  */
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
  file_modified = TRUE;
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
  Wv_Editor_Data *cur_data = &wv_all_freqs->d[g_fund_set].fund_editor;
  file_modified = TRUE;
  wv_all_freqs->d[g_fund_set].fund_freq =
    sci_notation_get_value (entry, GTK_SPIN_BUTTON (cur_data->fndfrq_exp));

  update_slider_bases (entry, cur_data, TRUE);

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
  Wv_Editor_Data *cur_data = &wv_all_freqs->d[g_fund_set].fund_editor;
  file_modified = TRUE;
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
  Wv_Editor_Data *cur_data = &wv_all_freqs->d[g_fund_set].fund_editor;
  file_modified = TRUE;
  wv_all_freqs->d[g_fund_set].amplitude =
    sci_notation_get_value (entry, GTK_SPIN_BUTTON (cur_data->amp_exp));

  update_slider_bases (entry, cur_data, FALSE);

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
  Wv_Editor_Data *cur_data = &wv_all_freqs->d[g_fund_set].fund_editor;
  file_modified = TRUE;
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
  Wv_Editor_Data *cur_editor = (Wv_Editor_Data *) user_data;
  gint new_sel = gtk_combo_box_get_active (combobox);
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
  Wv_Editor_Data *cur_data = (Wv_Editor_Data *) user_data;
  file_modified = TRUE;
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
  Wv_Editor_Data *cur_editor = (Wv_Editor_Data *) user_data;
  unsigned harmc_idx = cur_editor->data->group_idx;
  unsigned swap_idx;
  unsigned i;
  file_modified = TRUE;

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
  /* Add the new editor after the current editor.  */
  Wv_Editor_Data *cur_editor = (Wv_Editor_Data *) user_data;
  unsigned new_ed_idx = cur_editor->index + 1;
  GtkWidget *new_widget;

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
  Wv_Editor_Data *cur_editor = (Wv_Editor_Data *) user_data;
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
  Wv_Editor_Data *cur_editor = (Wv_Editor_Data *) user_data;
  file_modified = TRUE;
  cur_editor->data->amplitude =
    sci_notation_get_value (entry, GTK_SPIN_BUTTON (cur_editor->amp_exp));

  update_slider_bases (entry, cur_editor, FALSE);

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
  Wv_Editor_Data *cur_editor = (Wv_Editor_Data *) user_data;
  file_modified = TRUE;
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
  const gchar *mntisa_string = gtk_entry_get_text (mntisa_widget);
  int exp_num = gtk_spin_button_get_value_as_int (exp_widget);
  char *sci_string = (char *) g_malloc (strlen (mntisa_string) + 11 + 1);
  float sci_value;
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
  gboolean last_mod_state = file_modified;
  sprintf (sci_string, "%e", value);
  e_pos = strchr (sci_string, (int)'e');
  *e_pos = '\0'; /* Break the string in half.  */
  e_pos++;
  sscanf (e_pos, "%i", &exp_num);
  gtk_entry_set_text (mntisa_widget, sci_string);

  /* Make sure not to accidentally change the file modification state
     when doing this.  */
  gtk_spin_button_set_value (exp_widget, (gdouble) exp_num);
  file_modified = last_mod_state;
}

/**
 * Adds a precision slider to a precision slider group.
 * @param user_data a pointer to the Slide_Data structure
 */
void
precslid_add_clicked (GtkButton * button, gpointer user_data)
{
  Slide_Data *cur_slider = (Slide_Data *) user_data;
  Wv_Editor_Data *cur_editor;
  gboolean fund_editor = cur_slider->fund_assoc;
  unsigned index = cur_slider->parent_index;
  add_prec_slider (fund_editor, index);

  if (fund_editor)
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
  Slide_Data *cur_slider = (Slide_Data *) user_data;
  Wv_Editor_Data *cur_editor;
  gboolean fund_editor = cur_slider->fund_assoc;
  unsigned index = cur_slider->parent_index;
  remove_prec_slider (fund_editor, index);

  if (fund_editor)
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
  gdouble new_value;
  gdouble value_mult;
  Wv_Editor_Data *cur_editor;
  Slide_Data_Ptr_array *slider_array;
  GtkWidget *cur_mntisa;
  GtkWidget *cur_exp;
  float sci_value;
  float store_value; /* Value to store in whatever is actually being
			modified */
  file_modified = TRUE;

  cur_slider = (Slide_Data *) user_data;
  new_value = gtk_range_get_value (GTK_RANGE (cur_slider->widget));
  cur_slider->last_value = new_value;

  if (cur_slider->fund_assoc)
    {
      cur_editor = &wv_all_freqs->d[g_fund_set].fund_editor;
      if (cur_slider->parent_index == 0)
	{
	  slider_array = cur_editor->freq_sliders;
	  cur_mntisa = cur_editor->fndfrq_mntisa;
	  cur_exp = cur_editor->fndfrq_exp;
	}
      else
	{
	  slider_array = cur_editor->amp_sliders;
	  cur_mntisa = cur_editor->amp_mntisa;
	  cur_exp = cur_editor->amp_exp;
	}
    }
  else
    {
      unsigned index = cur_slider->parent_index;
      cur_editor =
	wv_all_freqs->d[g_fund_set].wv_editors->d[index];
      slider_array = cur_editor->amp_sliders;
      cur_mntisa = cur_editor->amp_mntisa;
      cur_exp = cur_editor->amp_exp;
    }

  /* Because of range and precision conversions, we must compute the
     new mantissa value from this "base + offset * scale" formula
     rather than a delta formula to prevent unusual scroll-bar
     behavior.  */
  {
    float next_base = cur_slider->base;
    unsigned i;
    for (i = cur_slider->index; i < slider_array->len; i++)
      {
	Slide_Data *iter_slider = slider_array->d[i];
	/* Note that the base of the exponent calculation below should
	   be user configurable or calculated from the length of the
	   scroll bars.  Note that this formula is copied throughout
	   the program rather than only having a single defintion.  */
	gdouble value_mult = pow(100, -((gdouble) i));
	iter_slider->base = next_base;
	next_base = next_base + iter_slider->last_value * value_mult;
      }
    sci_value = next_base;
  }

  {
    /* The sliders should primarily change the mantissa in the display
       rather than change the direct floating point value.  */

    /* Never allocate "big data" on the stack.  That includes "fixed
       length" strings.  Okay, never say never... on modern computers,
       strings are considered small.  */
    gchar *new_mntisa = (gchar *) g_malloc (140);
    sprintf (new_mntisa, "%f", sci_value);
    gtk_entry_set_text (GTK_ENTRY (cur_mntisa), new_mntisa);
    g_free (new_mntisa);

    store_value =
      sci_notation_get_value (GTK_ENTRY (cur_mntisa),
			      GTK_SPIN_BUTTON (cur_exp));
  }

  if (cur_slider->fund_assoc)
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

/**
 * Update the slider base values.
 *
 * Update the mantissa adjustment sliders for a specific editor.
 * @param cur_data the editor to adjust
 * @param freq_sliders TRUE to adjust @a freq_sliders, FALSE to adjust
 * @a amp_sliders.
 */
void
update_slider_bases (GtkEntry * entry, Wv_Editor_Data * cur_data,
		     gboolean freq_sliders)
{
  Slide_Data_Ptr_array *sliders = ((freq_sliders) ? cur_data->freq_sliders :
				   cur_data->amp_sliders);
  Slide_Data *last_slider = sliders->d[sliders->len-1];
  gdouble value_mult = pow(100, -((gdouble) last_slider->index));
  float last_mntisa = (last_slider->base +
		       last_slider->last_value * value_mult);
  float cur_mntisa;
  float mntisa_diff;
  unsigned i;
  sscanf (gtk_entry_get_text (entry), "%e", &cur_mntisa);
  mntisa_diff = cur_mntisa - last_mntisa;

  for (i = 0; i < sliders->len; i++)
    sliders->d[i]->base += mntisa_diff;
}
