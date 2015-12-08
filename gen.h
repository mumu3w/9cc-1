#ifndef _GEN_H
#define _GEN_H

#define NUM_IARG_REGS  6
#define NUM_FARG_REGS  8

struct reg {
    const char *r64;
    const char *r32;
    const char *r16;
    const char *r8;
    unsigned uses;
};

enum {
    OPERAND_REGISTER,
    OPERAND_MEMORY,
    OPERAND_LITERAL
};

struct operand {
    int kind;
    union {
        const char *name;
        struct reg *reg;
    }u;
};

// sym
#define SYM_X_LABEL(NODE)     ((NODE)->symbol.x.sym.label)
#define SYM_X_LOFF(NODE)      ((NODE)->symbol.x.sym.loff)
// decl
#define DECL_X_SVARS(NODE)             ((NODE)->decl.x.decl.svars)
#define DECL_X_LVARS(NODE)             ((NODE)->decl.x.decl.lvars)
#define DECL_X_EXTRA_STACK_SIZE(NODE)  ((NODE)->decl.x.decl.extra_stack_size)
// expr
#define EXPR_X_ADDR(NODE)       ((NODE)->expr.x.expr.addr)
#define EXPR_X_ARG(NODE)        ((NODE)->expr.x.expr.arg)
// stmt
#define STMT_X_LABEL(NODE)    ((NODE)->stmt.x.stmt.label)

union code {
    struct {
        const char *label;
        long loff;             // stack offset
    }sym;
    
    struct {
        node_t **lvars;        // function local vars
        node_t **svars;        // function static vars
        size_t extra_stack_size;
    }decl;
    
    struct {
        struct operand *addr;
        struct operand *arg;
    }expr;

    struct {
        const char *label;
    }stmt;
};

// register.c
extern void init_regs(void);
extern void print_register_state(void);

// gen.c
extern void emit(const char *fmt, ...);
extern void gen(node_t * tree, FILE * fp);

#endif
