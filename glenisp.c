#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <editline/readline.h>
#ifndef __APPLE__
    #include <editline/history.h>
#endif

#include "mpc.h"

enum lval_type { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_SEXP };

struct lval {
    enum lval_type type;
    union {
        char* err;
        long num;
        char* sym;
        struct lval_sexp {
            int count;
            struct lval** cell;
        };
    };
};

struct lval lval_num(long x) {
    struct lval v;
    v.type = LVAL_NUM;
    v.num = x;
    return v;
}

struct lval lval_err(char* x) {
    struct lval v;
    v.type = LVAL_ERR;
    v.err = x;
    return v;
}

char* lval_format(struct lval v) {
    switch (v.type) {
        case LVAL_NUM: {
            char* out;
            asprintf(&out, "%li", v.num);
            return out;
        }
        case LVAL_SYM:
        case LVAL_SEXP:
            return "!! todo";
        case LVAL_ERR:
            return v.err;
    }
}

struct lval eval_operator(
    char* op, struct lval x, struct lval y
) {

    if (x.type == LVAL_ERR) return x;
    if (y.type == LVAL_ERR) return y;

    long a = x.num;
    long b = y.num;

    if (strcmp(op, "+") == 0) {
        return lval_num(a + b);
    }
    if (strcmp(op, "-") == 0) {
        return lval_num(a - b);
    }
    if (strcmp(op, "*") == 0) {
        return lval_num(a * b);
    }
    if (strcmp(op, "/") == 0) {
        if (b == 0) return lval_err("Division by 0");
        return lval_num(a / b);
    }
    if (strcmp(op, "%") == 0) {
        if (b == 0) return lval_err("Division by 0");
        return lval_num(a % b);
    }
    if (strcmp(op, "^") == 0) {
        return lval_num(pow(a, b));
    }
    if (strcmp(op, "min") == 0) {
        return lval_num(a < b ? a : b);
    }
    if (strcmp(op, "maa") == 0) {
        return lval_num(a > b ? a : b);
    }

    return lval_err("Unknown symbol");
}

struct lval eval_unary(char* op, struct lval x) {
    if (x.type == LVAL_ERR) return x;

    if (strcmp(op, "-") == 0) return lval_num(-1 * x.num);
    return x;
}

struct lval eval(mpc_ast_t* node) {

    if (strstr(node->tag, "number")) {
        errno = 0;
        long x = strtol(node->contents, NULL, 10);
        if (errno == 0) {
            return lval_num(x);
        } else {
            lval_err("What number is this?");
        }
    }

    char* op = node->children[1]->contents;

    struct lval x = eval(node->children[2]);

    if (node->children_num == 4) {
        x = eval_unary(op, x);
    } else {
        for (int i = 3; i < node->children_num - 1; i++) {
            x = eval_operator(op, x, eval(node->children[i]));
        }
    }

    return x;
}

int main(int argc, char** argv)
{
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexp = mpc_new("sexp");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Program = mpc_new("program");

    mpca_lang(MPCA_LANG_DEFAULT,
    "                                                       \
        number   : /-?[0-9]+/ ;                             \
        symbol   : '+' | '-' | '*' | '/' | '%' | '^' |      \
                   \"min\" | \"max\";                       \
        sexp     : '(' <expr>* ')' ;                        \
        expr     : <number> | <symbol> | <sexp> ;           \
        program  : /^/ <expr>* /$/ ;                        \
    ",
        Number, Symbol, Sexp, Expr, Program);

    puts("Welcome to gLenISP Version 0.0.0.1");
    puts("You have 1000 parentheses remaining");
    puts("Press Ctrl+c to Exit\n");

    while (1) {

        char* input = readline("glenisp> ");

        add_history(input);

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Program, &r)) {
            mpc_ast_t* a = r.output;

            mpc_ast_print(r.output);

            char* result = lval_format(eval(a));
            printf("%s\n", result);
            free(result);

            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);

    }

    mpc_cleanup(5, Number, Symbol, Sexp, Expr, Program);

    return 0;
}
