/* Circumvent problems with different Microsoft runtime versions and
   libintl *printf overrides.

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
#include <gtk/gtk.h>

#include "wv_editors.h"
#include "file_business.h"

/**
 * Isolates fprintf functions from save_sliw_project().
 */
void
do_save_printing (FILE * fp)
{
  unsigned i;
  unsigned j;
  for (i = 0; i < wv_all_freqs->len; i++)
    {
      fprintf (fp, "\nFundamental %u\n", i + 1);
      fprintf (fp, "Frequency: %g\n", wv_all_freqs->d[i].fund_freq);
      fprintf (fp, "Amplitude: %g\n", wv_all_freqs->d[i].amplitude);
      fputs ("Harmonics:", fp);
      for (j = 0; j < wv_all_freqs->d[i].harmonics->len; j++)
	{
	  fprintf (fp, " %u, %g;", wv_all_freqs->d[i].
		   harmonics->d[j].harmc_num,
		   wv_all_freqs->d[i].harmonics->d[j].amplitude);
	}
      fputs ("\n", fp);
    }
}

/**
 * Isolates fprintf functions from export_sliw_project().
 */
void
do_export_printing (FILE * fp)
{
  unsigned i;
  unsigned j;
  for (i = 0; i < wv_all_freqs->len; i++)
    {
      float fund_freq;
      fund_freq = wv_all_freqs->d[i].fund_freq;
      fprintf (fp, " (mult %g (hzosc %g))", wv_all_freqs->d[i].amplitude,
	       fund_freq);
      for (j = 0; j < wv_all_freqs->d[i].harmonics->len; j++)
	{
	  fprintf(fp, " (mult %g (hzosc %g))",
		  wv_all_freqs->d[i].harmonics->d[j].amplitude, fund_freq *
		  wv_all_freqs->d[i].harmonics->d[j].harmc_num);
	}
      if (i < wv_all_freqs->len - 1)
	fputs("\n", fp);
    }
}
