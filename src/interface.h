/* Graphical user interface building function declarations.

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
 * Graphical user interface building function declarations.
 *
 * These functions do most of the work of generating GTK+ widgets.
 * However, they only do some of the work of configuring the widgets
 * to their proper values.  The rest of this work is taken care of by
 * wv_editors.c.
 */

#ifndef INTERFACE_H
#define INTERFACE_H

extern GtkWidget *main_window;
extern GtkWidget *wave_edit_cntr;
extern GtkWidget *cb_fund_set;
extern GtkWidget *wave_render;

GtkWidget *create_main_window (void);
GtkWidget *create_wvedit_holder (gboolean first_type, unsigned index);
GtkWidget *create_mult_amps_dialog (void);
void add_prec_slider (gboolean fund_editor, unsigned index);
void remove_prec_slider (gboolean fund_editor, unsigned index);

#endif /* not INTERFACE_H */
