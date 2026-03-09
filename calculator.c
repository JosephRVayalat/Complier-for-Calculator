/*
 ============================================================
  Simple Calculator Language Compiler  –  Written in C
  Phases:
    1. Lexical Analysis  (Lexer / Tokenizer)
    2. Syntax Analysis   (LL(1) Recursive-Descent Parser)
    3. Intermediate Code (Postfix / RPN Generation)
    4. Evaluation        (Postfix Stack Evaluator)
 ============================================================

  Grammar (after left-recursion removal & left-factoring):

    stmt   -> ID = expr | expr
    expr   -> term expr'
    expr'  -> + term expr'  |  - term expr'  |  ε
    term   -> factor term'
    term'  -> * factor term' |  / factor term'  |  ε
    factor -> NUM  |  ID  |  ( expr )

  Regular Expressions for tokens:
    NUM    : [0-9]+
    ID     : [a-zA-Z_][a-zA-Z0-9_]*
    OP     : [+\-*\/]
    ASSIGN : =
    LPAREN : \(
    RPAREN : \)
 ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

//SECTION 1 – TOKEN DEFINITIONS
typedef enum {
    TOK_NUM, TOK_ID, TOK_OP, TOK_ASSIGN,
    TOK_LPAREN, TOK_RPAREN, TOK_EOF, TOK_UNKNOWN
} TokenType;

typedef struct {
    TokenType type;
    char      value[64];   /* string value of token */
    int       num_val;     /* numeric value (for NUM tokens) */
} Token;

//SECTION 2 – POSTFIX (RPN) OUTPUT BUFFER
#define MAX_POSTFIX 256

typedef struct {
    TokenType type;
    char      value[64];
    int       num_val;
} PostfixToken;

PostfixToken postfix[MAX_POSTFIX];
int          postfix_len = 0;

void emit(TokenType type, const char *value, int num_val) {
    if (postfix_len >= MAX_POSTFIX) {
        fprintf(stderr, "Postfix buffer overflow\n");
        exit(1);
    }
    postfix[postfix_len].type    = type;
    postfix[postfix_len].num_val = num_val;
    strncpy(postfix[postfix_len].value, value, 63);
    postfix[postfix_len].value[63] = '\0';
    postfix_len++;
}

//SECTION 2b – PARSE TRACE TABLE  (Stack | Input | Action)
#define MAX_TRACE   512
#define MAX_PSTACK  128

/* ── virtual parse stack (symbol names) ── */
char parse_stack[MAX_PSTACK][32];
int  parse_stack_top = -1;

void ps_push(const char *sym) {
    if (parse_stack_top < MAX_PSTACK - 1)
        strncpy(parse_stack[++parse_stack_top], sym, 31);
}
void ps_pop(void) {
    if (parse_stack_top >= 0) parse_stack_top--;
}

/* snapshot of one trace row */
typedef struct {
    char stack_str[256];
    char input_str[256];
    char action_str[128];
} TraceRow;

TraceRow trace_table[MAX_TRACE];
int      trace_len = 0;

/* build a string of the remaining input tokens */
static void remaining_input_str(char *buf, int bufsz) {
    buf[0] = '\0';
    int pos = 0;
    /* 'cur' is the global parser cursor */
    extern int cur;
    extern Token token_stream[];
    extern int   token_count;
    for (int i = cur; i < token_count; i++) {
        if (token_stream[i].type == TOK_EOF) break;
        int n = snprintf(buf + pos, bufsz - pos, "%s ", token_stream[i].value);
        if (n < 0 || pos + n >= bufsz) break;
        pos += n;
    }
    if (pos == 0) strncpy(buf, "$", bufsz);
    else { /* trim trailing space */
        if (buf[pos-1] == ' ') buf[pos-1] = '\0';
    }
}

/* build a string of the current parse stack */
static void parse_stack_str(char *buf, int bufsz) {
    buf[0] = '\0';
    int pos = 0;
    /* bottom → top, left → right */
    for (int i = 0; i <= parse_stack_top; i++) {
        int n = snprintf(buf + pos, bufsz - pos, "%s ", parse_stack[i]);
        if (n < 0 || pos + n >= bufsz) break;
        pos += n;
    }
    if (pos == 0) strncpy(buf, "$", bufsz);
    else { if (buf[pos-1] == ' ') buf[pos-1] = '\0'; }
}

/* record one trace row */
void trace(const char *action) {
    if (trace_len >= MAX_TRACE) return;
    parse_stack_str    (trace_table[trace_len].stack_str, 256);
    remaining_input_str(trace_table[trace_len].input_str, 256);
    strncpy(trace_table[trace_len].action_str, action, 127);
    trace_len++;
}

/* print the collected trace as a formatted table */
void print_parse_trace(void) {
    /* compute column widths */
    int ws = 5, wi = 5, wa = 6;
    for (int i = 0; i < trace_len; i++) {
        int ls = (int)strlen(trace_table[i].stack_str);
        int li = (int)strlen(trace_table[i].input_str);
        int la = (int)strlen(trace_table[i].action_str);
        if (ls > ws) ws = ls;
        if (li > wi) wi = li;
        if (la > wa) wa = la;
    }
    /* cap widths so the table fits reasonably */
    if (ws > 40) ws = 40;
    if (wi > 30) wi = 30;
    if (wa > 45) wa = 45;

    int total = ws + wi + wa + 10;
    printf("\n PARSE TRACE  (Stack | Input | Action)\n");
    for (int i = 0; i < total; i++) putchar('-'); putchar('\n');
    printf(" %-*s | %-*s | %-*s\n", ws, "STACK", wi, "INPUT", wa, "ACTION");
    for (int i = 0; i < total; i++) putchar('-'); putchar('\n');
    for (int i = 0; i < trace_len; i++) {
        printf(" %-*.*s | %-*.*s | %-*.*s\n",
               ws, ws, trace_table[i].stack_str,
               wi, wi, trace_table[i].input_str,
               wa, wa, trace_table[i].action_str);
    }
    for (int i = 0; i < total; i++) putchar('-'); putchar('\n');
}

//SECTION 3 – LEXER
#define MAX_TOKENS 512

Token  token_stream[MAX_TOKENS];
int    token_count = 0;

const char *token_type_name(TokenType t) {
    switch (t) {
        case TOK_NUM:    return "NUM";
        case TOK_ID:     return "ID";
        case TOK_OP:     return "OP";
        case TOK_ASSIGN: return "ASSIGN";
        case TOK_LPAREN: return "LPAREN";
        case TOK_RPAREN: return "RPAREN";
        case TOK_EOF:    return "EOF";
        default:         return "UNKNOWN";
    }
}

/* tokenize: fill token_stream[] from input string */
void tokenize(const char *src) {
    int i = 0;
    token_count = 0;

    while (src[i] != '\0') {
        /* skip whitespace */
        if (isspace((unsigned char)src[i])) { i++; continue; }

        Token tok;
        memset(&tok, 0, sizeof(tok));

        /* ── NUM ── */
        if (isdigit((unsigned char)src[i])) {
            int j = 0;
            while (isdigit((unsigned char)src[i]))
                tok.value[j++] = src[i++];
            tok.value[j] = '\0';
            tok.type    = TOK_NUM;
            tok.num_val = atoi(tok.value);
        }
        /* ── ID ── */
        else if (isalpha((unsigned char)src[i]) || src[i] == '_') {
            int j = 0;
            while (isalnum((unsigned char)src[i]) || src[i] == '_')
                tok.value[j++] = src[i++];
            tok.value[j] = '\0';
            tok.type = TOK_ID;
        }
        /* ── ASSIGN (must check before OP set) ── */
        else if (src[i] == '=') {
            tok.type = TOK_ASSIGN;
            tok.value[0] = '='; tok.value[1] = '\0';
            i++;
        }
        /* ── OP ── */
        else if (src[i] == '+' || src[i] == '-' ||
                 src[i] == '*' || src[i] == '/') {
            tok.type = TOK_OP;
            tok.value[0] = src[i++]; tok.value[1] = '\0';
        }
        /* ── LPAREN ── */
        else if (src[i] == '(') {
            tok.type = TOK_LPAREN;
            tok.value[0] = '('; tok.value[1] = '\0';
            i++;
        }
        /* ── RPAREN ── */
        else if (src[i] == ')') {
            tok.type = TOK_RPAREN;
            tok.value[0] = ')'; tok.value[1] = '\0';
            i++;
        }
        else {
            fprintf(stderr, "Lexer error: unexpected character '%c'\n", src[i]);
            exit(1);
        }

        if (token_count >= MAX_TOKENS - 1) {
            fprintf(stderr, "Token buffer overflow\n"); exit(1);
        }
        token_stream[token_count++] = tok;
    }

    /* sentinel EOF token */
    Token eof;
    memset(&eof, 0, sizeof(eof));
    eof.type = TOK_EOF;
    strcpy(eof.value, "EOF");
    token_stream[token_count++] = eof;
}

//SECTION 4 – PARSER  (LL(1) Recursive Descent) + Syntax-Directed Postfix Emission
int cur = 0;   /* index into token_stream */

/* forward declarations */
void stmt(void);
void expr(void);
void expr_prime(void);
void term(void);
void term_prime(void);
void factor(void);

static Token *peek(void)          { return &token_stream[cur]; }
static Token  consume(void)       { return token_stream[cur++]; }
static void   expect(TokenType t) {
    if (token_stream[cur].type != t) {
        fprintf(stderr,
            "Parse error: expected %s, got %s('%s')\n",
            token_type_name(t),
            token_type_name(token_stream[cur].type),
            token_stream[cur].value);
        exit(1);
    }
}

void stmt(void) {
    ps_push("stmt");
    if (peek()->type == TOK_ID &&
        token_stream[cur + 1].type == TOK_ASSIGN) {

        trace("stmt -> ID = expr");
        Token id = consume();          /* consume ID  */
        ps_pop(); ps_push("expr"); ps_push("="); ps_push(id.value);
        trace("match ID");
        ps_pop();                      /* pop matched ID symbol */
        consume();                     /* consume '=' */
        trace("match =");
        ps_pop();                      /* pop '=' symbol */
        expr();
        emit(TOK_ID,     id.value, 0);
        emit(TOK_ASSIGN, "=",      0);
    } else {
        trace("stmt -> expr");
        ps_pop(); ps_push("expr");
        expr();
    }
    ps_pop();
}

void expr(void) {
    ps_push("expr");
    trace("expr -> term expr'");
    ps_pop(); ps_push("expr'"); ps_push("term");
    term();
    expr_prime();
    ps_pop();
}

void expr_prime(void) {
    Token *t = peek();
    if (t->type == TOK_OP &&
        (t->value[0] == '+' || t->value[0] == '-')) {
        char op[2]; op[0] = t->value[0]; op[1] = '\0';
        char act[64]; snprintf(act, sizeof(act), "expr' -> %s term expr'", op);
        ps_push("expr'");
        trace(act);
        ps_pop(); ps_push("expr'"); ps_push("term"); ps_push(op);
        consume();
        trace("match op");
        ps_pop();
        term();
        emit(TOK_OP, op, 0);
        expr_prime();
        ps_pop();
    } else {
        ps_push("expr'");
        trace("expr' -> epsilon");
        ps_pop();
    }
}

void term(void) {
    ps_push("term");
    trace("term -> factor term'");
    ps_pop(); ps_push("term'"); ps_push("factor");
    factor();
    term_prime();
    ps_pop();
}

void term_prime(void) {
    Token *t = peek();
    if (t->type == TOK_OP &&
        (t->value[0] == '*' || t->value[0] == '/')) {
        char op[2]; op[0] = t->value[0]; op[1] = '\0';
        char act[64]; snprintf(act, sizeof(act), "term' -> %s factor term'", op);
        ps_push("term'");
        trace(act);
        ps_pop(); ps_push("term'"); ps_push("factor"); ps_push(op);
        consume();
        trace("match op");
        ps_pop();
        factor();
        emit(TOK_OP, op, 0);
        term_prime();
        ps_pop();
    } else {
        ps_push("term'");
        trace("term' -> epsilon");
        ps_pop();
    }
}

void factor(void) {
    Token *t = peek();
    if (t->type == TOK_NUM) {
        int v = t->num_val;
        char s[64]; strcpy(s, t->value);
        char act[64]; snprintf(act, sizeof(act), "factor -> NUM(%s)", s);
        ps_push("factor");
        trace(act);
        ps_pop(); ps_push(s);
        consume();
        trace("match NUM");
        ps_pop();
        emit(TOK_NUM, s, v);
    } else if (t->type == TOK_ID) {
        char s[64]; strcpy(s, t->value);
        char act[64]; snprintf(act, sizeof(act), "factor -> ID(%s)", s);
        ps_push("factor");
        trace(act);
        ps_pop(); ps_push(s);
        consume();
        trace("match ID");
        ps_pop();
        emit(TOK_ID, s, 0);
    } else if (t->type == TOK_LPAREN) {
        ps_push("factor");
        trace("factor -> ( expr )");
        ps_pop(); ps_push(")"); ps_push("expr"); ps_push("(");
        consume();
        trace("match (");
        ps_pop();
        expr();
        expect(TOK_RPAREN);
        trace("match )");
        ps_pop();
        consume();
    } else {
        fprintf(stderr,
            "Parse error: unexpected token %s('%s') in factor\n",
            token_type_name(t->type), t->value);
        exit(1);
    }
}

//SECTION 5 – SYMBOL TABLE 
#define MAX_VARS 64

typedef struct { char name[64]; int value; } Variable;
Variable sym_table[MAX_VARS];
int      sym_count = 0;

int  var_get(const char *name) {
    for (int i = 0; i < sym_count; i++)
        if (strcmp(sym_table[i].name, name) == 0)
            return sym_table[i].value;
    fprintf(stderr, "Runtime error: undefined variable '%s'\n", name);
    exit(1);
}

void var_set(const char *name, int val) {
    for (int i = 0; i < sym_count; i++)
        if (strcmp(sym_table[i].name, name) == 0) {
            sym_table[i].value = val; return;
        }
    if (sym_count >= MAX_VARS) { fprintf(stderr,"Symbol table full\n"); exit(1); }
    strcpy(sym_table[sym_count].name, name);
    sym_table[sym_count].value = val;
    sym_count++;
}


//SECTION 6 – POSTFIX EVALUATOR  (stack machine)
#define STACK_SIZE 256

int eval_stack[STACK_SIZE];
int stack_top = -1;

void push(int v) {
    if (stack_top >= STACK_SIZE - 1) { fprintf(stderr,"Stack overflow\n"); exit(1); }
    eval_stack[++stack_top] = v;
}
int pop(void) {
    if (stack_top < 0) { fprintf(stderr,"Stack underflow\n"); exit(1); }
    return eval_stack[stack_top--];
}

int evaluate_postfix(void) {
    stack_top = -1;
    char last_assign_var[64] = "";

    for (int i = 0; i < postfix_len; i++) {
        PostfixToken *pt = &postfix[i];

        if (pt->type == TOK_NUM) {
            push(pt->num_val);

        } else if (pt->type == TOK_ID) {
            /* peek ahead: if next is ASSIGN this ID is the lvalue */
            if (i + 1 < postfix_len && postfix[i+1].type == TOK_ASSIGN) {
                strcpy(last_assign_var, pt->value);
            } else {
                push(var_get(pt->value));
            }

        } else if (pt->type == TOK_ASSIGN) {
            int rval = pop();
            var_set(last_assign_var, rval);
            push(rval);           /* assignment expression has a value */

        } else if (pt->type == TOK_OP) {
            int b = pop(), a = pop();
            int result = 0;
            switch (pt->value[0]) {
                case '+': result = a + b; break;
                case '-': result = a - b; break;
                case '*': result = a * b; break;
                case '/':
                    if (b == 0) { fprintf(stderr,"Runtime error: division by zero\n"); exit(1); }
                    result = a / b;
                    break;
            }
            push(result);
        }
    }
    return (stack_top >= 0) ? pop() : 0;
}

//SECTION 7 – DISPLAY HELPERS

void print_separator(char c, int n) {
    for (int i = 0; i < n; i++) putchar(c);
    putchar('\n');
}

void print_token_stream(void) {
    printf("\n--- TOKEN STREAM ---\n");
    print_separator('-', 45);
    printf("| %-10s | %-20s | %s |\n", "Type", "Value", "NumVal");
    printf("-----------------------------------------------\n");
    for (int i = 0; i < token_count - 1; i++) {   /* skip EOF */
        Token *t = &token_stream[i];
        if (t->type == TOK_NUM)
            printf("  %-10s  %-20s  %d\n", token_type_name(t->type), t->value, t->num_val);
        else
            printf("  %-10s  %-20s  N/A\n", token_type_name(t->type), t->value);
    }
    printf("\n");
    print_separator('-', 60);
}

void print_postfix(void) {
    print_separator('-', 60);
    printf("POSTFIX (RPN) CODE: ");
    for (int i = 0; i < postfix_len; i++) {
        printf("%s ", postfix[i].value);
    }
    printf("\n");
    print_separator('-', 60);
}

void print_result(const char *input, int result) {
    print_separator('-', 60);
    printf("RESULT \n");
    printf(" Input  : %s\n", input);
    printf(" Result : %d\n", result);
    printf("");
    print_separator('-', 60);
}

//SECTION 8 – COMPILE & RUN ONE STATEMENT

void compile_and_run(const char *input) {
    printf("\n");
    print_separator('=', 62);
    printf("  INPUT: %s\n", input);
    print_separator('=', 62);

    /* reset per-statement state */
    token_count      = 0;
    postfix_len      = 0;
    cur              = 0;
    trace_len        = 0;
    parse_stack_top  = -1;

    /* Phase 1 – Lex */
    tokenize(input);
    print_token_stream();

    /* Phase 2 & 3 – Parse + emit postfix */
    stmt();
    if (peek()->type != TOK_EOF) {
        fprintf(stderr, "Parse error: unexpected tokens after expression\n");
        exit(1);
    }
    print_parse_trace();

    /* Print postfix */
    print_postfix();

    /* Phase 4 – Evaluate */
    int result = evaluate_postfix();
    print_result(input, result);
}

//SECTION 9 – MAIN AND LL(1) TABLE

void print_ll1_table(void) {
    printf("\n");
    print_separator('=', 62);
    printf("  LL(1) PARSING TABLE\n");
    print_separator('=', 62);
    printf("%-10s %-12s %-30s\n", "Non-Term", "Lookahead", "Production");
    print_separator('-', 62);
    printf("%-10s %-12s %-30s\n", "stmt",   "ID (=next)", "stmt -> ID = expr");
    printf("%-10s %-12s %-30s\n", "stmt",   "NUM/ID/(  ", "stmt -> expr");
    printf("%-10s %-12s %-30s\n", "expr",   "NUM/ID/(  ", "expr -> term expr'");
    printf("%-10s %-12s %-30s\n", "expr'",  "+         ", "expr' -> + term expr'");
    printf("%-10s %-12s %-30s\n", "expr'",  "-         ", "expr' -> - term expr'");
    printf("%-10s %-12s %-30s\n", "expr'",  ")/EOF     ", "expr' -> ε");
    printf("%-10s %-12s %-30s\n", "term",   "NUM/ID/(  ", "term -> factor term'");
    printf("%-10s %-12s %-30s\n", "term'",  "*         ", "term' -> * factor term'");
    printf("%-10s %-12s %-30s\n", "term'",  "/         ", "term' -> / factor term'");
    printf("%-10s %-12s %-30s\n", "term'",  "+/-)/EOF  ", "term' -> ε");
    printf("%-10s %-12s %-30s\n", "factor", "NUM       ", "factor -> NUM");
    printf("%-10s %-12s %-30s\n", "factor", "ID        ", "factor -> ID");
    printf("%-10s %-12s %-30s\n", "factor", "(         ", "factor -> ( expr )");
    print_separator('=', 62);
}

int main(void) {
    char input[256];

    printf("\n");
    print_separator('*', 62);
    printf("  SIMPLE CALCULATOR LANGUAGE COMPILER\n");
    print_separator('*', 62);

  printf("\nContext-Free Grammar (left-recursion removed):\n");
    printf("  stmt   -> ID = expr | expr\n");
    printf("  expr   -> term expr'\n");
    printf("  expr'  -> + term expr' | - term expr' | epsilon\n");
    printf("  term   -> factor term'\n");
    printf("  term'  -> * factor term' | / factor term' | epsilon\n");
    printf("  factor -> NUM | ID | ( expr )\n");

    print_ll1_table();

    printf("\nEnter expressions to evaluate.\n");
    printf("Type 'exit' to quit.\n");

    while (1) {
        printf("\n>> ");

        if (!fgets(input, sizeof(input), stdin))
            break;

        input[strcspn(input, "\n")] = 0;

        if (strcmp(input, "exit") == 0)
            break;

        if (strlen(input) == 0)
            continue;

        compile_and_run(input);
    }

    printf("\n");
    print_separator('*', 62);
    printf("  SYMBOL TABLE (final state)\n");
    print_separator('*', 62);

    for (int i = 0; i < sym_count; i++)
        printf("  %-12s = %d\n", sym_table[i].name, sym_table[i].value);

    print_separator('*', 62);
    printf("\n");

    return 0;
}