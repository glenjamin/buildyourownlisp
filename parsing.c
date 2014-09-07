#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <editline/readline.h>
#ifndef __APPLE__
    #include <editline/history.h>
#endif

#include "mpc.h"

enum { LVAL_NUM, LVAL_ERR };
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

typedef struct {
    int type;
    long num;
    int err;
} lval;

lval lval_num(long x) {
    lval v;
    v.type = LVAL_NUM;
    v.num = x;
    return v;
}

lval lval_err(int x) {
    lval v;
    v.type = LVAL_ERR;
    v.err = x;
    return v;
}

char* lval_format(lval v) {
    char* out;
    switch (v.type) {
        case LVAL_NUM:
            asprintf(&out, "%li", v.num);
            break;
        case LVAL_ERR:
            switch (v.err) {
                case LERR_DIV_ZERO:
                    out = "Error: Division by zero";
                    break;
                case LERR_BAD_NUM:
                    out = "Error: Invalid number";
                    break;
                case LERR_BAD_OP:
                    out = "Error: Invaild operator";
                    break;
            }
            break;
    }
    return out;
}

lval eval_operator(char* op, lval x, lval y) {

    if (x.type == LVAL_ERR) return x;
    if (y.type == LVAL_ERR) return y;

    if (strcmp(op, "+") == 0) {
        return lval_num(x.num + y.num);
    }
    if (strcmp(op, "-") == 0) {
        return lval_num(x.num - y.num);
    }
    if (strcmp(op, "*") == 0) {
        return lval_num(x.num * y.num);
    }
    if (strcmp(op, "/") == 0) {
        if (y.num == 0) return lval_err(LERR_DIV_ZERO);
        return lval_num(x.num / y.num);
    }
    if (strcmp(op, "%") == 0) {
        if (y.num == 0) return lval_err(LERR_DIV_ZERO);
        return lval_num(x.num % y.num);
    }
    if (strcmp(op, "^") == 0) {
        return lval_num(pow(x.num, y.num));
    }
    if (strcmp(op, "min") == 0) {
        return lval_num(x.num < y.num ? x.num : y.num);
    }
    if (strcmp(op, "max.num") == 0) {
        return lval_num(x.num > y.num ? x.num : y.num );
    }

    return lval_err(LERR_BAD_OP);
}

lval eval_unary(char* op, lval x) {
    if (x.type)

    if (strcmp(op, "-") == 0) return lval_num(-1 * x.num);
    return x;
}

lval eval(mpc_ast_t* node) {

    if (strstr(node->tag, "number")) {
        errno = 0;
        long x = strtol(node->contents, NULL, 10);
        return errno == 0 ? lval_num(x) : lval_err(LERR_BAD_NUM);
    }

    char* op = node->children[1]->contents;

    lval x = eval(node->children[2]);

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
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Program = mpc_new("program");

    mpca_lang(MPCA_LANG_DEFAULT,
    "                                                       \
        number   : /-?[0-9]+/ ;                             \
        operator : '+' | '-' | '*' | '/' | '%' | '^' |      \
                   \"min\" | \"max\";                       \
        expr     : <number> | '(' <operator> <expr>+ ')' ;  \
        program  : /^/ <operator> <expr>+ /$/ ;             \
    ",
        Number, Operator, Expr, Program);

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

            printf("%s\n", lval_format(eval(a)));

            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);

    }

    mpc_cleanup(4, Number, Operator, Expr, Program);

    return 0;
}
