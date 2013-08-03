/* Graphical user interface building functions.

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

#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include "wv_editors.h"

#define GLADE_HOOKUP_OBJECT(component,widget,name) \
  g_object_set_data_full (G_OBJECT (component), name, \
    gtk_widget_ref (widget), (GDestroyNotify) gtk_widget_unref)

#define GLADE_HOOKUP_OBJECT_NO_REF(component,widget,name) \
  g_object_set_data (G_OBJECT (component), name, widget)

/** Main application window.  */
GtkWidget *main_window = NULL;
/** Combo box that selects the fundamental set.  */
GtkWidget *cb_fund_set = NULL;
/** GTK+ container that holds the wave editor windows.  */
GtkWidget *wave_edit_cntr;
/** Composite wave rendering area.  */
GtkWidget *wave_render;
/** Foreground color of wave rendering area.  */
GdkColor wr_foreground;
/** Background color of wave rendering area.  */
GdkColor wr_background;
/** Graphics context for drawing in the wave rendering area.  */
GdkGC *wr_gc = NULL;
/** Audio playback button */
GtkWidget *b_play;
GtkWidget *play_image;
GtkWidget *stop_image;
/** The window that displays the documentation manual.  */
GtkWidget *manual_window = NULL;

static const gchar *ui_info =
"<ui>"
"  <menubar name='MenuBar'>"
"    <menu action='FileMenu'>"
"      <menuitem action='New'/>"
"      <menuitem action='Open'/>"
"      <menuitem action='Save'/>"
"      <menuitem action='SaveAs'/>"
"      <menuitem action='Export'/>"
/* "      <menuitem action='Preferences'/>" */
"      <separator/>"
"      <menuitem action='Quit'/>"
"    </menu>"
"    <menu action='TransportMenu'>"
"      <menuitem action='Play'/>"
"      <menuitem action='Stop'/>"
"    </menu>"
"    <menu action='HelpMenu'>"
"      <menuitem action='Manual'/>"
"      <menuitem action='About'/>"
"    </menu>"
"  </menubar>"
"</ui>";

GtkWidget *
create_main_window (void)
{
  GtkWidget *main_vbox;
  GtkWidget *menu_bar;
  GtkWidget *tools_hbox;
  GtkWidget *main_commands;
  GtkWidget *b_save_as;
  GtkWidget *agc_vol_scale;
  GtkWidget *fundset_sel_hbox;
  GtkWidget *fundset_label;
  GtkWidget *wv_edit_div;
  GtkWidget *wave_editors_sb;
  GtkWidget *wv_edit_viewport;

  /* Each entry takes the following form:
     { name, stock id, label, accelerator, tooltip, callback }
     Any parts not specified take on default zero values.  */
  GtkActionEntry entries[] = {
    { "FileMenu", NULL, _("_File") },
    { "TransportMenu", NULL, _("_Transport") },
    { "HelpMenu", NULL, _("_Help") },
    { "New", GTK_STOCK_NEW, _("_New"), "<control>N",
      _("Create a new file"),
      G_CALLBACK (activate_action) },
    { "Open", GTK_STOCK_OPEN, _("_Open..."), "<control>O",
      _("Open an existing file"),
      G_CALLBACK (activate_action) },
    { "Save", GTK_STOCK_SAVE, _("_Save"),"<control>S",
      _("Save the current file"),
      G_CALLBACK (activate_action) },
    { "SaveAs", GTK_STOCK_SAVE_AS, _("Save _As..."), NULL,
      _("Save the current file with a new name"),
      G_CALLBACK (activate_action) },
    { "Export", NULL, _("_Export..."), NULL,
      _("Export a Nyquist script that generates this waveform"),
      G_CALLBACK (activate_action) },
    /* { "Preferences", NULL, _("_Preferences"), NULL,
       _("Preferences"),
       G_CALLBACK (activate_action) }, */
    { "Quit", GTK_STOCK_QUIT, _("_Quit"), "<control>Q",
      _("Leave Slider Wave Editor"),
      G_CALLBACK (activate_action) },
    { "Play", GTK_STOCK_MEDIA_PLAY, _("_Play"), "<control>X",
      _("Enable playback of the current waveform"),
      G_CALLBACK (activate_action) },
    { "Stop", GTK_STOCK_MEDIA_STOP, _("_Stop"), "<control>Z",
      _("Stop playback of the current waveform"),
      G_CALLBACK (activate_action) },
    { "Manual", GTK_STOCK_HELP, _("_Manual"), "F1",
      _("Display the manual"),
      G_CALLBACK (activate_action) },
    { "About", GTK_STOCK_ABOUT, _("_About"), NULL,
      _("Display program name and copyright information"),
      G_CALLBACK (activate_action) },
  };
  guint n_entries = G_N_ELEMENTS (entries);

  main_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  /* gtk_container_set_border_width (GTK_CONTAINER (main_window), 8); */
  gtk_window_set_title (GTK_WINDOW (main_window), _("Slider Wave Editor"));
  gtk_window_set_default_size (GTK_WINDOW (main_window), 600, 400);

  main_vbox = gtk_vbox_new (FALSE, 5);
  gtk_widget_show (main_vbox);
  gtk_container_add (GTK_CONTAINER (main_window), main_vbox);

  { /* Build the menu bar.  */
    GtkActionGroup *action_group;
    GtkUIManager *merge;
    GError *error = NULL;

    action_group = gtk_action_group_new ("AppWindowActions");
    gtk_action_group_add_actions (action_group,
				  entries, n_entries,
				  main_window);
    merge = gtk_ui_manager_new ();
    g_object_set_data_full (G_OBJECT (main_window), "ui-manager", merge,
			    g_object_unref);
    gtk_ui_manager_insert_action_group (merge, action_group, 0);
    gtk_window_add_accel_group (GTK_WINDOW (main_window),
				gtk_ui_manager_get_accel_group (merge));
    if (!gtk_ui_manager_add_ui_from_string (merge, ui_info, -1, &error))
      {
	g_message ("building menus failed: %s", error->message);
	g_error_free (error);
      }
    menu_bar = gtk_ui_manager_get_widget (merge, "/MenuBar");
    gtk_widget_show (menu_bar);
    gtk_box_pack_start (GTK_BOX (main_vbox), menu_bar, FALSE, FALSE, 0);
  }

  tools_hbox = gtk_hbox_new (FALSE, 5);
  gtk_widget_show (tools_hbox);
  gtk_box_pack_start (GTK_BOX (main_vbox), tools_hbox, FALSE, FALSE, 0);

  main_commands = gtk_hbutton_box_new ();
  gtk_widget_show (main_commands);
  gtk_box_pack_start (GTK_BOX (tools_hbox), main_commands, FALSE, FALSE, 0);
  gtk_box_set_spacing (GTK_BOX (main_commands), 5);

  b_save_as = gtk_button_new_with_mnemonic (_("Save As"));
  gtk_button_set_image (GTK_BUTTON (b_save_as),
    gtk_image_new_from_stock (GTK_STOCK_SAVE_AS, GTK_ICON_SIZE_BUTTON));
  gtk_widget_show (b_save_as);
  gtk_container_add (GTK_CONTAINER (main_commands), b_save_as);
  GTK_WIDGET_SET_FLAGS (b_save_as, GTK_CAN_DEFAULT);

  play_image = gtk_image_new_from_stock (GTK_STOCK_MEDIA_PLAY,
					 GTK_ICON_SIZE_BUTTON);
  g_object_ref (play_image);
  gtk_object_sink (GTK_OBJECT (play_image));
  stop_image = gtk_image_new_from_stock (GTK_STOCK_MEDIA_STOP,
					 GTK_ICON_SIZE_BUTTON);
  g_object_ref (stop_image);
  gtk_object_sink (GTK_OBJECT (stop_image));

  b_play = gtk_button_new_with_mnemonic (_("Play"));
  gtk_button_set_image (GTK_BUTTON (b_play), play_image);
  gtk_widget_show (b_play);
  gtk_container_add (GTK_CONTAINER (main_commands), b_play);
  GTK_WIDGET_SET_FLAGS (b_play, GTK_CAN_DEFAULT);

  agc_vol_scale = gtk_hscale_new_with_range (0.0, 1.0, 0.01);
  gtk_scale_set_value_pos (GTK_SCALE (agc_vol_scale), GTK_POS_LEFT);
  gtk_range_set_value (GTK_RANGE (agc_vol_scale), 0.5);
  gtk_widget_show (agc_vol_scale);
  gtk_box_pack_start (GTK_BOX (tools_hbox), agc_vol_scale, TRUE, TRUE, 0);

  fundset_sel_hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (fundset_sel_hbox);
  gtk_box_pack_end (GTK_BOX (tools_hbox), fundset_sel_hbox, FALSE, FALSE, 0);

  fundset_label = gtk_label_new (_("Fundamental Set: "));
  gtk_widget_show (fundset_label);
  gtk_box_pack_start (GTK_BOX (fundset_sel_hbox), fundset_label, FALSE,
		      FALSE, 0);

  cb_fund_set = gtk_combo_box_new_text ();
  gtk_widget_show (cb_fund_set);
  gtk_box_pack_start (GTK_BOX (fundset_sel_hbox), cb_fund_set, TRUE, TRUE, 0);
  gtk_widget_set_size_request (cb_fund_set, 100, -1);

  wv_edit_div = gtk_vpaned_new ();
  gtk_widget_show (wv_edit_div);
  gtk_box_pack_start (GTK_BOX (main_vbox), wv_edit_div, TRUE, TRUE, 0);

  wave_render = gtk_drawing_area_new ();
  gtk_widget_show (wave_render);
  gtk_paned_pack1 (GTK_PANED (wv_edit_div), wave_render, FALSE, TRUE);

  wave_editors_sb = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (wave_editors_sb);
  gtk_paned_pack2 (GTK_PANED (wv_edit_div), wave_editors_sb, TRUE, TRUE);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (wave_editors_sb),
				  GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

  wv_edit_viewport = gtk_viewport_new (NULL, NULL);
  gtk_widget_show (wv_edit_viewport);
  gtk_container_add (GTK_CONTAINER (wave_editors_sb), wv_edit_viewport);

  wave_edit_cntr = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (wave_edit_cntr);
  gtk_container_add (GTK_CONTAINER (wv_edit_viewport), wave_edit_cntr);

  g_signal_connect ((gpointer) main_window, "delete-event",
		    G_CALLBACK (main_window_delete), NULL);
  g_signal_connect ((gpointer) wv_edit_div, "size_allocate",
		    G_CALLBACK (wv_edit_div_allocate), NULL);
  g_signal_connect ((gpointer) b_save_as, "clicked",
		    G_CALLBACK (b_save_as_clicked), NULL);
  g_signal_connect ((gpointer) b_play, "clicked",
		    G_CALLBACK (b_play_clicked), NULL);
  g_signal_connect ((gpointer) agc_vol_scale, "value-changed",
		    G_CALLBACK (agc_vol_changed), NULL);
  g_signal_connect ((gpointer) cb_fund_set, "changed",
		    G_CALLBACK (cb_fund_set_changed), NULL);
  g_signal_connect ((gpointer) wave_render, "expose_event",
		    G_CALLBACK (wavrnd_expose), NULL);

  select_fund_freq (g_fund_set);

  /* Store pointers to all widgets, for use by lookup_widget().  */
  GLADE_HOOKUP_OBJECT_NO_REF (main_window, main_window, "main_window");
  GLADE_HOOKUP_OBJECT (main_window, main_vbox, "main_vbox");
  GLADE_HOOKUP_OBJECT (main_window, tools_hbox, "tools_hbox");
  GLADE_HOOKUP_OBJECT (main_window, main_commands, "main_commands");
  GLADE_HOOKUP_OBJECT (main_window, b_save_as, "b_save_as");
  GLADE_HOOKUP_OBJECT (main_window, b_play, "b_play");
  GLADE_HOOKUP_OBJECT (main_window, agc_vol_scale, "agc_vol_scale");
  GLADE_HOOKUP_OBJECT (main_window, fundset_sel_hbox, "fundset_sel_hbox");
  GLADE_HOOKUP_OBJECT (main_window, fundset_label, "fundset_label");
  GLADE_HOOKUP_OBJECT (main_window, cb_fund_set, "cb_fund_set");
  GLADE_HOOKUP_OBJECT (main_window, wv_edit_div, "wv_edit_div");
  GLADE_HOOKUP_OBJECT (main_window, wave_render, "wave_render");
  GLADE_HOOKUP_OBJECT (main_window, wave_editors_sb, "wave_editors_sb");
  GLADE_HOOKUP_OBJECT (main_window, wv_edit_viewport, "wv_edit_viewport");
  GLADE_HOOKUP_OBJECT (main_window, wave_edit_cntr, "wave_edit_cntr");

  return main_window;
}

/**
 * Create the widgets for a wave editor window.
 *
 * When a different fundamental frequency set is selected or a new
 * project is created or opened, the widgets for the wave editors need
 * to be recreated.  This function only creates the widgets for a wave
 * editor window and does not update the data model.  In order to
 * properly add a new wave editor window for harmonics, you should
 * also call add_wv_editor().  If the fundamental frequency set being
 * worked with is currently selected, then you will need to update
 * other information as is done in harmc_win_add_clicked().
 *
 * @param first_type if TRUE then a fundamental frequency wave editor
 * window will be created.  If FALSE, a harmonic wave editor window
 * will be created.
 * @param index if first_type is TRUE, then this specifies which
 * fundamental frequency set should be worked with; otherwise, it
 * specifies which harmonic wave editor of the current fundamental
 * frequency set to work with from the data model.
 */
GtkWidget *
create_wvedit_holder (gboolean first_type, unsigned index)
{
  GtkWidget *wvedit_holder_frame;
  GtkWidget *wvedit_holder_vbox;
  GtkWidget *fund_editor_hbox;
  GtkWidget *fund_editor_left_hbox;
  GtkWidget *harmc_one_drop;
  GtkWidget *mult_amps;
  GtkWidget *fundset_hbox;
  GtkWidget *fundset_label;
  GtkWidget *fundset_add;
  GtkWidget *fundset_remove;
  GtkWidget *fund_freq_hbox;
  GtkWidget *fund_freq_left_hbox;
  GtkWidget *fndfrq_label;
  GtkWidget *fndfrq_mntisa;
  GtkWidget *fndfrq_exp_label;
  GtkObject *fndfrq_exp_adj;
  GtkWidget *fndfrq_exp;
  GtkWidget *fndfrq_precslid_change;
  GtkWidget *fndfrq_precslid_label;
  GtkWidget *fndfrq_precslid_add;
  GtkWidget *fndfrq_precslid_remove;
  GtkWidget *fund_freq_slider_vbox;
  /* GtkWidget *hscrollbar2; */
  GtkWidget *harmc_editor_hbox;
  GtkWidget *harmc_editor_left_hbox;
  GtkWidget *harmc_sel_label;
  GtkWidget *harmc_sel;
  GtkWidget *harmc_window_hbox;
  GtkWidget *harmc_win_label;
  GtkWidget *harmc_win_add;
  GtkWidget *harmc_win_remove;
  GtkWidget *harmc_change_hbox;
  GtkWidget *harmc_add_label;
  GtkWidget *harmc_add;
  GtkWidget *harmc_remove;
  GtkWidget *amp_hbox;
  GtkWidget *amp_left_hbox;
  GtkWidget *amp_set_label;
  GtkWidget *amp_mntisa;
  GtkWidget *amp_exp_label;
  GtkObject *amp_exp_adj;
  GtkWidget *amp_exp;
  GtkWidget *amp_precslid_change;
  GtkWidget *amp_precslid_label;
  GtkWidget *amp_precslid_add;
  GtkWidget *amp_precslid_remove;
  GtkWidget *amp_slider_vbox;
  /* GtkWidget *hscrollbar1; */

  wvedit_holder_frame = gtk_frame_new (NULL);
  gtk_widget_show (wvedit_holder_frame);
  gtk_container_set_border_width (GTK_CONTAINER (wvedit_holder_frame), 5);
  gtk_frame_set_label_align (GTK_FRAME (wvedit_holder_frame), 0, 0);
  gtk_frame_set_shadow_type (GTK_FRAME (wvedit_holder_frame), GTK_SHADOW_OUT);

  wvedit_holder_vbox = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (wvedit_holder_vbox);
  gtk_container_add (GTK_CONTAINER (wvedit_holder_frame), wvedit_holder_vbox);
  gtk_container_set_border_width (GTK_CONTAINER (wvedit_holder_vbox), 5);

  if (first_type)
    {
      fund_editor_hbox = gtk_hbox_new (FALSE, 5);
      gtk_widget_show (fund_editor_hbox);
      gtk_box_pack_start (GTK_BOX (wvedit_holder_vbox), fund_editor_hbox,
			  TRUE, FALSE, 0);

      fund_editor_left_hbox = gtk_hbox_new (FALSE, 0);
      gtk_widget_show (fund_editor_left_hbox);
      gtk_box_pack_start (GTK_BOX (fund_editor_hbox), fund_editor_left_hbox,
			  FALSE, FALSE, 0);

      harmc_one_drop = gtk_button_new_with_mnemonic (_("1st Harmonic Drop"));
      /* Don't discourage the user by displaying something that can
	 never be clicked on.  */
      /* gtk_widget_show (harmc_one_drop); */
      gtk_box_pack_start (GTK_BOX (fund_editor_left_hbox), harmc_one_drop,
			  FALSE, FALSE, 0);
      gtk_widget_set_sensitive (harmc_one_drop, FALSE);

      mult_amps = gtk_button_new_with_mnemonic (_("Multiply Amplitudes"));
      gtk_widget_show (mult_amps);
      gtk_box_pack_start (GTK_BOX (fund_editor_left_hbox), mult_amps, FALSE,
			  FALSE, 0);

      fundset_hbox = gtk_hbox_new (FALSE, 0);
      gtk_widget_show (fundset_hbox);
      gtk_box_pack_end (GTK_BOX (fund_editor_hbox), fundset_hbox, FALSE,
			FALSE, 0);

      fundset_label = gtk_label_new (_("Fundamental Set: "));
      gtk_widget_show (fundset_label);
      gtk_box_pack_start (GTK_BOX (fundset_hbox), fundset_label, FALSE,
			  FALSE, 0);

      fundset_add = gtk_button_new_with_mnemonic (_("Add"));
      gtk_widget_show (fundset_add);
      gtk_box_pack_start (GTK_BOX (fundset_hbox), fundset_add, FALSE,
			  FALSE, 0);

      fundset_remove = gtk_button_new_with_mnemonic (_("Remove"));
      gtk_widget_show (fundset_remove);
      gtk_box_pack_start (GTK_BOX (fundset_hbox), fundset_remove, FALSE,
			  FALSE, 0);
      /* If there is only one fundamental left, disable this button.  */
      if (wv_all_freqs->len == 1)
	gtk_widget_set_sensitive (fundset_remove, FALSE);

      fund_freq_hbox = gtk_hbox_new (FALSE, 5);
      gtk_widget_show (fund_freq_hbox);
      gtk_box_pack_start (GTK_BOX (wvedit_holder_vbox), fund_freq_hbox, TRUE,
			  FALSE, 0);

      fund_freq_left_hbox = gtk_hbox_new (FALSE, 0);
      gtk_widget_show (fund_freq_left_hbox);
      gtk_box_pack_start (GTK_BOX (fund_freq_hbox), fund_freq_left_hbox,
			  FALSE, FALSE, 0);

      fndfrq_label = gtk_label_new (_("Fundamental Frequency: "));
      gtk_widget_show (fndfrq_label);
      gtk_box_pack_start (GTK_BOX (fund_freq_left_hbox), fndfrq_label, FALSE,
			  FALSE, 0);

      fndfrq_mntisa = gtk_entry_new ();
      gtk_widget_show (fndfrq_mntisa);
      gtk_box_pack_start (GTK_BOX (fund_freq_left_hbox), fndfrq_mntisa, TRUE,
			  TRUE, 0);

      fndfrq_exp_label = gtk_label_new (" \303— 10^");
      gtk_widget_show (fndfrq_exp_label);
      gtk_box_pack_start (GTK_BOX (fund_freq_left_hbox), fndfrq_exp_label,
			  FALSE, FALSE, 0);

      fndfrq_exp_adj = gtk_adjustment_new (0, -100, 100, 1, 10, 0);
      fndfrq_exp = gtk_spin_button_new (GTK_ADJUSTMENT (fndfrq_exp_adj), 1, 0);
      gtk_widget_show (fndfrq_exp);
      gtk_box_pack_start (GTK_BOX (fund_freq_left_hbox), fndfrq_exp, TRUE,
			  TRUE, 0);

      fndfrq_precslid_change = gtk_hbox_new (FALSE, 0);
      gtk_widget_show (fndfrq_precslid_change);
      gtk_box_pack_end (GTK_BOX (fund_freq_hbox), fndfrq_precslid_change,
			FALSE, FALSE, 0);

      fndfrq_precslid_label = gtk_label_new (_("Precision Slider: "));
      gtk_widget_show (fndfrq_precslid_label);
      gtk_box_pack_start (GTK_BOX (fndfrq_precslid_change),
			  fndfrq_precslid_label, FALSE, FALSE, 0);

      fndfrq_precslid_add = gtk_button_new_with_mnemonic (_("Add"));
      gtk_widget_show (fndfrq_precslid_add);
      gtk_box_pack_start (GTK_BOX (fndfrq_precslid_change),
			  fndfrq_precslid_add, FALSE, FALSE, 0);

      fndfrq_precslid_remove = gtk_button_new_with_mnemonic (_("Remove"));
      gtk_widget_show (fndfrq_precslid_remove);
      gtk_box_pack_start (GTK_BOX (fndfrq_precslid_change),
			  fndfrq_precslid_remove, FALSE, FALSE, 0);
      if (wv_all_freqs->d[index].fund_editor.freq_sliders->len <= 1)
	gtk_widget_set_sensitive (fndfrq_precslid_remove, FALSE);

      fund_freq_slider_vbox = gtk_vbox_new (FALSE, 0);
      gtk_widget_show (fund_freq_slider_vbox);
      gtk_box_pack_start (GTK_BOX (wvedit_holder_vbox), fund_freq_slider_vbox,
			  TRUE, FALSE, 0);

      /* hscrollbar2 =
	gtk_hscrollbar_new (GTK_ADJUSTMENT
			    (gtk_adjustment_new (10, 0, 21, 0.1, 1, 1)));
      gtk_widget_show (hscrollbar2);
      gtk_box_pack_start (GTK_BOX (fund_freq_slider_vbox), hscrollbar2, TRUE,
			  TRUE, 0); */
    }
  else
    {
      harmc_editor_hbox = gtk_hbox_new (FALSE, 5);
      gtk_widget_show (harmc_editor_hbox);
      gtk_box_pack_start (GTK_BOX (wvedit_holder_vbox), harmc_editor_hbox,
			  TRUE, FALSE, 0);

      harmc_editor_left_hbox = gtk_hbox_new (FALSE, 0);
      gtk_widget_show (harmc_editor_left_hbox);
      gtk_box_pack_start (GTK_BOX (harmc_editor_hbox), harmc_editor_left_hbox,
			  FALSE, FALSE, 0);

      harmc_sel_label = gtk_label_new (_("Harmonic "));
      gtk_widget_show (harmc_sel_label);
      gtk_box_pack_start (GTK_BOX (harmc_editor_left_hbox), harmc_sel_label,
			  FALSE, FALSE, 0);

      harmc_sel = gtk_combo_box_new_text ();
      gtk_widget_show (harmc_sel);
      gtk_box_pack_start (GTK_BOX (harmc_editor_left_hbox), harmc_sel, TRUE,
			  TRUE, 0);
      gtk_widget_set_size_request (harmc_sel, 75, -1);

      harmc_window_hbox = gtk_hbox_new (FALSE, 0);
      gtk_widget_show (harmc_window_hbox);
      gtk_box_pack_start (GTK_BOX (harmc_editor_hbox), harmc_window_hbox, TRUE,
			  FALSE, 0);

      harmc_win_label = gtk_label_new (_("Harmonic Window: "));
      gtk_widget_show (harmc_win_label);
      gtk_box_pack_start (GTK_BOX (harmc_window_hbox), harmc_win_label, FALSE,
			  FALSE, 0);

      harmc_win_add = gtk_button_new_with_mnemonic (_("Add"));
      gtk_widget_show (harmc_win_add);
      gtk_box_pack_start (GTK_BOX (harmc_window_hbox), harmc_win_add, FALSE,
			  FALSE, 0);

      harmc_win_remove = gtk_button_new_with_mnemonic (_("Remove"));
      gtk_widget_show (harmc_win_remove);
      gtk_box_pack_start (GTK_BOX (harmc_window_hbox), harmc_win_remove, FALSE,
			  FALSE, 0);
      if (wv_all_freqs->d[g_fund_set].wv_editors->len == 1)
	gtk_widget_set_sensitive (harmc_win_remove, FALSE);
      /* We must make sure that the widget we might reference in the
	 data model will be valid.  */
      wv_all_freqs->d[g_fund_set].wv_editors->d[index]->harmc_win_rm_btn =
	harmc_win_remove;
      if (wv_all_freqs->d[g_fund_set].wv_editors->len == 2)
	{
	  gtk_widget_set_sensitive ((wv_all_freqs->d[g_fund_set].wv_editors->
				     d[0]->harmc_win_rm_btn), TRUE);
	}

      harmc_change_hbox = gtk_hbox_new (FALSE, 0);
      gtk_widget_show (harmc_change_hbox);
      gtk_box_pack_end (GTK_BOX (harmc_editor_hbox), harmc_change_hbox, FALSE,
			FALSE, 0);

      harmc_add_label = gtk_label_new (_("Harmonic: "));
      gtk_widget_show (harmc_add_label);
      gtk_box_pack_start (GTK_BOX (harmc_change_hbox), harmc_add_label, FALSE,
			  FALSE, 0);

      harmc_add = gtk_button_new_with_mnemonic (_("Add"));
      gtk_widget_show (harmc_add);
      gtk_box_pack_start (GTK_BOX (harmc_change_hbox), harmc_add, FALSE, FALSE,
			  0);

      harmc_remove = gtk_button_new_with_mnemonic (_("Remove"));
      gtk_widget_show (harmc_remove);
      gtk_box_pack_start (GTK_BOX (harmc_change_hbox), harmc_remove, FALSE,
			  FALSE, 0);
    }

  amp_hbox = gtk_hbox_new (FALSE, 5);
  gtk_widget_show (amp_hbox);
  gtk_box_pack_start (GTK_BOX (wvedit_holder_vbox), amp_hbox, TRUE, FALSE, 0);

  amp_left_hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (amp_left_hbox);
  gtk_box_pack_start (GTK_BOX (amp_hbox), amp_left_hbox, FALSE, FALSE, 0);

  amp_set_label = gtk_label_new (_("Amplitude: "));
  gtk_widget_show (amp_set_label);
  gtk_box_pack_start (GTK_BOX (amp_left_hbox), amp_set_label, FALSE, FALSE,
		      0);

  amp_mntisa = gtk_entry_new ();
  gtk_widget_show (amp_mntisa);
  gtk_box_pack_start (GTK_BOX (amp_left_hbox), amp_mntisa, TRUE, TRUE, 0);

  amp_exp_label = gtk_label_new (" \303— 10^");
  gtk_widget_show (amp_exp_label);
  gtk_box_pack_start (GTK_BOX (amp_left_hbox), amp_exp_label, FALSE, FALSE,
		      0);

  amp_exp_adj = gtk_adjustment_new (0, -100, 100, 1, 10, 0);
  amp_exp = gtk_spin_button_new (GTK_ADJUSTMENT (amp_exp_adj), 1, 0);
  gtk_widget_show (amp_exp);
  gtk_box_pack_start (GTK_BOX (amp_left_hbox), amp_exp, TRUE, TRUE, 0);

  amp_precslid_change = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (amp_precslid_change);
  gtk_box_pack_end (GTK_BOX (amp_hbox), amp_precslid_change, FALSE, FALSE, 0);

  amp_precslid_label = gtk_label_new (_("Precision Slider: "));
  gtk_widget_show (amp_precslid_label);
  gtk_box_pack_start (GTK_BOX (amp_precslid_change), amp_precslid_label,
		      FALSE, FALSE, 0);

  amp_precslid_add = gtk_button_new_with_mnemonic (_("Add"));
  gtk_widget_show (amp_precslid_add);
  gtk_box_pack_start (GTK_BOX (amp_precslid_change), amp_precslid_add, FALSE,
		      FALSE, 0);

  amp_precslid_remove = gtk_button_new_with_mnemonic (_("Remove"));
  gtk_widget_show (amp_precslid_remove);
  gtk_box_pack_start (GTK_BOX (amp_precslid_change), amp_precslid_remove,
		      FALSE, FALSE, 0);
  if ((first_type &&
       wv_all_freqs->d[g_fund_set].fund_editor.amp_sliders->len <= 1) ||
      (!first_type &&
       wv_all_freqs->d[g_fund_set].wv_editors->d[index]->
       amp_sliders->len <= 1))
    gtk_widget_set_sensitive (amp_precslid_remove, FALSE);

  amp_slider_vbox = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (amp_slider_vbox);
  gtk_box_pack_start (GTK_BOX (wvedit_holder_vbox), amp_slider_vbox, TRUE,
		      FALSE, 0);

  /* hscrollbar1 =
    gtk_hscrollbar_new (GTK_ADJUSTMENT
			(gtk_adjustment_new (10, 0, 21, 0.1, 1, 1)));
  gtk_widget_show (hscrollbar1);
  gtk_box_pack_start (GTK_BOX (amp_slider_vbox), hscrollbar1, TRUE, TRUE,
		      0); */

  if (first_type)
    {
      wv_all_freqs->d[index].fund_editor.fndfrq_mntisa = fndfrq_mntisa;
      wv_all_freqs->d[index].fund_editor.fndfrq_exp = fndfrq_exp;
      wv_all_freqs->d[index].fund_editor.amp_mntisa = amp_mntisa;
      wv_all_freqs->d[index].fund_editor.amp_exp = amp_exp;
      wv_all_freqs->d[index].fund_editor.freq_slid_rm_btn =
	fndfrq_precslid_remove;
      wv_all_freqs->d[index].fund_editor.freq_sliders_vbox =
	fund_freq_slider_vbox;
      wv_all_freqs->d[index].fund_editor.amp_slid_rm_btn =
	amp_precslid_remove;
      wv_all_freqs->d[index].fund_editor.amp_sliders_vbox = amp_slider_vbox;

      if (wv_all_freqs->d[index].fund_editor.freq_sliders->len == 0 &&
	  wv_all_freqs->d[index].fund_editor.amp_sliders->len == 0)
	{
	  add_prec_slider (TRUE, 0);
	  add_prec_slider (TRUE, 1);
	}

      g_signal_connect ((gpointer) harmc_one_drop, "clicked",
			G_CALLBACK (harmc_one_drop_clicked), NULL);
      g_signal_connect ((gpointer) mult_amps, "clicked",
			G_CALLBACK (mult_amps_clicked), NULL);
      g_signal_connect ((gpointer) fundset_add, "clicked",
			G_CALLBACK (fundset_add_clicked),
			(gpointer)(index));
      g_signal_connect ((gpointer) fundset_remove, "clicked",
			G_CALLBACK (fundset_remove_clicked),
			(gpointer)(index));
      g_signal_connect ((gpointer) fndfrq_mntisa, "activate",
			G_CALLBACK (fndfrq_mntisa_activate), 
			(gpointer)(index));
      g_signal_connect ((gpointer) fndfrq_mntisa, "focus-out-event",
			G_CALLBACK (fndfrq_mntisa_focus_out),
			(gpointer)(index));
      g_signal_connect ((gpointer) fndfrq_exp, "value_changed",
			G_CALLBACK (fndfrq_exp_value_changed),
			(gpointer)(index));
      sci_notation_set_values (GTK_ENTRY (fndfrq_mntisa),
			       GTK_SPIN_BUTTON (fndfrq_exp),
			       wv_all_freqs->d[index].fund_freq);
      g_signal_connect ((gpointer) fndfrq_precslid_add, "clicked",
	      G_CALLBACK (precslid_add_clicked),
	      (gpointer)(wv_all_freqs->d[index].fund_editor.
			 freq_sliders->d[0]));
      g_signal_connect ((gpointer) fndfrq_precslid_remove, "clicked",
	      G_CALLBACK (precslid_remove_clicked),
	      (gpointer)(wv_all_freqs->d[index].fund_editor.
			 freq_sliders->d[0]));

      g_signal_connect ((gpointer) amp_mntisa, "activate",
			G_CALLBACK (fndamp_mntisa_activate),
			(gpointer)(&wv_all_freqs->d[index].fund_editor));
      g_signal_connect ((gpointer) amp_mntisa, "focus-out-event",
			G_CALLBACK (fndamp_mntisa_focus_out),
			(gpointer)(&wv_all_freqs->d[index].fund_editor));
      g_signal_connect ((gpointer) amp_exp, "value_changed",
			G_CALLBACK (fndamp_exp_value_changed),
			(gpointer)(&wv_all_freqs->d[index].fund_editor));
      sci_notation_set_values (GTK_ENTRY (amp_mntisa),
		 GTK_SPIN_BUTTON (amp_exp), wv_all_freqs->d[index].amplitude);
      g_signal_connect ((gpointer) amp_precslid_add, "clicked",
	      G_CALLBACK (precslid_add_clicked),
	      (gpointer)(wv_all_freqs->d[index].fund_editor.
			 amp_sliders->d[0]));
      g_signal_connect ((gpointer) amp_precslid_remove, "clicked",
	      G_CALLBACK (precslid_remove_clicked),
	      (gpointer)(wv_all_freqs->d[index].fund_editor.
			 amp_sliders->d[0]));
    }
  else
    {
      Wv_Editor_Data *cur_editor;
      Wv_Data *cur_data;
      cur_editor = wv_all_freqs->d[g_fund_set].wv_editors->d[index];
      cur_data = cur_editor->data;

      wv_all_freqs->d[g_fund_set].wv_editors->d[index]->amp_mntisa =
	amp_mntisa;
      wv_all_freqs->d[g_fund_set].wv_editors->d[index]->amp_exp = amp_exp;
      wv_all_freqs->d[g_fund_set].wv_editors->d[index]->harmc_sel =
	harmc_sel;
      wv_all_freqs->d[g_fund_set].wv_editors->d[index]->amp_slid_rm_btn =
	amp_precslid_remove;
      wv_all_freqs->d[g_fund_set].wv_editors->d[index]->amp_sliders_vbox =
	amp_slider_vbox;
      /* We already called the following statement earlier in the
	 function to fix a bug.  (It would make more sense to put it
	 here though...)  */
      /* wv_all_freqs->d[g_fund_set].wv_editors->d[index]->harmc_win_rm_btn =
	harmc_win_remove; */

      if (wv_all_freqs->d[g_fund_set].wv_editors->d[index]->
	  amp_sliders->len == 0)
	add_prec_slider (FALSE, index);

      g_signal_connect ((gpointer) harmc_sel, "changed",
			G_CALLBACK (harmc_sel_changed),
			(gpointer)(cur_editor));
      g_signal_connect ((gpointer) harmc_win_add, "clicked",
			G_CALLBACK (harmc_win_add_clicked),
			(gpointer)(cur_editor));
      g_signal_connect ((gpointer) harmc_win_remove, "clicked",
			G_CALLBACK (harmc_win_remove_clicked),
			(gpointer)(cur_editor));
      g_signal_connect ((gpointer) harmc_add, "clicked",
			G_CALLBACK (harmc_add_clicked),
			(gpointer)(cur_editor));
      g_signal_connect ((gpointer) harmc_remove, "clicked",
			G_CALLBACK (harmc_remove_clicked),
			(gpointer)(cur_editor));

      {
	unsigned i;
	for (i = 0; i < wv_all_freqs->d[g_fund_set].harmonics->len; i++)
	  {
	    gchar cb_text[11];
	    unsigned harmc_num;
	    harmc_num = wv_all_freqs->d[g_fund_set].harmonics->d[i].harmc_num;
	    sprintf (cb_text, "%u", harmc_num);
	    gtk_combo_box_append_text (GTK_COMBO_BOX (harmc_sel), cb_text);
	  }
	gtk_combo_box_set_active (GTK_COMBO_BOX (harmc_sel),
	     cur_data->group_idx);
      }

      g_signal_connect ((gpointer) amp_mntisa, "activate",
			G_CALLBACK (amp_mntisa_activate),
			(gpointer)(cur_editor));
      g_signal_connect ((gpointer) amp_mntisa, "focus-out-event",
			G_CALLBACK (amp_mntisa_focus_out),
			(gpointer)(cur_editor));
      g_signal_connect ((gpointer) amp_exp, "value_changed",
			G_CALLBACK (amp_exp_value_changed),
			(gpointer)(cur_editor));
      sci_notation_set_values (GTK_ENTRY (amp_mntisa),
			       GTK_SPIN_BUTTON (amp_exp),
			       cur_data->amplitude);
      g_signal_connect ((gpointer) amp_precslid_add, "clicked",
			G_CALLBACK (precslid_add_clicked),
			(gpointer)(wv_all_freqs->d[g_fund_set].
				   wv_editors->d[index]->amp_sliders->d[0]));
      g_signal_connect ((gpointer) amp_precslid_remove, "clicked",
			G_CALLBACK (precslid_remove_clicked),
			(gpointer)(wv_all_freqs->d[g_fund_set].
				   wv_editors->d[index]->amp_sliders->d[0]));
    }

  if (first_type)
    {
      Wv_Editor_Data *cur_editor = &(wv_all_freqs->d[index].fund_editor);
      update_slider_bases (GTK_ENTRY (fndfrq_mntisa), cur_editor, TRUE);
      update_slider_bases (GTK_ENTRY (amp_mntisa), cur_editor, FALSE);
    }
  else
    {
      Wv_Editor_Data *cur_editor =
	wv_all_freqs->d[g_fund_set].wv_editors->d[index];
      update_slider_bases (GTK_ENTRY (amp_mntisa), cur_editor, FALSE);
    }

  /* Store pointers to all widgets, for use by lookup_widget().  */
  /* GLADE_HOOKUP_OBJECT_NO_REF (wvedit_holder, wvedit_holder,
				 "wvedit_holder");
  GLADE_HOOKUP_OBJECT (wvedit_holder, wvedit_holder_frame,
		       "wvedit_holder_frame");
  GLADE_HOOKUP_OBJECT (wvedit_holder, wvedit_holder_vbox,
		       "wvedit_holder_vbox");
  GLADE_HOOKUP_OBJECT (wvedit_holder, fund_editor_hbox, "fund_editor_hbox");
  GLADE_HOOKUP_OBJECT (wvedit_holder, fund_editor_left_hbox,
		       "fund_editor_left_hbox");
  GLADE_HOOKUP_OBJECT (wvedit_holder, harmc_one_drop, "harmc_one_drop");
  GLADE_HOOKUP_OBJECT (wvedit_holder, mult_amps, "mult_amps");
  GLADE_HOOKUP_OBJECT (wvedit_holder, fundset_hbox, "fundset_hbox");
  GLADE_HOOKUP_OBJECT (wvedit_holder, fundset_label, "fundset_label");
  GLADE_HOOKUP_OBJECT (wvedit_holder, fundset_add, "fundset_add");
  GLADE_HOOKUP_OBJECT (wvedit_holder, fundset_remove, "fundset_remove");
  GLADE_HOOKUP_OBJECT (wvedit_holder, fund_freq_hbox, "fund_freq_hbox");
  GLADE_HOOKUP_OBJECT (wvedit_holder, fund_freq_left_hbox,
		       "fund_freq_left_hbox");
  GLADE_HOOKUP_OBJECT (wvedit_holder, fndfrq_label, "fndfrq_label");
  GLADE_HOOKUP_OBJECT (wvedit_holder, fndfrq_mntisa, "fndfrq_mntisa");
  GLADE_HOOKUP_OBJECT (wvedit_holder, fndfrq_exp_label, "fndfrq_exp_label");
  GLADE_HOOKUP_OBJECT (wvedit_holder, fndfrq_exp, "fndfrq_exp");
  GLADE_HOOKUP_OBJECT (wvedit_holder, fndfrq_precslid_change,
		       "fndfrq_precslid_change");
  GLADE_HOOKUP_OBJECT (wvedit_holder, fndfrq_precslid_label,
		       "fndfrq_precslid_label");
  GLADE_HOOKUP_OBJECT (wvedit_holder, fndfrq_precslid_add,
		       "fndfrq_precslid_add");
  GLADE_HOOKUP_OBJECT (wvedit_holder, fndfrq_precslid_remove,
		       "fndfrq_precslid_remove");
  GLADE_HOOKUP_OBJECT (wvedit_holder, fund_freq_slider_vbox,
		       "fund_freq_slider_vbox");
  GLADE_HOOKUP_OBJECT (wvedit_holder, hscrollbar2, "hscrollbar2");
  GLADE_HOOKUP_OBJECT (wvedit_holder, harmc_editor_hbox, "harmc_editor_hbox");
  GLADE_HOOKUP_OBJECT (wvedit_holder, harmc_editor_left_hbox,
		       "harmc_editor_left_hbox");
  GLADE_HOOKUP_OBJECT (wvedit_holder, harmc_sel_label, "harmc_sel_label");
  GLADE_HOOKUP_OBJECT (wvedit_holder, harmc_sel, "harmc_sel");
  GLADE_HOOKUP_OBJECT (wvedit_holder, harmc_window_hbox, "harmc_window_hbox");
  GLADE_HOOKUP_OBJECT (wvedit_holder, harmc_win_label, "harmc_win_label");
  GLADE_HOOKUP_OBJECT (wvedit_holder, harmc_win_add, "harmc_win_add");
  GLADE_HOOKUP_OBJECT (wvedit_holder, harmc_win_remove, "harmc_win_remove");
  GLADE_HOOKUP_OBJECT (wvedit_holder, harmc_change_hbox, "harmc_change_hbox");
  GLADE_HOOKUP_OBJECT (wvedit_holder, harmc_add_label, "harmc_add_label");
  GLADE_HOOKUP_OBJECT (wvedit_holder, harmc_add, "harmc_add");
  GLADE_HOOKUP_OBJECT (wvedit_holder, harmc_remove, "harmc_remove");
  GLADE_HOOKUP_OBJECT (wvedit_holder, amp_hbox, "amp_hbox");
  GLADE_HOOKUP_OBJECT (wvedit_holder, amp_left_hbox, "amp_left_hbox");
  GLADE_HOOKUP_OBJECT (wvedit_holder, amp_set_label, "amp_set_label");
  GLADE_HOOKUP_OBJECT (wvedit_holder, amp_mntisa, "amp_mntisa");
  GLADE_HOOKUP_OBJECT (wvedit_holder, amp_exp_label, "amp_exp_label");
  GLADE_HOOKUP_OBJECT (wvedit_holder, amp_exp, "amp_exp");
  GLADE_HOOKUP_OBJECT (wvedit_holder, amp_precslid_change,
		       "amp_precslid_change");
  GLADE_HOOKUP_OBJECT (wvedit_holder, amp_precslid_label,
		       "amp_precslid_label");
  GLADE_HOOKUP_OBJECT (wvedit_holder, amp_precslid_add, "amp_precslid_add");
  GLADE_HOOKUP_OBJECT (wvedit_holder, amp_precslid_remove,
		       "amp_precslid_remove");
  GLADE_HOOKUP_OBJECT (wvedit_holder, amp_slider_vbox, "amp_slider_vbox");
  GLADE_HOOKUP_OBJECT (wvedit_holder, hscrollbar1, "hscrollbar1"); */

  return wvedit_holder_frame;
}

/**
 * Creates the "Multiply Amplitudes" dialog.
 */
GtkWidget *
create_mult_amps_dialog (void)
{
  GtkWidget *mult_amps_dialog;
  GtkWidget *dialog_main_vbox;
  GtkWidget *mlt_amp_entry_pair;
  GtkWidget *mult_amp_label;
  GtkWidget *mult_amp_entry;

  mult_amps_dialog = gtk_dialog_new_with_buttons (_("Multiply Amplitudes"),
			  GTK_WINDOW (main_window),
			  GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			  GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
			  GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
  /* gtk_dialog_set_alternative_button_order (GTK_DIALOG (mult_amps_dialog),
				 GTK_RESPONSE_ACCEPT, GTK_RESPONSE_REJECT); */

  dialog_main_vbox = GTK_DIALOG (mult_amps_dialog)->vbox;
  gtk_widget_show (dialog_main_vbox);

  mlt_amp_entry_pair = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (mlt_amp_entry_pair);
  gtk_box_pack_start (GTK_BOX (dialog_main_vbox), mlt_amp_entry_pair, TRUE,
		      TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (mlt_amp_entry_pair), 5);

  mult_amp_label = gtk_label_new (_("Maximum amplitude: "));
  gtk_widget_show (mult_amp_label);
  gtk_box_pack_start (GTK_BOX (mlt_amp_entry_pair), mult_amp_label, FALSE,
		      FALSE, 0);

  mult_amp_entry = gtk_entry_new ();
  gtk_widget_show (mult_amp_entry);
  gtk_box_pack_start (GTK_BOX (mlt_amp_entry_pair), mult_amp_entry, TRUE,
		      TRUE, 0);
  gtk_entry_set_text (GTK_ENTRY (mult_amp_entry), "1.0");

  g_signal_connect ((gpointer) mult_amp_entry, "activate",
		    G_CALLBACK (mult_amp_entry_activate), NULL);
  g_signal_connect ((gpointer) mult_amp_entry, "focus-out-event",
		    G_CALLBACK (mult_amp_entry_focus_out), NULL);

  /* Store pointers to all widgets, for use by lookup_widget().  */
  GLADE_HOOKUP_OBJECT_NO_REF (mult_amps_dialog, mult_amps_dialog,
			      "mult_amps_dialog");
  GLADE_HOOKUP_OBJECT_NO_REF (mult_amps_dialog, dialog_main_vbox,
			      "dialog_main_vbox");
  GLADE_HOOKUP_OBJECT (mult_amps_dialog, mlt_amp_entry_pair,
		       "mlt_amp_entry_pair");
  GLADE_HOOKUP_OBJECT (mult_amps_dialog, mult_amp_label, "mult_amp_label");
  GLADE_HOOKUP_OBJECT (mult_amps_dialog, mult_amp_entry, "mult_amp_entry");

  return mult_amps_dialog;
}

/**
 * Adds a precision slider to the given editor.
 *
 * @param fund_editor if TRUE, then @a index specifies the frequency
 * sliders if zero and the amplitude sliders if one.  Otherwise, @a
 * index specifies which wave editor window to change.
 * @param index zero-based index or frequency versus amplitude
 * specifier
 */
void
add_prec_slider (gboolean fund_editor, unsigned index)
{
  GtkWidget *hscrollbar;
  GtkWidget *parent_box;
  Slide_Data *sd_block;

  hscrollbar =
    gtk_hscrollbar_new (GTK_ADJUSTMENT
			(gtk_adjustment_new (10, 0, 21, 0.1, 1.0, 1.0)));
  gtk_widget_show (hscrollbar);

  sd_block = (Slide_Data *) g_slice_alloc (sizeof (Slide_Data));
  sd_block->widget = hscrollbar;
  sd_block->last_value = 10;

  if (fund_editor)
    {
      if (index == 0)
	{
	  Wv_Editor_Data *cur_editor =
	    &wv_all_freqs->d[g_fund_set].fund_editor;
	  parent_box = cur_editor->freq_sliders_vbox;

	  sd_block->index = cur_editor->freq_sliders->len;
	  sd_block->fund_assoc = TRUE;
	  sd_block->parent_index = 0;
	  sd_block->base = -10.0 * pow(100, -((gdouble) sd_block->index));
	  if (sd_block->index > 0)
	    {
	      Slide_Data *last_slider =
		cur_editor->freq_sliders->d[sd_block->index-1];
	      gdouble value_mult =
		pow(100, -((gdouble) (sd_block->index - 1)));
	      sd_block->base +=
		last_slider->base + last_slider->last_value * value_mult;
	    }

	  g_array_append_val ((GArray *) cur_editor->freq_sliders, sd_block);
	}
      else
	{
	  Wv_Editor_Data *cur_editor =
	    &wv_all_freqs->d[g_fund_set].fund_editor;
	  parent_box = cur_editor->amp_sliders_vbox;

	  sd_block->index = cur_editor->amp_sliders->len;
	  sd_block->fund_assoc = TRUE;
	  sd_block->parent_index = 1;
	  sd_block->base = -10.0 * pow(100, -((gdouble) sd_block->index));
	  if (sd_block->index > 0)
	    {
	      Slide_Data *last_slider =
		cur_editor->amp_sliders->d[sd_block->index-1];
	      gdouble value_mult =
		pow(100, -((gdouble) (sd_block->index - 1)));
	      sd_block->base +=
		last_slider->base + last_slider->last_value * value_mult;
	    }

	  g_array_append_val ((GArray *) cur_editor->amp_sliders, sd_block);
	}
    }
  else
    {
      Wv_Editor_Data *cur_editor =
	wv_all_freqs->d[g_fund_set].wv_editors->d[index];
      parent_box = cur_editor->amp_sliders_vbox;

      sd_block->index = cur_editor->amp_sliders->len;
      sd_block->fund_assoc = FALSE;
      sd_block->parent_index = index;
      sd_block->base = -10.0 * pow(100, -((gdouble) sd_block->index));
      if (sd_block->index > 0)
	{
	  Slide_Data *last_slider =
	    cur_editor->amp_sliders->d[sd_block->index-1];
	  gdouble value_mult =
	    pow(100, -((gdouble) (sd_block->index - 1)));
	  sd_block->base +=
	    last_slider->base + last_slider->last_value * value_mult;
	}

      g_array_append_val ((GArray *) cur_editor->amp_sliders, sd_block);
    }
  gtk_box_pack_start (GTK_BOX (parent_box), hscrollbar, TRUE, TRUE, 0);
  g_signal_connect ((gpointer) hscrollbar, "value_changed",
		    G_CALLBACK (precslid_value_changed),
		    (gpointer) sd_block);
}

/**
 * Deletes a precision slider from the given editor.
 *
 * @param fund_editor if TRUE, then @a index specifies the frequency
 * sliders if zero and the amplitude sliders if one.  Otherwise, @a
 * index specifies which wave editor window to change.
 * @param index zero-based index or frequency versus amplitude
 * specifier
 */
void
remove_prec_slider (gboolean fund_editor, unsigned index)
{
  /* We will assume it to be unnecessary to nullify the pointer to the
     freed array entry, since it will be beyond the bounds of the
     array.  */
  if (fund_editor)
    {
      if (index == 0)
	{
	  Wv_Editor_Data *cur_editor;
	  unsigned last_slider;
	  cur_editor = &wv_all_freqs->d[g_fund_set].fund_editor;
	  last_slider = cur_editor->freq_sliders->len - 1;
	  gtk_widget_destroy (cur_editor->freq_sliders->d[last_slider]->
			      widget);
	  g_slice_free1 (sizeof (Slide_Data), cur_editor->
		freq_sliders->d[--cur_editor->freq_sliders->len]);
	}
      else
	{
	  Wv_Editor_Data *cur_editor;
	  unsigned last_slider;
	  cur_editor = &wv_all_freqs->d[g_fund_set].fund_editor;
	  last_slider = cur_editor->amp_sliders->len - 1;
	  gtk_widget_destroy (cur_editor->
			      amp_sliders->d[last_slider]->widget);
	  g_slice_free1 (sizeof (Slide_Data), cur_editor->
			 amp_sliders->d[--cur_editor->amp_sliders->len]);
	}
    }
  else
    {
      Wv_Editor_Data *cur_editor;
      unsigned last_slider;
      cur_editor = wv_all_freqs->d[g_fund_set].wv_editors->d[index];
      last_slider = cur_editor->amp_sliders->len - 1;
      gtk_widget_destroy (cur_editor->amp_sliders->d[last_slider]->widget);
      g_slice_free1 (sizeof (Slide_Data), cur_editor->
		     amp_sliders->d[--cur_editor->amp_sliders->len]);
    }
}

/**
 * Sets the foreground and background colors used by the wave
 * rendering area.
 */
void
set_render_colors (GdkColor * foreground, GdkColor * background)
{
  memcpy (&wr_foreground, foreground, sizeof (GdkColor));
  memcpy (&wr_background, background, sizeof (GdkColor));
  gtk_widget_modify_bg (wave_render, GTK_STATE_NORMAL, &wr_background);
  if (G_LIKELY (wr_gc != NULL))
    gdk_gc_set_rgb_fg_color (wr_gc, &wr_foreground);
}

/**
 * Display the manual for Slider Wave Editor.
 */
void
display_manual (void)
{
  gboolean file_error = FALSE;
  gchar *filename;
  FILE *fp;
  gchar *manual_text;

  GtkWidget *scrolled_window;
  GtkTextBuffer *text_buffer;
  GtkWidget *text_view;
  PangoFontDescription *font_desc;

  if (manual_window != NULL)
    {
      gtk_window_present (GTK_WINDOW (manual_window));
      return;
    }

  /* Read the manual from its file.  */
  filename = g_build_filename (package_data_dir,
			       PACKAGE, "help.txt", NULL);
  fp = fopen (filename, "r");
  if (fp == NULL)
    file_error = TRUE;
  else
    {
      long file_size;
      fseek (fp, 0, SEEK_END);
      file_size = ftell (fp);
      if (file_size < 0)
	file_error = TRUE;
      else
	{
	  size_t read_status;
	  manual_text = g_malloc (sizeof (gchar) * (file_size + 1));
	  fseek (fp, 0, SEEK_SET);
	  read_status = fread (manual_text, file_size, 1, fp);
	  manual_text[file_size] = '\0';
	  if (read_status != 1)
	    {
	      file_error = TRUE;
	      g_free (manual_text);
	    }
	}
      fclose (fp);
    }
  g_free (filename);

  if (file_error)
    manual_text = "Error: Could not read the manual file.\n";

  manual_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (manual_window),
			_("Slider Wave Editor Manual"));
  gtk_window_set_default_size (GTK_WINDOW (manual_window), 650, 600);

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (scrolled_window);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);
  gtk_container_add (GTK_CONTAINER (manual_window), scrolled_window);

  text_view = gtk_text_view_new ();
  gtk_widget_show (text_view);
  text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
  gtk_text_view_set_editable (GTK_TEXT_VIEW (text_view), FALSE);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (text_view), FALSE);
  font_desc = pango_font_description_from_string ("monospace 10");
  gtk_widget_modify_font (text_view, font_desc);
  pango_font_description_free (font_desc);
  gtk_text_buffer_set_text (text_buffer, manual_text, -1);
  gtk_container_add (GTK_CONTAINER (scrolled_window), text_view);

  g_signal_connect ((gpointer) manual_window, "destroy",
		    G_CALLBACK (manual_win_destroy), NULL);
  gtk_widget_show (manual_window);

  if (!file_error)
    g_free (manual_text);
}

/**
 * Display the about dialog for Slider Wave Editor.
 */
void
display_about_box (void)
{
  /* The license use to be loaded from the COPYING file, but now only
     an embedded copy will be used.  */
  const gchar *license =
"Slider Wave Editor is licensed under the Simplified BSD License.\n"
"\n"
"Copyright (C) 2011, 2012, 2013 Andrew Makousky\n"
"All rights reserved.\n"
"\n"
"Redistribution and use in source and binary forms, with or without\n"
"modification, are permitted provided that the following conditions are\n"
"met:\n"
"\n"
"  * Redistributions of source code must retain the above copyright\n"
"    notice, this list of conditions and the following disclaimer.\n"
"\n"
"  * Redistributions in binary form must reproduce the above copyright\n"
"    notice, this list of conditions and the following disclaimer in\n"
"    the documentation and/or other materials provided with the\n"
"    distribution.\n"
"\n"
"THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS\n"
"\"AS IS\" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT\n"
"LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR\n"
"A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT\n"
"HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,\n"
"SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT\n"
"LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,\n"
"DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY\n"
"THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n"
"(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n"
"OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n";

  GdkPixbuf *pixbuf = create_pixbuf ("icon192.png");

  gtk_show_about_dialog (GTK_WINDOW (main_window),
	 "program-name", _("Slider Wave Editor"),
	 "version", PACKAGE_VERSION,
	 "copyright", "Â© 2011, 2012, 2013 Andrew Makousky",
	 "license", license,
	 "comments",
_("Slider is an interactive program designed to help you synthesize sound " \
"effects by building them from harmonics and by combining multiple " \
"non-harmonic sine waves."),
	 "logo", pixbuf,
         "title", _("About Slider Wave Editor"),
	 NULL);

  g_object_unref (pixbuf);
}

void
interface_shutdown (void)
{
  g_free (loaded_fname);
  g_free (last_folder);
  gtk_widget_destroy (play_image); g_object_unref (play_image);
  gtk_widget_destroy (stop_image); g_object_unref (stop_image);
  g_object_unref (wr_gc);
}
