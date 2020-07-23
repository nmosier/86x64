            /* REFERNEC: https://www.lysator.liu.se/c/ANSI-C-grammar-l.html */

%{

#include <inttypes.h>
#include <string>
#include <istream>
#include "ast.hpp"
    /* #include "c.tab.hpp" */
#include ZC_PARSER_HEADER
    

#define YY_NO_UNPUT
#define yywrap() 1
#define YY_SKIP_YYWRAP

    extern std::istream *g_istream;

#undef YY_INPUT
#define YY_INPUT(buf, result, max_size)                     \
    g_istream->read((char *) buf, max_size);                \
    if ((result = g_istream->gcount()) < 0) {               \
        YY_FATAL_ERROR("read() in flex scanner failed");    \
    }
    
extern zc::SourceLoc g_lineno;
extern YYSTYPE yylval;

zc::ScopedTable<Symbol *,char> typenames;

bool comment(void);
char extract_char(const char *);

%}

D  [0-9]
L  [[:alpha:]_]
H  [[:hexdigit:]]
E  [Ee][+-]?{D}+
               /* OMITTED: FS -- float suffix */
               /* OMITTED: IS -- integer suffix */

%%

"/*" {
    if (comment()) {
        return ERROR;
    }
}
"void" { return VOID; }
"char" { return CHAR; }
"short" { return SHORT; }
"int" { return INT; }
"long" { return LONG; }
"struct" { return STRUCT; }
"union" { return UNION; }
"enum" { return ENUM; }
"signed" { return SIGNED; }
"unsigned" { return UNSIGNED; }

"sizeof" { return SIZEOF; }
"goto" { return GOTO; }
"continue" { return CONTINUE; }
"break" { return BREAK; }

"return" { return RETURN; }

"if" { return IF; }
"else" { return ELSE; }
"while" { return WHILE; }
"for" { return FOR; }

"auto" { return AUTO; }
"register" { return REGISTER; }
"static" { return STATIC; }
"extern" { return EXTERN; }
"typedef" { return TYPEDEF; }

"++" { return INC; }
"--" { return DEC; }

{L}({L}|{D})* {
    /* convert yytext to AST identifier */
    yylval.identifier = zc::Identifier::Create(std::string(yytext), g_lineno);
    if (typenames.Lookup(yylval.identifier->id())) {
        return TYPE_NAME;
    } else {
        return ID;
    }
   }

  /* incomplete */
{D}+ {
    yylval.expr = zc::LiteralExpr::Create(strtoimax(yytext, NULL, 10), g_lineno);
    return INT_CONST;
   }

   /* incomplete */
\'[^\\']\' {
        yylval.expr = zc::LiteralExpr::Create(extract_char(yytext), g_lineno);
        return CHAR_CONST;
    }
    
    /* incomplete, string literal */
\"[^\\"]*\" {
        yylval.expr = zc::StringExpr::Create(std::string(yytext), g_lineno);
        return STRING;
}

"&&" { return AND; }
"||" { return OR; }
"<=" { return LEQ; }
"==" { return EQ; }
">=" { return GEQ; }
"!=" { return NEQ; }
"->" { return ARROW; }

    /* special scope tokens */
"{" { typenames.EnterScope(); return '{'; }
"}" { typenames.ExitScope(); return '}'; }

    /* single character tokens */
[;,:=()[\].&!~\-+*/%<>^|?] { return yytext[0]; }

\n { ++g_lineno; }
[ \t\v\f]  { /* ignore */ }

. { yylval.error_msg = "invalid character"; return ERROR; }
    
%%

bool comment(void) {
    char cur;
    char prev;

    for (prev = '\0', cur = yyinput();
         cur && !(prev == '*' && cur == '/');
         prev = cur, cur = yyinput()) {
        if (cur == '\n') {
            ++g_lineno;
        }
    }

    if (!cur) {
        yylval.error_msg = "reached end-of-line while scanning multiline comment";
        return false;
    }

    return true;
}

    /* extract character from yytext for character constants */
char extract_char(const char *str) {
    return str[1];
}
