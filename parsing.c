#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>
#ifndef __APPLE__
    #include <editline/history.h>
#endif

#include "mpc.h"

int node_count(mpc_ast_t* node) {
    if (node->children_num == 0) {
        return 1;
    }
    int total = 1;
    for (int i = 0; i < node->children_num; i++) {
        total = total + node_count(node->children[i]);
    }
    return total;
}

void node_debug(mpc_ast_t* node) {
    printf("Tag: %s\n", node->tag);
    printf("Contents: %s\n", node->contents);
    printf("Total Children: %d\n", node_count(node));
}

long eval_operator(char* op, long x, long y) {
    if (strcmp(op, "+") == 0) return x + y;
    if (strcmp(op, "-") == 0) return x - y;
    if (strcmp(op, "*") == 0) return x * y;
    if (strcmp(op, "/") == 0) return x / y;
    if (strcmp(op, "%") == 0) return x % y;
    return 0;
}

long eval(mpc_ast_t* node) {

    if (strstr(node->tag, "number")) {
        return atoi(node->contents);
    }

    char* op = node->children[1]->contents;

    long x = eval(node->children[2]);

    for (int i = 3; i < node->children_num - 1; i++) {
        x = eval_operator(op, x, eval(node->children[i]));
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
        number   : /-?[0-9]+(\\.[0-9]+)?/ ;                 \
        operator : '+' | '-' | '*' | '/' | '%' ;            \
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

            printf("Result: %li\n", eval(a));

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
