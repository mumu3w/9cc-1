#include "cc.h"

#define STR(str)  ((str) ? (str) : "<null>")

struct print_context {
    int level;
    struct node * node;
};

struct type_context {
    int level;
    struct type * type;
};

static void print_tree1(struct print_context context);
static void print_type1(struct type_context context);

static void print_qual(struct type *type)
{
    if (isconst(type)) {
        fprintf(stderr, "const ");
    }
    if (isvolatile(type)) {
        fprintf(stderr, "volatile ");
    }
    if (isrestrict(type)) {
        fprintf(stderr, "restrict ");
    }
    if (isinline(type)) {
        fprintf(stderr, "inline ");
    }
}

static void print_params(struct type_context context)
{
    struct symbol **params = context.type->u.f.params;
    if (params) {
        for (int i=0; params[i]; i++) {
            struct symbol *sym = params[i];
            struct type_context con = {context.level+1, sym->type};
            for (int i=0; i < context.level+1; i++)
                fprintf(stderr, "  ");
            fprintf(stderr, "'%s' %s ", STR(sym->name), sym->defined ? "<defined>" : "");
            print_type1(con);
        }
    }
}

static void print_return(struct type_context context)
{
    struct type_context rcontext = {context.level+1, context.type};
    for (int i=0; i < rcontext.level; i++)
        fprintf(stderr, "  ");
    fprintf(stderr, "return ");
    print_type1(rcontext);
}

static void print_short_type(struct type *type)
{
    struct type *rty = unqual(type);
    print_qual(type);
    if (isfunc(type)) {
        fprintf(stderr, "'%s'", rty->name);
    } else if (isptr(type)) {
        fprintf(stderr, "'%s to %s'", rty->name, unqual(rty->type)->name);
    } else if (isarray(type)) {
        fprintf(stderr, "'%s %lu of'", rty->name, rty->size);
    } else if (isenum(type) || isstruct(type) || isunion(type)) {
        fprintf(stderr, "'%s %s'", rty->name, rty->tag);
    } else {
        fprintf(stderr, "'%s'", rty->name);
    }
}

static void print_type1(struct type_context context)
{
    struct type *type = context.type;
    if (type) {
        struct type *rty = unqual(type);
        struct type_context tcontext = {context.level, rty->type};
        print_qual(type);
        if (isfunc(type)) {
            fprintf(stderr, "%s", rty->name);
            fprintf(stderr, "\n");
            print_return(tcontext);
            print_params(context);
        } else if (isptr(type)) {
            fprintf(stderr, "%s to ", rty->name);
            print_type1(tcontext);
        } else if (isarray(type)) {
            fprintf(stderr, "%s %lu of ", rty->name, rty->size);
            print_type1(tcontext);
        } else if (isenum(type) || isstruct(type) || isunion(type)) {
            fprintf(stderr, "%s %s ", rty->name, rty->tag);
            print_type1(tcontext);
        } else {
            fprintf(stderr, "%s ", rty->name);
            print_type1(tcontext);
        }
    } else {
        fprintf(stderr, "\n");
    }
}

void print_type(struct type *type)
{
    struct type_context context = {0, type};
    print_type1(context);
}

static void print_decl(struct node *node, struct print_context context)
{
    int level;
    
    if (node->sym) {
        fprintf(stderr, "%s '%s' %s ", nname(node), STR(node->sym->name), node->sym->defined ? "<defined>" : "");
        if (node->sym->type) {
            struct type_context tcontext = {context.level, node->sym->type};
            print_type1(tcontext);
        } else {
            fprintf(stderr, "\n");
        }
    } else {
        fprintf(stderr, "%s\n", nname(node));
    }
    
    level = context.level + 1;
    
    struct node **exts = node->u.d.exts;
    if (exts) {
        for (int i=0; exts[i]; i++) {
            struct print_context con = {level, exts[i]};
            print_tree1(con);
        }
    }
}

static void print_expr(struct node *node, struct print_context context)
{
    int level;
    int op = node->u.e.op;
    bool prefix = node->u.e.prefix;
    if (node->sym)
        fprintf(stderr, "%s '%s' %s %s ", nname(node), tname(op), STR(node->sym->name), (op == INCR || op == DECR) ? (prefix ? "prefix" : "postfix") : "");
    else
        fprintf(stderr, "%s '%s' %s ", nname(node), tname(op), (op == INCR || op == DECR) ? (prefix ? "prefix" : "postfix") : "");
    if (node->type)
        print_short_type(node->type);
    fprintf(stderr, "\n");
    
    level = context.level + 1;
    
    if (node->id == CALL_EXPR) {
        struct node **args = node->u.e.args;
        if (args) {
            for (int i=0; args[i]; i++) {
                struct print_context con = {level, args[i]};
                print_tree1(con);
            }
        }
    } else if (node->id == COND_EXPR) {
        struct node *cond = node->u.e.c.cond;
        struct node *then = node->u.e.c.then;
        struct node *els = node->u.e.c.els;
        if (cond) {
            struct print_context con = {level, cond};
            print_tree1(con);
        }
        if (then) {
            struct print_context con = {level, then};
            print_tree1(con);
        }
        if (els) {
            struct print_context con = {level, els};
            print_tree1(con);
        }
    }
}

static void print_stmt(struct node *node, struct print_context context)
{
    int level;
    struct node *up = node->u.s.up;
    if (up)
        fprintf(stderr, "%s %p -> %s %p\n",
                nname(node), node, nname(up), up);
    else
        fprintf(stderr, "%s %p\n", nname(node), node);
    
    level = context.level + 1;
    
    if (node->id == COMPOUND_STMT) {
        struct node **blks = node->u.s.compoundstmt.blks;
        if (blks) {
            for (int i=0; blks[i]; i++) {
                struct print_context con = {level, blks[i]};
                print_tree1(con);
            }
        }
    } else if (node->id == FOR_STMT) {
        struct node **decl = node->u.s.forstmt.decl;
        struct node *init = node->u.s.forstmt.init;
        struct node *cond = node->u.s.forstmt.cond;
        struct node *ctrl = node->u.s.forstmt.ctrl;
        if (decl) {
            for (int i=0; decl[i]; i++) {
                struct print_context con = {level, decl[i]};
                print_tree1(con);
            }
        } else if (init) {
            struct print_context con = {level, init};
            print_tree1(con);
        } else {
            for (int i=0; i < level; i++)
                fprintf(stderr, "  ");
            fprintf(stderr, "init: <NULL>\n");
        }
        
        if (cond) {
            struct print_context con = {level, cond};
            print_tree1(con);
        } else {
            for (int i=0; i < level; i++)
                fprintf(stderr, "  ");
            fprintf(stderr, "cond: <NULL>\n");
        }
        
        if (ctrl) {
            struct print_context con = {level, ctrl};
            print_tree1(con);
        } else {
            for (int i=0; i < level; i++)
                fprintf(stderr, "  ");
            fprintf(stderr, "ctrl: <NULL>\n");
        }
    }
}

static void print_tree1(struct print_context context)
{
    struct node *node = context.node;
    int level = context.level + 1;
    
    for (int i=0; i < context.level; i++)
        fprintf(stderr, "  ");
    
    if (isdecl(node))
        print_decl(node, context);
    else if (isexpr(node))
        print_expr(node, context);
    else if (isstmt(node))
        print_stmt(node, context);
    else
        assert(0);
    
    if (LEFT(context.node)) {
        struct print_context lcontext;
        lcontext.level = level;
        lcontext.node = LEFT(context.node);
        print_tree1(lcontext);
    }
    
    if (RIGHT(context.node)) {
        struct print_context rcontext;
        rcontext.level = level;
        rcontext.node = RIGHT(context.node);
        print_tree1(rcontext);
    }
}

void print_tree(struct node *tree)
{
    struct print_context context = {0, tree};
    print_tree1(context);
}