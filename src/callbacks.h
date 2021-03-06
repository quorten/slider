/* GTK+ widget signal handlers.

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
 * GTK+ widget signal handlers.
 *
 * Most of the functions in this file are not deserving of very heavy
 * documentation.  All of the signal handlers are connected to their
 * signals in interface.h, and the function calling form of each
 * signal handler is dictated by the signal that it is connected to.
 * Therefore, it is mostly unnecessary to document the signal
 * handlers, as most of their behavior is explained in the GTK+
 * documentation.  However, the parameter @a user_data is documented
 * whenever it is expected to be a non-NULL value in any of the signal
 * handlers.
 */

#ifndef CALLBACKS_H
#define CALLBACKS_H

#include "wv_editors.h"

extern gchar *last_folder;
extern gchar *loaded_fname;
extern float max_ypt;

void manual_win_destroy (GtkObject * object, gpointer user_data);
gboolean check_save (gboolean closing);
gboolean save_as (void);
void open_file (void);
gboolean main_window_delete (GtkWidget * widget, gpointer user_data);
void activate_action (GtkAction * action);
gboolean
wavrnd_expose (GtkWidget * widget,
	       GdkEventExpose * event, gpointer user_data);
void b_save_as_clicked (GtkButton * button, gpointer user_data);
void b_play_clicked (GtkButton * button, gpointer user_data);
void agc_vol_changed (GtkRange * range, gpointer user_data);
void cb_fund_set_changed (GtkComboBox * combobox, gpointer user_data);
void
wv_edit_div_allocate (GtkWidget * widget,
		      GtkAllocation * allocation, gpointer user_data);
void harmc_one_drop_clicked (GtkButton * button, gpointer user_data);
void mult_amps_clicked (GtkButton * button, gpointer user_data);
void fundset_add_clicked (GtkButton * button, gpointer user_data);
void fundset_remove_clicked (GtkButton * button, gpointer user_data);
void fndfrq_mntisa_activate (GtkEntry * entry, gpointer user_data);
gboolean
fndfrq_mntisa_focus_out (GtkEntry * entry,
			 GdkEventFocus * event, gpointer user_data);
void
fndfrq_exp_value_changed (GtkSpinButton * spinbutton, gpointer user_data);
void
fndamp_mntisa_activate (GtkEntry * entry, gpointer user_data);
gboolean
fndamp_mntisa_focus_out (GtkEntry * entry,
			 GdkEventFocus * event, gpointer user_data);
void
fndamp_exp_value_changed (GtkSpinButton * spinbutton, gpointer user_data);
void harmc_sel_changed (GtkComboBox * combobox, gpointer user_data);
void harmc_add_clicked (GtkButton * button, gpointer user_data);
void harmc_remove_clicked (GtkButton * button, gpointer user_data);
void harmc_win_add_clicked (GtkButton * button, gpointer user_data);
void harmc_win_remove_clicked (GtkButton * button, gpointer user_data);
void amp_mntisa_activate (GtkEntry * entry, gpointer user_data);
gboolean
amp_mntisa_focus_out (GtkEntry * entry,
		      GdkEventFocus * event, gpointer user_data);
void amp_exp_value_changed (GtkSpinButton * spinbutton, gpointer user_data);
float sci_notation_get_value (GtkEntry * mntisa_widget,
			      GtkSpinButton * exp_widget);
void sci_notation_set_values (GtkEntry * mntisa_widget,
			      GtkSpinButton * exp_widget, float value);
void precslid_add_clicked (GtkButton * button, gpointer user_data);
void precslid_remove_clicked (GtkButton * button, gpointer user_data);
void precslid_value_changed (GtkHScrollbar * scrollbar, gpointer user_data);
void mult_amp_entry_activate (GtkEntry * entry, gpointer user_data);
gboolean mult_amp_entry_focus_out (GtkEntry * entry,
				   GdkEventFocus * event, gpointer user_data);
void update_slider_bases (GtkEntry * entry, Wv_Editor_Data * cur_data,
			  gboolean freq_sliders);

#endif /* not CALLBACKS_H */
