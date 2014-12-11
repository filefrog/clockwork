%{
/*
  Copyright 2011-2014 James Hunt <james@jameshunt.us>

  This file is part of Clockwork.

  Clockwork is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Clockwork is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Clockwork.  If not, see <http://www.gnu.org/licenses/>.
 */

/**

  grammar.y - Reentrant (pure) Bison LALR Parser

  This file defines the productions necessary to interpret
  tokens found by the lexical analyzer, and subsquently build
  a valid abstract syntax tree to describe policy generators.

 */
#include "private.h"
%}

/* Build a reentrant Bison parser */
%define api.pure
%parse-param {spec_parser_context *ctx}
%lex-param   {YYLEX_PARAM}

/* Define the lexical tokens used by the grammar.
      These definitions will be available to the lexer via the
      grammar.h header file, which is generated by bison */ 
%token T_KEYWORD_POLICY
%token T_KEYWORD_HOST
%token T_KEYWORD_ENFORCE
%token T_KEYWORD_EXTEND
%token T_KEYWORD_IF
%token T_KEYWORD_UNLESS
%token T_KEYWORD_ELSE
%token T_KEYWORD_MAP
%token T_KEYWORD_DEFAULT
%token T_KEYWORD_AND
%token T_KEYWORD_OR
%token T_KEYWORD_IS
%token T_KEYWORD_NOT
%token T_KEYWORD_LIKE
%token T_KEYWORD_DOUBLE_EQUAL
%token T_KEYWORD_BANG_EQUAL
%token T_KEYWORD_EQUAL_TILDE
%token T_KEYWORD_BANG_TILDE
%token T_KEYWORD_DEPENDS_ON
%token T_KEYWORD_AFFECTS
%token T_KEYWORD_DEFAULTS
%token T_KEYWORD_FALLBACK
%token T_KEYWORD_ALLOW
%token T_KEYWORD_DENY
%token T_KEYWORD_FINAL
%token T_KEYWORD_ALL

/* These token definitions identify the expected type of the lvalue.
   The name 'string' comes from the union members of the YYSTYPE
   union, defined in private.h

   N.B.: I deliberately do not use the %union construct provided by
   bison, opting to define the union myself in private.h.  If one of
   the possible lvalue types is not a basic type (like char*, int, etc.)
   then lexer is required to include the necessary header files. */
%token <string> T_ACLGROUP
%token <string> T_IDENTIFIER
%token <string> T_FACT
%token <string> T_QSTRING
%token <string> T_NUMERIC
%token <string> T_REGEX

/* Define the lvalue types of non-terminal productions.
   These definitions are necessary so that the $1..$n and $$ "magical"
   variables work in the generated C code. */
%type <manifest> manifest
%type <stree> host policy
%type <stree> enforcing enforce
%type <stree> blocks block
%type <stree> resource extension
%type <stree> conditional alt_condition
%type <stree> conditional_enforce alt_condition_enforce
%type <stree> attributes attribute optional_attributes
%type <stree> dependency resource_id
%type <stree> expr simple_expr lvalue rvalue regex map_rvalue
%type <string> qstring literal_value
%type <map> map map_conds
%type <map_cond> map_cond map_default
%type <stree> acl acl_subject
%type <string> acl_disposition acl_command
%{
#ifdef YYDEBUG
int yydebug = 1;
#endif

#define MANIFEST(ctx) (((spec_parser_context*)ctx)->root)
#define NODE(op,d1,d2) (manifest_new_stree(MANIFEST(ctx), (op), (d1), (d2)))
#define EXPR(t,n1,n2) manifest_new_stree_expr(MANIFEST(ctx), EXPR_ ## t, (n1), (n2))
#define NEGATE(n) manifest_new_stree_expr(MANIFEST(ctx), EXPR_NOT, (n), NULL)

static struct stree* s_regex(struct manifest *m, const char *literal)
{
	char *re, *d, delim, *opts = NULL;
	const char *p;
	int esc = 0;

	d = re = vmalloc(strlen(literal) + 1);
	p = literal;
	if (*p == 'm') p++;
	delim = *p++;

	for (; *p; p++) {
		if (esc) {
			if (*p != delim)
				*d++ = '\\';
			*d++ = *p;
			esc = 0;
			continue;
		}

		if (*p == '\\') {
			esc = 1;
			continue;
		}

		if (*p == delim) {
			opts = strdup(p+1);
			break;
		}

		*d++ = *p;
	}

	if (!opts) opts = strdup("");
	return manifest_new_stree(m, EXPR_REGEX, re, opts);
}

%}

%%

manifest:
		{ MANIFEST(ctx) = manifest_new(); }
	| manifest host
		{ stree_add(MANIFEST(ctx)->root, $2);
		  if ($2->data1) {
			hash_set(MANIFEST(ctx)->hosts, $2->data1, $2);
		  } else {
			MANIFEST(ctx)->fallback = $2;
		  } }
	| manifest policy
		{ stree_add(MANIFEST(ctx)->root, $2);
		  hash_set(MANIFEST(ctx)->policies, $2->data1, $2); }
	;

host: T_KEYWORD_HOST qstring '{' enforcing '}'
		{ $$ = $4;
		  $$->op = HOST;
		  $$->data1 = $2; }
	| T_KEYWORD_HOST T_KEYWORD_FALLBACK '{' enforcing '}'
		{ $$ = $4;
		  $$->op = HOST;
		  $$->data1 = NULL; }
	;

enforcing:
		{ $$ = NODE(PROG, NULL, NULL); }
	| enforcing enforce
		{ stree_add($$, $2); }
	| enforcing conditional_enforce
		{ stree_add($$, $2); }
	;

enforce: T_KEYWORD_ENFORCE qstring
		{ $$ = NODE(INCLUDE, $2, NULL); }
	;

conditional_enforce: T_KEYWORD_IF '(' expr ')' '{' enforcing '}' alt_condition_enforce
		{ $$ = NODE(IF, NULL, NULL);
		  stree_add($$, $3);
		  stree_add($$, $6);
		  stree_add($$, $8); }
	| T_KEYWORD_UNLESS '(' expr ')' '{' enforcing '}' alt_condition_enforce
		{ $$ = NODE(IF, NULL, NULL);
		  stree_add($$, NEGATE($3));
		  stree_add($$, $6);
		  stree_add($$, $8); }
	;

alt_condition_enforce:
		{ $$ = NODE(NOOP, NULL, NULL); }
	| T_KEYWORD_ELSE '{' enforcing '}'
		{ $$ = $3; }
	| T_KEYWORD_ELSE conditional_enforce
		{ $$ = $2; }
	;

policy: T_KEYWORD_POLICY qstring '{' blocks '}'
		{ $$ = $4;
		  $$->op = POLICY;
		  $$->data1 = $2; }
	;

blocks:
		{ $$ = NODE(PROG, NULL, NULL); }
	| blocks block
		{ stree_add($$, $2); }
	;

block: resource | conditional | extension | dependency | acl
	;

resource: T_IDENTIFIER literal_value optional_attributes
		{ $$ = $3;
		  $$->op = RESOURCE;
		  $$->data1 = $1;
		  $$->data2 = $2; }
	| T_IDENTIFIER T_KEYWORD_DEFAULTS '{' attributes '}'
		{ $$ = $4;
		  $$->op = RESOURCE;
		  $$->data1 = $1;
		  $$->data2 = NULL; }
	| T_KEYWORD_HOST literal_value optional_attributes
		{ $$ = $3;
		  $$->op = RESOURCE;
		  $$->data1 = cw_strdup("host"); /* dynamic string for stree_free */
		  $$->data2 = $2; }
	| T_KEYWORD_HOST T_KEYWORD_DEFAULTS '{' attributes '}'
		{ $$ = $4;
		  $$->op = RESOURCE;
		  $$->data1 = cw_strdup("host"); /* dynamic string for stree_free */
		  $$->data2 = NULL; }
	;

optional_attributes:
		{ $$ = NODE(PROG, NULL, NULL); }
	| '{' attributes '}'
		{ $$ = $2; }
	;


attributes:
		{ $$ = NODE(PROG, NULL, NULL); }
	| attributes attribute
		{ stree_add($$, $2); }
	;

attribute: T_IDENTIFIER ':' literal_value
		{ $$ = NODE(ATTR, $1, $3); }
	| T_IDENTIFIER ':' map
		{
			struct stree *n;
			parser_map_cond *c, *tmp;
			$$ = NULL;

			for_each_object_safe_r(c, tmp, &$3->cond, l) {
				if (c->rhs) {
					if (c->rhs) {
						n = NODE(IF, NULL, NULL);
						stree_add(n, c->rhs->op == EXPR_REGEX
								? EXPR(MATCH, $3->lhs, c->rhs)
								: EXPR(EQ,    $3->lhs, c->rhs));
						stree_add(n, NODE(ATTR, strdup($1), c->value));
					} else {
						n = NODE(ATTR, strdup($1), c->value);
					}
					if ($$) stree_add(n, $$);
					$$ = n;
				} else {
					$$ = NODE(ATTR, strdup($1), c->value);
				}
				free(c);
			}
			free($1);
			free($3);
		}
	| T_KEYWORD_DEPENDS_ON resource_id
		{ $$ = NODE(LOCAL_DEP, NULL, NULL);
		  stree_add($$, $2); }
	| T_KEYWORD_AFFECTS    resource_id
		{ $$ = NODE(LOCAL_REVDEP, NULL, NULL);
		  stree_add($$, $2); }
	;

literal_value: qstring | T_NUMERIC
	;

conditional: T_KEYWORD_IF '(' expr ')' '{' blocks '}' alt_condition
		{ $$ = NODE(IF, NULL, NULL);
		  stree_add($$, $3);
		  stree_add($$, $6);
		  stree_add($$, $8); }
	| T_KEYWORD_UNLESS '(' expr ')' '{' blocks '}' alt_condition
		{ $$ = NODE(IF, NULL, NULL);
		  stree_add($$, NEGATE($3));
		  stree_add($$, $6);
		  stree_add($$, $8); }
	;

alt_condition:
		{ $$ = NODE(NOOP, NULL, NULL); }
	| T_KEYWORD_ELSE '{' blocks '}'
		{ $$ = $3; }
	| T_KEYWORD_ELSE conditional
		{ $$ = $2; }
	;

expr: simple_expr
	| '(' expr ')' { $$ = EXPR(NOOP, $2, NULL); }
	| T_KEYWORD_NOT expr { $$ = NEGATE($2); }
	| expr T_KEYWORD_AND expr { $$ = EXPR(AND, $1, $3); }
	| expr T_KEYWORD_OR  expr { $$ = EXPR(OR,  $1, $3); }
	;

expr_eq: T_KEYWORD_IS | T_KEYWORD_DOUBLE_EQUAL ;

expr_not_eq: T_KEYWORD_IS T_KEYWORD_NOT | T_KEYWORD_BANG_EQUAL ;

expr_like: T_KEYWORD_LIKE | T_KEYWORD_EQUAL_TILDE ;

expr_not_like: T_KEYWORD_NOT T_KEYWORD_LIKE | T_KEYWORD_BANG_TILDE ;

simple_expr: lvalue expr_eq rvalue
		{ $$ = EXPR(EQ, $1, $3); }
	| lvalue expr_not_eq rvalue
		{ $$ = NEGATE(EXPR(EQ, $1, $3)); }

	| lvalue expr_like regex
		{ $$ = EXPR(MATCH, $1, $3); }
	| lvalue expr_not_like regex
		{ $$ = NEGATE(EXPR(MATCH, $1, $3)); }
	;

lvalue: literal_value
		{ $$ = NODE(EXPR_VAL, $1, NULL); }
	| T_FACT
		{ $$ = NODE(EXPR_FACT, $1, NULL); }
	;

rvalue: lvalue
	;

regex: T_REGEX
		{ $$ = s_regex(MANIFEST(ctx), $1); free($1); }
	;

extension: T_KEYWORD_EXTEND qstring
		{ $$ = NODE(INCLUDE, $2, NULL); }
	;

dependency: resource_id T_KEYWORD_DEPENDS_ON resource_id
		{ $$ = NODE(DEPENDENCY, NULL, NULL);
		  stree_add($$, $1);
		  stree_add($$, $3); }
	  | resource_id T_KEYWORD_AFFECTS    resource_id
		{ $$ = NODE(DEPENDENCY, NULL, NULL);
		  stree_add($$, $3);
		  stree_add($$, $1); }
	  ;

resource_id: T_IDENTIFIER '(' literal_value ')'
			{ $$ = NODE(RESOURCE_ID, $1, $3); }

map: T_KEYWORD_MAP '(' T_FACT ')' '{' map_conds map_default '}'
		{ $$ = $6;
		  if ($7) list_push(&$$->cond, &$7->l);
		  $6->lhs = NODE(EXPR_FACT, $3, NULL); }
	;

map_conds:
		{ $$ = vmalloc(sizeof(parser_map));
		  list_init(&$$->cond); }
	| map_conds map_cond
		{ list_push(&$$->cond, &$2->l); }
	;

map_rvalue: rvalue | regex ;

map_cond: map_rvalue ':' literal_value
		{ $$ = vmalloc(sizeof(parser_map_cond));
		  list_init(&$$->l);
		  $$->rhs   = $1;
		  $$->value = $3; }
	;

map_else: T_KEYWORD_ELSE | T_KEYWORD_DEFAULT
	;

map_default:
		{ $$ = NULL; }
	| map_else ':' literal_value
		{ $$ = vmalloc(sizeof(parser_map_cond));
		  list_init(&$$->l);
		  $$->value = $3; }
	;

acl:
	  acl_disposition acl_subject acl_command
		{ $$ = NODE(ACL, $1, strdup("continue"));
		  stree_add($$, $2);
		  stree_add($$, NODE(ACL_COMMAND, $3, NULL)); }
	| acl_disposition acl_subject acl_command T_KEYWORD_FINAL
		{ $$ = NODE(ACL, $1, strdup("final"));
		  stree_add($$, $2);
		  stree_add($$, NODE(ACL_COMMAND, $3, NULL)); }
	;

acl_disposition:
	  T_KEYWORD_ALLOW { $$ = strdup("allow"); }
	| T_KEYWORD_DENY  { $$ = strdup("deny");  }
	;

acl_subject:
	  T_ACLGROUP    { $$ = NODE(ACL_SUBJECT, NULL, $1); }
	| T_IDENTIFIER  { $$ = NODE(ACL_SUBJECT, $1, NULL); }
	;

acl_command: T_KEYWORD_ALL { $$ = strdup("*"); }
	| T_QSTRING
	| T_IDENTIFIER
	;

qstring: T_QSTRING
	| T_IDENTIFIER
		{ spec_parser_warning(YYPARSE_PARAM, "unexpected identifier '%s', expected quoted string literal", $1); }
	;
