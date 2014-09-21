#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <editline/readline.h>
#ifndef __APPLE__
    #include <editline/history.h>
#endif

#include "mpc.h"

#define STR_COPY(a, b)          \
    a = malloc(strlen(b) + 1);  \
    strcpy(a, b);               \

enum lval_type {
    LVAL_ERR, LVAL_NUM, LVAL_SYM,
    LVAL_FN, LVAL_SEXP, LVAL_QEXP
};

char* lval_type_name(enum lval_type t) {
    switch (t) {
        case LVAL_ERR:  return "Error";
        case LVAL_NUM:  return "Number";
        case LVAL_SYM:  return "Symbol";
        case LVAL_FN:   return "Function";
        case LVAL_SEXP: return "Sexp";
        case LVAL_QEXP: return "Qexp";
        default: return "Unknown";
    }
}

struct lval;
struct lenv;

typedef struct lval* (*lfunc)(struct lenv*, struct lval*);

struct lval {
    enum lval_type type;
    union {
        char* err;
        long num;
        char* sym;
        struct {
            char* name;
            lfunc fn;
        };
        struct {
            int count;
            struct lval** cell;
        };
    };
};
struct lval* lval_err(char* msg, ...);

void lval_del(struct lval*);
struct lval* lval_copy(struct lval*);

struct lenv {
    int count;
    char** syms;
    struct lval** vals;
};

struct lenv* lenv_new(void) {
    struct lenv* e = malloc(sizeof(struct lenv));
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

void lenv_del(struct lenv* e) {
    for (int i = 0; i< e->count; i++) {
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }
    free(e->syms);
    free(e->vals);
    free(e);
}

struct lval* lenv_get(struct lenv* e, char* sym) {
    for (int i = 0; i < e->count; i++) {
        if (strcmp(e->syms[i], sym) == 0) {
            return lval_copy(e->vals[i]);
        }
    }
    return lval_err("Unbound symbol '%s'", sym);
}

void lenv_put(struct lenv* e, char* sym, struct lval* v) {
    for (int i = 0; i < e->count; i++) {
        if (strcmp(e->syms[i], sym) == 0) {
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            return;
        }
    }
    e->count += 1;
    e->syms = realloc(e->syms, sizeof(char*) * e->count);
    e->vals = realloc(e->vals, sizeof(struct lval*) * e->count);

    STR_COPY(e->syms[e->count - 1], sym);
    e->vals[e->count - 1] = lval_copy(v);
}

struct lval* lval_num(long x) {
    struct lval* v = malloc(sizeof(struct lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

struct lval* lval_err(char* msg, ...) {
    struct lval* v = malloc(sizeof(struct lval));
    v->type = LVAL_ERR;

    va_list va;
    va_start(va, msg);
    vasprintf(&v->err, msg, va);
    va_end(va);

    return v;
}

struct lval* lval_sym(char* s) {
    struct lval* v = malloc(sizeof(struct lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}

struct lval* lval_fn(char* name, lfunc fn) {
    struct lval* v = malloc(sizeof(struct lval));
    v->type = LVAL_FN;
    STR_COPY(v->name, name);
    v->fn = fn;
    return v;
}

struct lval* lval_sexp(void) {
    struct lval* v = malloc(sizeof(struct lval));
    v->type = LVAL_SEXP;
    v->count = 0;
    v->cell = NULL;
    return v;
}
struct lval* lval_qexp(void) {
    struct lval* v = malloc(sizeof(struct lval));
    v->type = LVAL_QEXP;
    v->count = 0;
    v->cell = NULL;
    return v;
}

void lval_del(struct lval* v) {
    switch(v->type) {
        case LVAL_NUM: break;
        case LVAL_FN: free(v->name); break;

        case LVAL_ERR: free(v->err); break;
        case LVAL_SYM: free(v->sym); break;

        case LVAL_SEXP:
        case LVAL_QEXP:
            for (int i = 0; i < v->count; i++) {
                lval_del(v->cell[i]);
            }
            free(v->cell);
            break;
    }
    free(v);
}

#define LASSERT(v, cond, msg, ...)                          \
    if (!(cond)) {                                          \
        struct lval* err = lval_err(msg, ##__VA_ARGS__);    \
        lval_del(v);                                        \
        return err;                                         \
    }                                                       \

#define LNUMARGS(v, n, source)                      \
    LASSERT(                                        \
        v,                                          \
        v->type == LVAL_SEXP && v->count == n,      \
        "Wrong arg count for '%s'. "                \
        "Got %i, expected %i",                      \
        source, v->count, n);                       \

#define LTYPE(v, t, i, source)              \
    LASSERT(                                \
        v,                                  \
        v->cell[i]->type == t,              \
        "Wrong type for arg %i in '%s'. "   \
        "Got %s, expected %s",              \
        i, source,                          \
        lval_type_name(v->cell[i]->type),   \
        lval_type_name(t));                 \

#define LNONEMPTY(v, i, source)                         \
    LASSERT(                                            \
        v,                                              \
        v->cell[i]->type == LVAL_QEXP &&                \
        v->cell[i]->count > 0,                          \
        "'%s' expects arg %i to be a non-empty qexp",   \
        source, i);                                     \

struct lval* lval_add(struct lval* v, struct lval* x) {
    v->count += 1;
    v->cell = realloc(v->cell, v->count * sizeof(struct lval*));
    v->cell[v->count - 1] = x;
    return v;
}

struct lval* lval_pop(struct lval* v, int i) {

    struct lval* x = v->cell[i];

    int width = sizeof(struct lval*);

    memmove(&v->cell[i], &v->cell[i + 1], width * (v->count - i - 1));

    v->count -= 1;

    v->cell = realloc(v->cell, width * v->count);

    return x;
}

struct lval* lval_take(struct lval* v, int i) {
    struct lval* x = lval_pop(v, i);
    lval_del(v);
    return x;
}

struct lval* lval_join(struct lval* v, struct lval* x) {
    while (x->count) {
        v = lval_add(v, lval_pop(x, 0));
    }
    lval_del(x);
    return v;
}

struct lval* lval_copy(struct lval* v) {
    struct lval* x = malloc(sizeof(struct lval));
    x->type = v->type;
    switch(v->type) {
        case LVAL_NUM: x->num = v->num; break;

        case LVAL_ERR: STR_COPY(x->err, v->err); break;
        case LVAL_SYM: STR_COPY(x->sym, v->sym); break;

        case LVAL_FN:
            STR_COPY(x->name, v->name);
            x->fn = v->fn;
            break;

        case LVAL_SEXP:
        case LVAL_QEXP:
            x->count = v->count;
            x->cell = malloc(sizeof(struct lval*) * x->count);
            for (int i = 0; i < x->count; i++) {
                x->cell[i] = lval_copy(v->cell[i]);
            }
            break;
    }

    return x;
}

void lval_print(struct lval* v) {
    switch (v->type) {
        case LVAL_ERR: printf("Error: %s", v->err); break;
        case LVAL_NUM: printf("%li", v->num); break;
        case LVAL_SYM: printf("%s", v->sym); break;

        case LVAL_FN: printf("<fn %s>", v->name); break;

        case LVAL_SEXP:
        case LVAL_QEXP:
            putchar(v->type == LVAL_SEXP ? '(' : '{');
            for (int i = 0; i < v->count; i++) {
                lval_print(v->cell[i]);
                if (i != (v->count - 1)) putchar(' ');
            }
            putchar(v->type == LVAL_SEXP ? ')' : '}');
            break;
    }
}

void lenv_add_builtin(struct lenv* e, char* name, lfunc fn) {
    struct lval* v = lval_fn(name, fn);
    lenv_put(e, name, v);
    lval_del(v);
}

struct lval* lval_read_num(mpc_ast_t* node) {
    errno = 0;
    long x = strtol(node->contents, NULL, 10);
    if (errno == 0) {
        return lval_num(x);
    } else {
        return lval_err("Unknown number %s", node->contents);
    }
}

int read_ignore(mpc_ast_t* node) {
    if (strcmp(node->contents, "(") == 0) return 1;
    if (strcmp(node->contents, ")") == 0) return 1;
    if (strcmp(node->contents, "{") == 0) return 1;
    if (strcmp(node->contents, "}") == 0) return 1;
    if (strcmp(node->tag, "regex") == 0) return 1;
    return 0;
}

struct lval* lval_read(mpc_ast_t* node) {

    // Upwrap top-level form as a single expression
    if (strcmp(node->tag, ">") == 0) {
        return lval_read(node->children[1]);
    }

    if (strstr(node->tag, "number")) {
        return lval_read_num(node);
    }
    if (strstr(node->tag, "symbol")) {
        return lval_sym(node->contents);
    }

    struct lval* x;
    if (strstr(node->tag, "sexp")) {
        x = lval_sexp();
    }
    else if (strstr(node->tag, "qexp")) {
        x = lval_qexp();
    }
    else {
        return lval_err("Unexpected node: %s", node->tag);
    }

    for (int i = 0; i < node->children_num; i++) {
        if (read_ignore(node->children[i])) continue;
        x = lval_add(x, lval_read(node->children[i]));
    }
    return x;
}

struct lval* lval_eval(struct lenv* e, struct lval* v);

struct lval* lval_eval_sexp(struct lenv* e, struct lval* v);

struct lval* lval_eval_unary(char* sym, struct lval* v) {
    if (v->type == LVAL_ERR) return v;

    if (strcmp(sym, "-") == 0) v->num = -v->num;

    return v;
}

struct lval* lval_eval_binary(
    char* sym, struct lval* x, struct lval* y
) {
    long a = x->num;
    long b = y->num;

    if (strcmp(sym, "+") == 0) x->num = a + b;
    else if (strcmp(sym, "-") == 0) x->num = a - b;
    else if (strcmp(sym, "*") == 0) x->num = a * b;
    else if (strcmp(sym, "^") == 0) x->num = pow(a, b);
    else if (strcmp(sym, "min") == 0) x->num = a < b ? a : b;
    else if (strcmp(sym, "max") == 0) x->num = a > b ? a : b;
    else if (strcmp(sym, "/") == 0) {
        LASSERT(x, b != 0, "Division by 0");
        x->num = a / b;
    }
    else if (strcmp(sym, "%") == 0) {
        LASSERT(x, b != 0, "Division by 0");
        x->num = a % b;
    }
    else {
        LASSERT(x, 0, "Unknown operator %s", sym);
    }

    return x;
}

struct lval* lval_eval_head(struct lenv* e, struct lval* v) {
    LNUMARGS(v, 1, "head");
    LNONEMPTY(v, 0, "head");

    struct lval* x = lval_qexp();
    lval_add(x, lval_take(lval_take(v, 0), 0));
    return x;
}

struct lval* lval_eval_last(struct lenv* e, struct lval* v) {
    LNUMARGS(v, 1, "last");
    LNONEMPTY(v, 0, "last");

    struct lval* x = lval_qexp();
    struct lval* l = lval_take(v, 0);
    lval_add(x, lval_take(l, l->count - 1));
    return x;
}

struct lval* lval_eval_tail(struct lenv* e, struct lval* v) {
    LNUMARGS(v, 1, "tail");
    LNONEMPTY(v, 0, "tail");

    struct lval* x = lval_take(v, 0);
    lval_del(lval_pop(x, 0));
    return x;
}

struct lval* lval_eval_init(struct lenv* e, struct lval* v) {
    LNUMARGS(v, 1, "init");
    LNONEMPTY(v, 0, "init");

    struct lval* x = lval_take(v, 0);
    lval_del(lval_pop(x, x->count - 1));
    return x;
}

struct lval* lval_eval_list(struct lenv* e, struct lval* v) {
    v->type = LVAL_QEXP;
    return v;
}

struct lval* lval_eval_len(struct lenv* e, struct lval* v) {
    LNUMARGS(v, 1, "len");
    LTYPE(v, LVAL_QEXP, 0, "len");

    struct lval* x = lval_num(v->cell[0]->count);
    lval_del(v);

    return x;
}

struct lval* lval_eval_eval(struct lenv* e, struct lval* v) {
    LNUMARGS(v, 1, "eval");
    LTYPE(v, LVAL_QEXP, 0, "eval");

    struct lval* x = lval_take(v, 0);
    x->type = LVAL_SEXP;
    return lval_eval(e, x);
}

struct lval* lval_eval_cons(struct lenv* e, struct lval* v) {
    LNUMARGS(v, 2, "cons");
    LTYPE(v, LVAL_QEXP, 1, "cons");

    // New q-exp with first arg
    struct lval* x = lval_qexp();
    lval_add(x, lval_pop(v, 0));

    // Old q-exp from second arg
    struct lval* q = lval_take(v, 0);

    while (q->count) {
        lval_add(x, lval_pop(q, 0));
    }
    lval_del(q);

    return lval_eval(e, x);
}

struct lval* lval_eval_join(struct lenv* e, struct lval* v) {
    for (int i = 0; i < v->count; i++) {
        LTYPE(v, LVAL_QEXP, i, "join");
    }

    struct lval* x = lval_pop(v, 0);

    while (v->count) {
        x = lval_join(x, lval_pop(v, 0));
    }

    lval_del(v);
    return x;
}

struct lval* lval_eval_op(struct lenv* e, char* sym, struct lval* v) {
    for (int i = 0; i < v->count; i++) {
        LTYPE(v, LVAL_NUM, i, sym);
    }

    struct lval* x = lval_pop(v, 0);

    if (v->count == 0) {
        return lval_eval_unary(sym, x);
    }

    while (v->count > 0) {
        struct lval* y = lval_pop(v, 0);
        x = lval_eval_binary(sym, x, y);
        lval_del(y);
    }

    lval_del(v);

    return x;
}

struct lval* lval_builtin_def(struct lenv* e, struct lval* v) {
    LTYPE(v, LVAL_QEXP, 0, "def");

    struct lval* syms = v->cell[0];

    for (int i = 0; i < syms->count; i++) {
        LASSERT(v, syms->cell[i]->type == LVAL_SYM,
            "'def' expects variable %i to be symbol", i);
    }

    LASSERT(v, syms->count == (v->count - 1),
        "'def' expects same variable & value count. "
        "Got %i variables and %i values.",
        syms->count, v->count -1);

    for (int i = 0; i < syms->count; i++) {
        char* sym = syms->cell[i]->sym;
        struct lval* x = lenv_get(e, sym);
        if (x->type == LVAL_FN) {
            struct lval* err = lval_err(
                "Cannot redefine builtin function '%s'", sym);
            lval_del(v);
            return err;
        }

        lenv_put(e, sym, v->cell[i + 1]);
    }

    return lval_take(v, 0);
}

struct lval* lval_builtin_env(struct lenv* e, struct lval* v) {
    LNUMARGS(v, 0, "env");

    for (int i = 0; i < e->count; i++) {
        printf("%s - ", e->syms[i]);
        lval_print(e->vals[i]);
        printf("\n");
    }

    return lval_sexp();
}

struct lval* lval_builtin_exit(struct lenv* e, struct lval* v) {
    LNUMARGS(v, 0, "exit");

    exit(EXIT_SUCCESS);
}

struct lval* lval_builtin_add(struct lenv* e, struct lval* v) {
    return lval_eval_op(e, "+", v);
}
struct lval* lval_builtin_sub(struct lenv* e, struct lval* v) {
    return lval_eval_op(e, "-", v);
}
struct lval* lval_builtin_mul(struct lenv* e, struct lval* v) {
    return lval_eval_op(e, "*", v);
}
struct lval* lval_builtin_div(struct lenv* e, struct lval* v) {
    return lval_eval_op(e, "/", v);
}
struct lval* lval_builtin_mod(struct lenv* e, struct lval* v) {
    return lval_eval_op(e, "%", v);
}
struct lval* lval_builtin_pow(struct lenv* e, struct lval* v) {
    return lval_eval_op(e, "^", v);
}
struct lval* lval_builtin_min(struct lenv* e, struct lval* v) {
    return lval_eval_op(e, "min", v);
}
struct lval* lval_builtin_max(struct lenv* e, struct lval* v) {
    return lval_eval_op(e, "max", v);
}

void lenv_add_builtins(struct lenv* e) {
    lenv_add_builtin(e, "+", lval_builtin_add);
    lenv_add_builtin(e, "-", lval_builtin_sub);
    lenv_add_builtin(e, "*", lval_builtin_mul);
    lenv_add_builtin(e, "/", lval_builtin_div);
    lenv_add_builtin(e, "%", lval_builtin_mod);
    lenv_add_builtin(e, "^", lval_builtin_pow);
    lenv_add_builtin(e, "min", lval_builtin_min);
    lenv_add_builtin(e, "max", lval_builtin_max);

    lenv_add_builtin(e, "list", lval_eval_list);
    lenv_add_builtin(e, "head", lval_eval_head);
    lenv_add_builtin(e, "tail", lval_eval_tail);
    lenv_add_builtin(e, "last", lval_eval_last);
    lenv_add_builtin(e, "init", lval_eval_init);
    lenv_add_builtin(e, "join", lval_eval_join);
    lenv_add_builtin(e, "cons", lval_eval_cons);
    lenv_add_builtin(e, "len",  lval_eval_len);
    lenv_add_builtin(e, "eval", lval_eval_eval);

    lenv_add_builtin(e, "def", lval_builtin_def);
    lenv_add_builtin(e, "env", lval_builtin_env);

    lenv_add_builtin(e, "exit", lval_builtin_exit);
}

struct lval* lval_eval_sexp(struct lenv* e, struct lval* v) {

    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(e, v->cell[i]);
    }

    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) {
            return lval_take(v, i);
        }
    }

    if (v->count == 0) return v;

    struct lval* f = lval_pop(v, 0);
    if (f->type != LVAL_FN) {
        lval_del(f); lval_del(v);
        return lval_err("Expected sexp to begin with %s, got %s",
            lval_type_name(LVAL_FN), lval_type_name(f->type));
    }
    struct lval* result = f->fn(e, v);
    lval_del(f);
    return result;
}

struct lval* lval_eval(struct lenv* e, struct lval* v) {
    if (v->type == LVAL_SYM) {
        struct lval* x = lenv_get(e, v->sym);
        lval_del(v);
        return x;
    }
    if (v->type == LVAL_SEXP) {
        return lval_eval_sexp(e, v);
    }
    return v;
}

int main(int argc, char** argv)
{
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexp = mpc_new("sexp");
    mpc_parser_t* Qexp = mpc_new("qexp");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Program = mpc_new("program");

    mpca_lang(MPCA_LANG_DEFAULT,
    "                                                       \
        number   : /-?[0-9]+/ ;                             \
        symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&\\^]+/ ;    \
        sexp     : '(' <expr>* ')' ;                        \
        qexp     : '{' <expr>* '}' ;                        \
        expr     : <number> | <symbol> | <sexp> | <qexp> ;  \
        program  : /^/ <expr> /$/ ;                         \
    ",
        Number, Symbol, Sexp, Qexp, Expr, Program);

    puts("Welcome to gLenISP Version 0.0.0.1");
    puts("You have 1000 parentheses remaining");
    puts("Press Ctrl+c to Exit\n");

    struct lenv* e = lenv_new();
    lenv_add_builtins(e);

    while (1) {

        char* input = readline("glenisp> ");

        if (input == NULL) {
            break;
        }

        add_history(input);

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Program, &r)) {

            mpc_ast_print(r.output);

            struct lval* x = lval_read(r.output);
            mpc_ast_delete(r.output);

            puts("Input:");
            lval_print(x); putchar('\n');

            struct lval* r = lval_eval(e, x);

            puts("Output:");
            lval_print(r); putchar('\n');

            lval_del(r);

        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);

    }

    mpc_cleanup(5, Number, Symbol, Sexp, Qexp, Expr, Program);

    return 0;
}
