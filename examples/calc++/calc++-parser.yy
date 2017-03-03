#line 7296 "../../doc/bison.texinfo"
%skeleton "lalr1.cc"                          /*  -*- C++ -*- */
%define "parser_class_name" "calcxx_parser"
%defines
%{
# include <string>
# include "calc++-driver.hh"
%}
#line 7312 "../../doc/bison.texinfo"
// The parsing context.
%parse-param { calcxx_driver& driver }
%lex-param   { calcxx_driver& driver }
#line 7325 "../../doc/bison.texinfo"
%locations
%initial-action
{
  // Initialize the initial location.
  @$.begin.filename = @$.end.filename = &driver.file;
};
#line 7339 "../../doc/bison.texinfo"
%debug
%error-verbose
#line 7349 "../../doc/bison.texinfo"
// Symbols.
%union
{
  int          ival;
  std::string *sval;
};
#line 7366 "../../doc/bison.texinfo"
%token        TOKEN_EOF      0 "end of file"
%token        TOKEN_ASSIGN     ":="
%token <sval> TOKEN_IDENTIFIER "identifier"
%token <ival> TOKEN_NUMBER     "number"
%type  <ival> exp              "expression"
#line 7379 "../../doc/bison.texinfo"
%printer    { debug_stream () << *$$; } "identifier"
%destructor { delete $$; } "identifier"

%printer    { debug_stream () << $$; } "number" "expression"
#line 7390 "../../doc/bison.texinfo"
%%
%start unit;
unit: assignments exp  { driver.result = $2; };

assignments: assignments assignment {}
           | /* Nothing. */         {};

assignment: TOKEN_IDENTIFIER ":=" exp { driver.variables[*$1] = $3; };

%left '+' '-';
%left '*' '/';
exp: exp '+' exp   { $$ = $1 + $3; }
   | exp '-' exp   { $$ = $1 - $3; }
   | exp '*' exp   { $$ = $1 * $3; }
   | exp '/' exp   { $$ = $1 / $3; }
   | TOKEN_IDENTIFIER  { $$ = driver.variables[*$1]; }
   | TOKEN_NUMBER      { $$ = $1; };
%%
#line 7416 "../../doc/bison.texinfo"
void
yy::calcxx_parser::error (const yy::calcxx_parser::location_type& l,
                          const std::string& m)
{
  driver.error (l, m);
}
