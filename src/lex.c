/* Token-reader for Bison's input parser,
   Copyright 1984, 1986, 1989, 1992, 2000, 2001 Free Software Foundation, Inc.

   This file is part of Bison, the GNU Compiler Compiler.

   Bison is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   Bison is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Bison; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "system.h"
#include "getargs.h"
#include "files.h"
#include "symtab.h"
#include "lex.h"
#include "complain.h"
#include "gram.h"
#include "quote.h"

/* Buffer for storing the current token.  */
struct obstack token_obstack;
const char *token_buffer = NULL;

bucket *symval = NULL;
int numval;

/* A token to be reread, see unlex and lex. */
static token_t unlexed = tok_undef;
static bucket *unlexed_symval = NULL;
static const char *unlexed_token_buffer = NULL;

void
lex_init (void)
{
  obstack_init (&token_obstack);
  unlexed = tok_undef;
}


void
lex_free (void)
{
  obstack_free (&token_obstack, NULL);
}


int
skip_white_space (void)
{
  int c;
  int inside;

  c = getc (finput);

  for (;;)
    {
      int cplus_comment;

      switch (c)
	{
	case '/':
	  /* FIXME: Should probably be merged with copy_comment.  */
	  c = getc (finput);
	  if (c != '*' && c != '/')
	    {
	      complain (_("unexpected `/' found and ignored"));
	      break;
	    }
	  cplus_comment = (c == '/');

	  c = getc (finput);

	  inside = 1;
	  while (inside)
	    {
	      if (!cplus_comment && c == '*')
		{
		  while (c == '*')
		    c = getc (finput);

		  if (c == '/')
		    {
		      inside = 0;
		      c = getc (finput);
		    }
		}
	      else if (c == '\n')
		{
		  lineno++;
		  if (cplus_comment)
		    inside = 0;
		  c = getc (finput);
		}
	      else if (c == EOF)
		fatal (_("unterminated comment"));
	      else
		c = getc (finput);
	    }

	  break;

	case '\n':
	  lineno++;

	case ' ':
	case '\t':
	case '\f':
	  c = getc (finput);
	  break;

	default:
	  return c;
	}
    }
}


/*-----------------------------------------------------.
| Do a getc, but give error message if EOF encountered |
`-----------------------------------------------------*/

static int
xgetc (FILE *f)
{
  int c = getc (f);
  if (c == EOF)
    fatal (_("unexpected end of file"));
  return c;
}


/*------------------------------------------------------------------.
| Read one literal character from finput.  Process \ escapes.       |
| Append the normalized string version of the char to OUT.  Assign  |
| the character code to *PCODE. Return 1 unless the character is an |
| unescaped `term' or \n report error for \n.                       |
`------------------------------------------------------------------*/

/* FIXME: We could directly work in the obstack, but that would make
   it more difficult to move to quotearg some day.  So for the time
   being, I prefer have literalchar behave like quotearg, and change
   my mind later if I was wrong.  */

static int
literalchar (struct obstack *out, int *pcode, char term)
{
  int c;
  char buf[4096];
  char *cp;
  int code;
  int wasquote = 0;

  c = xgetc (finput);
  if (c == '\n')
    {
      complain (_("unescaped newline in constant"));
      ungetc (c, finput);
      code = '?';
      wasquote = 1;
    }
  else if (c != '\\')
    {
      code = c;
      if (c == term)
	wasquote = 1;
    }
  else
    {
      c = xgetc (finput);
      if (c == 't')
	code = '\t';
      else if (c == 'n')
	code = '\n';
      else if (c == 'a')
	code = '\007';
      else if (c == 'r')
	code = '\r';
      else if (c == 'f')
	code = '\f';
      else if (c == 'b')
	code = '\b';
      else if (c == 'v')
	code = '\013';
      else if (c == '\\')
	code = '\\';
      else if (c == '\'')
	code = '\'';
      else if (c == '\"')
	code = '\"';
      else if (c <= '7' && c >= '0')
	{
	  code = 0;
	  while (c <= '7' && c >= '0')
	    {
	      code = (code * 8) + (c - '0');
	      if (code >= 256 || code < 0)
		{
		  complain (_("octal value outside range 0...255: `\\%o'"),
			    code);
		  code &= 0xFF;
		  break;
		}
	      c = xgetc (finput);
	    }
	  ungetc (c, finput);
	}
      else if (c == 'x')
	{
	  c = xgetc (finput);
	  code = 0;
	  while (1)
	    {
	      if (c >= '0' && c <= '9')
		code *= 16, code += c - '0';
	      else if (c >= 'a' && c <= 'f')
		code *= 16, code += c - 'a' + 10;
	      else if (c >= 'A' && c <= 'F')
		code *= 16, code += c - 'A' + 10;
	      else
		break;
	      if (code >= 256 || code < 0)
		{
		  complain (_("hexadecimal value above 255: `\\x%x'"), code);
		  code &= 0xFF;
		  break;
		}
	      c = xgetc (finput);
	    }
	  ungetc (c, finput);
	}
      else
	{
	  char badchar [] = "c";
	  badchar[0] = c;
	  complain (_("unknown escape sequence: `\\' followed by `%s'"),
		    quote (badchar));
	  code = '?';
	}
    }				/* has \ */

  /* now fill BUF with the canonical name for this character as a
     literal token.  Do not use what the user typed, so that `\012'
     and `\n' can be interchangeable.  */

  cp = buf;
  if (code == term && wasquote)
    *cp++ = code;
  else if (code == '\\')
    {
      *cp++ = '\\';
      *cp++ = '\\';
    }
  else if (code == '\'')
    {
      *cp++ = '\\';
      *cp++ = '\'';
    }
  else if (code == '\"')
    {
      *cp++ = '\\';
      *cp++ = '\"';
    }
  else if (code >= 040 && code < 0177)
    *cp++ = code;
  else if (code == '\t')
    {
      *cp++ = '\\';
      *cp++ = 't';
    }
  else if (code == '\n')
    {
      *cp++ = '\\';
      *cp++ = 'n';
    }
  else if (code == '\r')
    {
      *cp++ = '\\';
      *cp++ = 'r';
    }
  else if (code == '\v')
    {
      *cp++ = '\\';
      *cp++ = 'v';
    }
  else if (code == '\b')
    {
      *cp++ = '\\';
      *cp++ = 'b';
    }
  else if (code == '\f')
    {
      *cp++ = '\\';
      *cp++ = 'f';
    }
  else
    {
      *cp++ = '\\';
      *cp++ = code / 0100 + '0';
      *cp++ = ((code / 010) & 07) + '0';
      *cp++ = (code & 07) + '0';
    }
  *cp = '\0';

  if (out)
    obstack_sgrow (out, buf);
  *pcode = code;
  return !wasquote;
}


void
unlex (token_t token)
{
  unlexed = token;
  unlexed_token_buffer = token_buffer;
  unlexed_symval = symval;
}

/*-----------------------------------------------------------------.
| We just read `<' from FIN.  Store in TOKEN_BUFFER, the type name |
| specified between the `<...>'.                                   |
`-----------------------------------------------------------------*/

void
read_type_name (FILE *fin)
{
  int c = getc (fin);

  while (c != '>')
    {
      if (c == EOF)
	fatal (_("unterminated type name at end of file"));
      if (c == '\n')
	{
	  complain (_("unterminated type name"));
	  ungetc (c, fin);
	  break;
	}

      obstack_1grow (&token_obstack, c);
      c = getc (fin);
    }
  obstack_1grow (&token_obstack, '\0');
  token_buffer = obstack_finish (&token_obstack);
}


token_t
lex (void)
{
  int c;

  /* Just to make sure. */
  token_buffer = NULL;

  if (unlexed != tok_undef)
    {
      token_t res = unlexed;
      symval = unlexed_symval;
      token_buffer = unlexed_token_buffer;
      unlexed = tok_undef;
      return res;
    }

  c = skip_white_space ();

  switch (c)
    {
    case EOF:
      token_buffer = "EOF";
      return tok_eof;

    case 'A':    case 'B':    case 'C':    case 'D':    case 'E':
    case 'F':    case 'G':    case 'H':    case 'I':    case 'J':
    case 'K':    case 'L':    case 'M':    case 'N':    case 'O':
    case 'P':    case 'Q':    case 'R':    case 'S':    case 'T':
    case 'U':    case 'V':    case 'W':    case 'X':    case 'Y':
    case 'Z':
    case 'a':    case 'b':    case 'c':    case 'd':    case 'e':
    case 'f':    case 'g':    case 'h':    case 'i':    case 'j':
    case 'k':    case 'l':    case 'm':    case 'n':    case 'o':
    case 'p':    case 'q':    case 'r':    case 's':    case 't':
    case 'u':    case 'v':    case 'w':    case 'x':    case 'y':
    case 'z':
    case '.':    case '_':

      while (isalnum (c) || c == '_' || c == '.')
	{
	  obstack_1grow (&token_obstack, c);
	  c = getc (finput);
	}
      obstack_1grow (&token_obstack, '\0');
      token_buffer = obstack_finish (&token_obstack);
      ungetc (c, finput);
      symval = getsym (token_buffer);
      return tok_identifier;

    case '0':    case '1':    case '2':    case '3':    case '4':
    case '5':    case '6':    case '7':    case '8':    case '9':
      {
	numval = 0;

	while (isdigit (c))
	  {
	    obstack_1grow (&token_obstack, c);
	    numval = numval * 10 + c - '0';
	    c = getc (finput);
	  }
	obstack_1grow (&token_obstack, '\0');
	token_buffer = obstack_finish (&token_obstack);
	ungetc (c, finput);
	return tok_number;
      }

    case '\'':
      /* parse the literal token and compute character code in  code  */

      {
	int code;

	obstack_1grow (&token_obstack, '\'');
	literalchar (&token_obstack, &code, '\'');

	c = getc (finput);
	if (c != '\'')
	  {
	    int discode;
	    complain (_("use \"...\" for multi-character literal tokens"));
	    while (1)
	      if (!literalchar (0, &discode, '\''))
		break;
	  }
	obstack_1grow (&token_obstack, '\'');
	obstack_1grow (&token_obstack, '\0');
	token_buffer = obstack_finish (&token_obstack);
	symval = getsym (token_buffer);
	symval->class = token_sym;
	if (symval->user_token_number == SUNDEF)
	  symval->user_token_number = code;
	return tok_identifier;
      }

    case '\"':
      /* parse the literal string token and treat as an identifier */

      {
	int code;		/* ignored here */

	obstack_1grow (&token_obstack, '\"');
	/* Read up to and including ".  */
	while (literalchar (&token_obstack, &code, '\"'))
	  /* nothing */;
	obstack_1grow (&token_obstack, '\0');
	token_buffer = obstack_finish (&token_obstack);

	symval = getsym (token_buffer);
	symval->class = token_sym;

	return tok_identifier;
      }

    case ',':
      token_buffer = ",";
      return tok_comma;

    case ':':
      token_buffer = ":";
      return tok_colon;

    case ';':
      token_buffer = ";";
      return tok_semicolon;

    case '|':
      token_buffer = "|";
      return tok_bar;

    case '{':
      token_buffer = "{";
      return tok_left_curly;

    case '=':
      obstack_1grow (&token_obstack, c);
      do
	{
	  c = getc (finput);
	  obstack_1grow (&token_obstack, c);
	  if (c == '\n')
	    lineno++;
	}
      while (c == ' ' || c == '\n' || c == '\t');
      obstack_1grow (&token_obstack, '\0');
      token_buffer = obstack_finish (&token_obstack);

      if (c == '{')
	{
	  return tok_left_curly;
	}
      else
	{
	  ungetc (c, finput);
	  return tok_illegal;
	}

    case '<':
      read_type_name (finput);
      return tok_typename;

    case '%':
      return parse_percent_token ();

    default:
      obstack_1grow (&token_obstack, c);
      obstack_1grow (&token_obstack, '\0');
      token_buffer = obstack_finish (&token_obstack);
      return tok_illegal;
    }
}

/* the following table dictates the action taken for the various %
   directives.  A set_flag value causes the named flag to be set.  A
   retval action returns the code.  */
struct percent_table_struct
{
  const char *name;
  void *set_flag;
  token_t retval;
};

struct percent_table_struct percent_table[] =
{
  { "token",		NULL,			tok_token },
  { "term",		NULL,			tok_token },
  { "nterm",		NULL,			tok_nterm },
  { "type",		NULL,			tok_type },
  { "guard",		NULL,			tok_guard },
  { "union",		NULL,			tok_union },
  { "expect",		NULL,			tok_expect },
  { "thong",		NULL,			tok_thong },
  { "start",		NULL,			tok_start },
  { "left",		NULL,			tok_left },
  { "right",		NULL,			tok_right },
  { "nonassoc",		NULL,			tok_nonassoc },
  { "binary",		NULL,			tok_nonassoc },
  { "prec",		NULL,			tok_prec },
  { "locations",	&locations_flag,	tok_intopt },	/* -l */
  { "no-lines",		&no_lines_flag,		tok_intopt },	/* -l */
  { "raw",		NULL,			tok_obsolete },	/* -r */
  { "token-table",	&token_table_flag,	tok_intopt },	/* -k */
  { "yacc",		&yacc_flag,		tok_intopt },	/* -y */
  { "fixed-output-files",&yacc_flag,		tok_intopt },	/* -y */
  { "defines",		&defines_flag,		tok_intopt },	/* -d */
  { "no-parser",	&no_parser_flag,	tok_intopt },	/* -n */
  { "graph",		&graph_flag,		tok_intopt },	/* -g */

  /* FIXME: semantic parsers which will output an `include' of an
     output file: be sure that the name included is indeed the name of
     the output file.  */
  { "output",		&spec_outfile,		tok_stropt },	/* -o */
  { "file-prefix",	&spec_file_prefix,	tok_stropt },	/* -b */
  { "name-prefix",	&spec_name_prefix,	tok_stropt },	/* -p */

  { "verbose",		&verbose_flag,		tok_intopt },	/* -v */
  { "debug",		&debug_flag,		tok_intopt },	/* -t */
  { "semantic-parser",	&semantic_parser,	tok_intopt },
  { "pure-parser",	&pure_parser,		tok_intopt },

  { NULL, NULL, tok_illegal}
};

/* Parse a token which starts with %.
   Assumes the % has already been read and discarded.  */

token_t
parse_percent_token (void)
{
  struct percent_table_struct *tx = NULL;
  const char *arg = NULL;
  /* Where the ARG was found in token_buffer. */
  size_t arg_offset = 0;

  int c = getc (finput);
  obstack_1grow (&token_obstack, '%');
  obstack_1grow (&token_obstack, c);

  switch (c)
    {
    case '%':
      token_buffer = obstack_finish (&token_obstack);
      return tok_two_percents;

    case '{':
      token_buffer = obstack_finish (&token_obstack);
      return tok_percent_left_curly;

      /* The following guys are here for backward compatibility with
	 very ancient Yacc versions.  The paper of Johnson mentions
	 them (as ancient :).  */
    case '<':
      token_buffer = obstack_finish (&token_obstack);
      return tok_left;

    case '>':
      token_buffer = obstack_finish (&token_obstack);
      return tok_right;

    case '2':
      token_buffer = obstack_finish (&token_obstack);
      return tok_nonassoc;

    case '0':
      token_buffer = obstack_finish (&token_obstack);
      return tok_token;

    case '=':
      token_buffer = obstack_finish (&token_obstack);
      return tok_prec;
    }

  if (!isalpha (c))
    {
      token_buffer = obstack_finish (&token_obstack);
      return tok_illegal;
    }

  while (c = getc (finput), isalpha (c) || c == '_' || c == '-')
    {
      if (c == '_')
	c = '-';
      obstack_1grow (&token_obstack, c);
    }

  /* %DIRECTIVE="ARG".  Separate into
     TOKEN_BUFFER = `%DIRECTIVE\0ARG\0'.
     This is a bit hackish, but once we move to a Bison parser,
     things will be cleaned up.  */
  if (c == '=')
    {
      /* End of the directive.  We skip the `='. */
      obstack_1grow (&token_obstack, '\0');
      /* Fetch the ARG if present. */
      c = getc (finput);
      if (c == '"')
	{
	  int code;
	  arg_offset = obstack_object_size (&token_obstack);
	  /* Read up to and including `"'.  Do not append the closing
	     `"' in the output: it's not part of the ARG.  */
	  while (literalchar (NULL, &code, '"'))
	    obstack_1grow (&token_obstack, code);
	}
      /* else: should be an error. */
    }
  else
    ungetc (c, finput);

  obstack_1grow (&token_obstack, '\0');
  token_buffer = obstack_finish (&token_obstack);
  if (arg_offset)
    arg = token_buffer + arg_offset;

  /* table lookup % directive */
  for (tx = percent_table; tx->name; tx++)
    if (strcmp (token_buffer + 1, tx->name) == 0)
      break;

  if (arg && tx->retval != tok_stropt)
    fatal (_("`%s' supports no argument: %s"), token_buffer, quote (arg));

  switch (tx->retval)
    {
    case tok_stropt:
      assert (tx->set_flag);
      if (arg)
	{
	  /* Keep only the first assignment: command line options have
	     already been processed, and we want them to have
	     precedence.  Side effect: if this %-option is used
	     several times, only the first is honored.  Bah.  */
	  if (!*((char **) (tx->set_flag)))
	    *((char **) (tx->set_flag)) = xstrdup (arg);
	}
      else
	fatal (_("`%s' requires an argument"), token_buffer);
      return tok_noop;
      break;

    case tok_intopt:
      assert (tx->set_flag);
      *((int *) (tx->set_flag)) = 1;
      return tok_noop;
      break;

    case tok_obsolete:
      fatal (_("`%s' is no longer supported"), token_buffer);
      return tok_noop;
      break;

    default:
      return tx->retval;
      break;
    }
  abort ();
}
