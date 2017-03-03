/* Output Graphviz specification of a state machine generated by Bison.

   Copyright (C) 2006-2007, 2009-2010 Free Software Foundation, Inc.

   This file is part of Bison, the GNU Compiler Compiler.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* Written by Paul Eggert and Satya Kiran Popuri.  */

#include <config.h>
#include "system.h"

#include <quotearg.h>

#include "graphviz.h"

/* Return an unambiguous printable representation for NAME, suitable
   for C strings.  Use slot 2 since the user may use slots 0 and 1.  */

static char const *
quote (char const *name)
{
  return quotearg_n_style (2, c_quoting_style, name);
}

void
start_graph (FILE *fout)
{
  fputs ("digraph Automaton {\n", fout);
}

void
output_node (int id, char const *label, FILE *fout)
{
  fprintf (fout, "  %d [label=%s]\n", id, quote (label));
}

void
output_edge (int source, int destination, char const *label,
	     char const *style, FILE *fout)
{
  fprintf (fout, "  %d -> %d [style=%s", source, destination, style);
  if (label)
    fprintf (fout, " label=%s", quote (label));
  fputs ("]\n", fout);
}

void
finish_graph (FILE *fout)
{
  fputs ("}\n", fout);
}
