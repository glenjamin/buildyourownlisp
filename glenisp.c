#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <editline/readline.h>
#ifndef __APPLE__
    #include <editline/history.h>
#endif

#include "mpc.h"

enum lval_type { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_SEXP, LVAL_QEXP };

struct lval {
    enum lval_type type;
    union {
        char* err;
        long num;
        char* sym;
        struct {
            int count;
            struct lval** cell;
        };
    };
};

struct lval* lval_num(long x) {
    struct lval* v = malloc(sizeof(struct lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

struct lval* lval_err(char* msg) {
    // TODO: avsprintf
    struct lval* v = malloc(sizeof(struct lval));
    v->type = LVAL_ERR;
    v->err = malloc(strlen(msg) + 1);
    strcpy(v->err, msg);
    return v;
}

struct lval* lval_sym(char* s) {
    struct lval* v = malloc(sizeof(struct lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
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

#define LASSERT(v, cond, err)   \
    if (!(cond)) {              \
        lval_del(v);            \
        return lval_err(err);   \
    }                           \


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

void lval_print(struct lval* v) {
    switch (v->type) {
        case LVAL_ERR: printf("Error: %s", v->err); break;
        case LVAL_NUM: printf("%li", v->num); break;
        case LVAL_SYM: printf("%s", v->sym); break;

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

struct lval* lval_read_num(mpc_ast_t* node) {
    errno = 0;
    long x = strtol(node->contents, NULL, 10);
    if (errno == 0) {
        return lval_num(x);
    } else {
        return lval_err("What number is this?");
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
    if (strstr(node->tag, "number")) {
        return lval_read_num(node);
    }
    if (strstr(node->tag, "symbol")) {
        return lval_sym(node->contents);
    }

    struct lval* x;
    if (strcmp(node->tag, ">") == 0 || strstr(node->tag, "sexp")) {
        x = lval_sexp();
    }
    else if (strstr(node->tag, "qexp")) {
        x = lval_qexp();
    }
    else {
        return lval_err("No idea what this is");
    }

    for (int i = 0; i < node->children_num; i++) {
        if (read_ignore(node->children[i])) continue;
        x = lval_add(x, lval_read(node->children[i]));
    }
    return x;
}

struct lval* lval_eval(struct lval* v);

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
        LASSERT(x, 0, "Unknown operator");
    }

    return x;
}

struct lval* lval_eval_head(struct lval* v) {
    LASSERT(v, v->count == 1, "'head' expects only 1 argument");
    LASSERT(v, v->cell[0]->type == LVAL_QEXP, "'head' expects Q-exp");
    LASSERT(v, v->cell[0]->count > 0, "'head' non-empty Q-exp");

    struct lval* x = lval_qexp();
    lval_add(x, lval_take(lval_take(v, 0), 0));
    return x;
}

struct lval* lval_eval_tail(struct lval* v) {
    LASSERT(v, v->count == 1, "'tail' expects only 1 argument");
    LASSERT(v, v->cell[0]->type == LVAL_QEXP, "'tail' expects Q-exp");
    LASSERT(v, v->cell[0]->count > 0, "'tail' non-empty Q-exp");

    struct lval* x = lval_take(v, 0);
    lval_del(lval_pop(x, 0));
    return x;
}

struct lval* lval_eval_list(struct lval* v) {
    v->type = LVAL_QEXP;
    return v;
}

struct lval* lval_eval_eval(struct lval* v) {
    LASSERT(v, v->count == 1, "'eval' expects only 1 argument");
    LASSERT(v, v->cell[0]->type == LVAL_QEXP, "'eval' expects Q-exp");

    struct lval* x = lval_take(v, 0);
    x->type = LVAL_SEXP;
    return lval_eval(x);
}

struct lval* lval_eval_join(struct lval* v) {
    for (int i = 0; i < v->count; i++) {
        LASSERT(
            v, v->cell[i]->type == LVAL_QEXP,
            "'join' expects 1 or more Q-exps");
    }

    struct lval* x = lval_pop(v, 0);

    while (v->count) {
        x = lval_join(x, lval_pop(v, 0));
    }

    lval_del(v);
    return x;
}

struct lval* lval_eval_op(char* sym, struct lval* v) {
    for (int i = 0; i < v->count; i++) {
        LASSERT(
            v, v->cell[i]->type == LVAL_NUM,
            "I'll only work with numbers");
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

struct lval* lval_eval_special(char* sym, struct lval* v) {
    if (strcmp(sym, "list") == 0) return lval_eval_list(v);
    if (strcmp(sym, "head") == 0) return lval_eval_head(v);
    if (strcmp(sym, "tail") == 0) return lval_eval_tail(v);
    if (strcmp(sym, "join") == 0) return lval_eval_join(v);
    if (strcmp(sym, "eval") == 0) return lval_eval_eval(v);
    return lval_eval_op(sym, v);
}

struct lval* lval_eval(struct lval* v) {
    if (v->type != LVAL_SEXP) {
        return v;
    }

    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(v->cell[i]);
    }

    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) {
            return lval_take(v, i);
        }
    }

    if (v->count == 0) return v;

    if (v->count == 1) return lval_take(v, 0);

    struct lval* f = lval_pop(v, 0);
    if (f->type != LVAL_SYM) {
        lval_del(f); lval_del(v);
        return lval_err("sexp does not start with a symbol");
    }
    struct lval* result = lval_eval_special(f->sym, v);
    lval_del(f);
    return result;
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
        symbol   : '+' | '-' | '*' | '/' | '%' | '^' |      \
                   \"min\" | \"max\" | \"list\" | \"head\" |\
                   \"tail\" | \"join\" | \"eval\" ;         \
        sexp     : '(' <expr>* ')' ;                        \
        qexp     : '{' <expr>* '}' ;                        \
        expr     : <number> | <symbol> | <sexp> | <qexp> ;  \
        program  : /^/ <expr>* /$/ ;                        \
    ",
        Number, Symbol, Sexp, Qexp, Expr, Program);

    puts("Welcome to gLenISP Version 0.0.0.1");
    puts("You have 1000 parentheses remaining");
    puts("Press Ctrl+c to Exit\n");

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

            struct lval* r = lval_eval(x);

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
