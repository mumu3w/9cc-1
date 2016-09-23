#include <assert.h>
#include "cc.h"

static void abstract_declarator(struct type ** ty);
static void declarator(struct type ** ty, struct token **id, struct symbol ***params);
static void param_declarator(struct type ** ty, struct token **id);
static struct type *tag_decl(void);
static void ids(struct symbol *sym);
static void fields(struct symbol * sym);

typedef struct symbol *decl_p(const char *id, struct type * ty, int sclass, int fspec, struct source src);
static struct symbol *paramdecl(const char *, struct type *, int, int, struct source);
static struct symbol *globaldecl(const char *, struct type *, int, int, struct source);
static struct symbol *localdecl(const char *, struct type *, int, int, struct source);
static void decls(decl_p * dcl);

static void typedefdecl(const char *id, struct type *ty, int fspec, int level, struct source src);
static void funcdef(const char *id, struct type *ftype, int sclass, int fspec, struct symbol *params[], struct source src);

static void func_body(struct symbol *sym);
struct func func;

int first_decl(struct token *t)
{
    return t->kind == STATIC || first_typename(t);
}

int first_stmt(struct token *t)
{
    return t->kind == IF || first_expr(t);
}

int first_expr(struct token *t)
{
    return t->kind == ID;
}

int first_typename(struct token * t)
{
    return t->kind == INT || t->kind == CONST ||
        (t->id == ID && istypedef(TOK_ID_STR(t)));
}

/// declaration-specifier:
///   storage-class-specifier declaration-specifiers[opt]
///   type-specifier          declaration-specifiers[opt]
///   type-qualifier          declaration-specifiers[opt]
///   function-specifier      declaration-specifiers[opt]
///
/// storage-class-specifier:
///   'auto'
///   'extern'
///   'register'
///   'static'
///   'typedef'
///
/// type-qualifier:
///   'const'
///   'volatile'
///   'restrict'
///
/// function-specifier:
///   'inline'
///
/// type-specifier:
///   'void'
///   'char'
///   'short'
///   'int'
///   'long'
///   'float'
///   'double'
///   'signed'
///   'unsigned'
///   '_Bool'
///   '_Complex'
///   '_Imaginary'
///   enum-specifier
///   struct-or-union-specifier
///   typedef-name
///
/// typedef-name:
///   identifier
///
static struct type *specifiers(int *sclass, int *fspec)
{
    int cls, sign, size, type;
    int cons, vol, res, inl;
    struct type *basety, *tydefty;
    int ci;                        // _Complex, _Imaginary

    basety = tydefty = NULL;
    cls = sign = size = type = 0;
    cons = vol = res = inl = 0;
    ci = 0;
    if (sclass == NULL)
        cls = AUTO;

    for (;;) {
        int *p, t = token->id;
        struct token *tok = token;
        switch (token->id) {
        case AUTO:
        case EXTERN:
        case REGISTER:
        case STATIC:
        case TYPEDEF:
            p = &cls;
            gettok();
            break;

        case CONST:
            p = &cons;
            gettok();
            break;

        case VOLATILE:
            p = &vol;
            gettok();
            break;

        case RESTRICT:
            p = &res;
            gettok();
            break;

        case INLINE:
            p = &inl;
            gettok();
            break;

        case ENUM:
        case STRUCT:
        case UNION:
            p = &type;
            basety = tag_decl();
            break;

        case LONG:
            if (size == LONG) {
                t = LONG + LONG;
                size = 0;        // clear
            }
            // go through
        case SHORT:
            p = &size;
            gettok();
            break;

        case FLOAT:
            p = &type;
            basety = floattype;
            gettok();
            break;

        case DOUBLE:
            p = &type;
            basety = doubletype;
            gettok();
            break;

        case VOID:
            p = &type;
            basety = voidtype;
            gettok();
            break;

        case CHAR:
            p = &type;
            basety = chartype;
            gettok();
            break;

        case INT:
            p = &type;
            basety = inttype;
            gettok();
            break;

        case _BOOL:
            p = &type;
            basety = booltype;
            gettok();
            break;

        case SIGNED:
        case UNSIGNED:
            p = &sign;
            gettok();
            break;

        case _COMPLEX:
        case _IMAGINARY:
            p = &ci;
            gettok();
            break;

        case ID:
            if (istypedef(TOK_ID_STR(token))) {
                tydefty = lookup_typedef(TOK_ID_STR(token));
                p = &type;
                gettok();
            } else {
                p = NULL;
            }
            break;

        default:
            p = NULL;
            break;
        }

        if (p == NULL)
            break;

        if (*p != 0) {
            if (p == &cls) {
                if (sclass)
                    error_at(tok->src,
                             "duplicate storage class '%s'",
                             tok2s(tok));
                else
                    error_at(tok->src,
                             "type name does not allow storage class to be specified",
                             tok2s(tok));
            } else if (p == &inl) {
                if (fspec)
                    warning_at(tok->src,
                               "duplicate '%s' declaration specifier",
                               tok2s(tok));
                else
                    error_at(tok->src, "function specifier not allowed");
            } else if (p == &cons || p == &res || p == &vol) {
                warning_at(tok->src,
                           "duplicate '%s' declaration specifier",
                           tok2s(tok));
            } else if (p == &ci) {
                error_at(tok->src,
                         "duplicate _Complex/_Imaginary specifier '%s'",
                         tok2s(tok));
            } else if (p == &sign) {
                error_at(tok->src,
                         "duplicate signed/unsigned speficier '%s'",
                         tok2s(tok));
            } else if (p == &type || p == &size) {
                error_at(tok->src,
                         "duplicate type specifier '%s'",
                         tok2s(tok));
            } else {
                assert(0);
            }
        }

        *p = t;
    }

    // default is int
    if (type == 0) {
        if (sign == 0 && size == 0)
            error("missing type specifier");
        type = INT;
        basety = inttype;
    }
    // type check
    if ((size == SHORT && type != INT) ||
        (size == LONG + LONG && type != INT) ||
        (size == LONG && type != INT && type != DOUBLE)) {
        if (size == LONG + LONG)
            error("%s %s %s is invalid",
                  id2s(size / 2), id2s(size / 2), id2s(type));
        else
            error("%s %s is invalid", id2s(size), id2s(type));
    } else if (sign && type != INT && type != CHAR) {
        error("'%s' cannot be signed or unsigned", id2s(type));
    } else if (ci && type != DOUBLE && type != FLOAT) {
        error("'%s' cannot be %s", id2s(type), id2s(ci));
    }

    if (type == ID)
        basety = tydefty;
    else if (type == CHAR && sign)
        basety = sign == UNSIGNED ? unsignedchartype : signedchartype;
    else if (size == SHORT)
        basety = sign == UNSIGNED ? unsignedshorttype : shorttype;
    else if (type == INT && size == LONG)
        basety = sign == UNSIGNED ? unsignedlongtype : longtype;
    else if (size == LONG + LONG)
        basety = sign == UNSIGNED ? unsignedlonglongtype : longlongtype;
    else if (type == DOUBLE && size == LONG)
        basety = longdoubletype;
    else if (sign == UNSIGNED)
        basety = unsignedinttype;

    // qulifier
    if (cons)
        basety = qual(CONST, basety);
    if (vol)
        basety = qual(VOLATILE, basety);
    if (res)
        basety = qual(RESTRICT, basety);

    if (sclass)
        *sclass = cls;
    if (fspec)
        *fspec = inl;

    return basety;
}

static void array_qualifiers(struct type * atype)
{
    int cons, vol, res;
    int *p;
    cons = vol = res = 0;
    while (token->kind == CONST) {
        int t = token->id;
        struct source src = source;
        switch (t) {
        case CONST:
            p = &cons;
            gettok();
            break;

        case VOLATILE:
            p = &vol;
            gettok();
            break;

        case RESTRICT:
            p = &res;
            gettok();
            break;

        default:
            assert(0);
        }

        if (*p != 0)
            warning_at(src, "duplicate type qualifier '%s'", id2s(*p));

        *p = t;
    }

    if (cons)
        TYPE_A_CONST(atype) = 1;
    if (vol)
        TYPE_A_VOLATILE(atype) = 1;
    if (res)
        TYPE_A_RESTRICT(atype) = 1;
}

static void exit_params(struct symbol *params[])
{
    assert(params);
    if (params[0] && !params[0]->defined)
        error_at(params[0]->src,
                 "a parameter list without types is only allowed in a function definition");
    
    if (cscope > PARAM)
        exit_scope();

    exit_scope();
}

/// parameter-type-list:
///   parameter-list
///   parameter-list ',' '...'
///
/// parameter-list:
///   parameter-declaration
///   parameter-list parameter-declaration
///
/// parameter-declaration:
///   declaration-specifier declarator
///   declaration-specifier abstract-declarator[opt]
///
static struct symbol **prototype(struct type *ftype)
{
    struct list *list = NULL;
    
    for (int i = 0;; i++) {
        struct type *basety = NULL;
        int sclass, fspec;
        struct type *ty = NULL;
        struct token *id = NULL;
        struct symbol *sym;
        struct source src = source;

        basety = specifiers(&sclass, &fspec);
        param_declarator(&ty, &id);
        attach_type(&ty, basety);

        sym = paramdecl(id ? TOK_ID_STR(id) : NULL,
                        ty, sclass, fspec, id ? id->src : src);
        list = list_append(list, sym);
        
        if (token->id != ',')
            break;

        expect(',');
        if (token->id == ELLIPSIS) {
            TYPE_VARG(ftype) = 1;
            gettok();
            break;
        }
    }

    // check
    struct symbol **params = ltoa(&list, FUNC);
    ensure_prototype(ftype, params);

    return params;
}

/// identifier-list:
///   identifier
///   identifier-list ',' identifier
///
static struct symbol **oldstyle(struct type *ftype)
{
    struct list *params = NULL;
    
    for (;;) {
        if (token->id == ID) {
            struct symbol *sym = paramdecl(TOK_ID_STR(token), inttype, 0, 0, token->src);
            sym->defined = false;
            params = list_append(params, sym);
        }
        expect(ID);
        if (token->id != ',')
            break;
        expect(',');
    }

    if (cscope > PARAM)
        error("a parameter list without types is only allowed in a function definition");

    return ltoa(&params, FUNC);
}

static struct symbol **parameters(struct type * ftype)
{
    struct symbol **params;

    if (first_decl(token)) {
        // prototype
        int i;
        struct type **proto;
        
        params = prototype(ftype);
        proto = newarray(sizeof(struct type *), length(params) + 1, PERM);
        for (i = 0; params[i]; i++)
            proto[i] = params[i]->type;

        proto[i] = NULL;
        TYPE_PROTO(ftype) = proto;
        TYPE_OLDSTYLE(ftype) = 0;
    } else if (token->id == ID) {
        // oldstyle
        params = oldstyle(ftype);
        TYPE_OLDSTYLE(ftype) = 1;
    } else if (token->id == ')') {
        params = vtoa(NULL, FUNC);
        TYPE_OLDSTYLE(ftype) = 1;
    } else {
        params = vtoa(NULL, FUNC);
        TYPE_OLDSTYLE(ftype) = 1;

        if (token->id == ELLIPSIS)
            error("ISO C requires a named parameter before '...'");
        else
            error("expect parameter declarator at '%s'", tok2s(token));
        gettok();
    }

    return params;
}

static void parse_assign(struct type *atype)
{
    struct expr *assign = assign_expr();
    TYPE_A_ASSIGN(atype) =assign;

    if (!assign)
        return;

    if (isint(EXPR_TYPE(assign))) {
        // try evaluate the length
        struct expr *ret = eval(assign, longtype);
        if (ret) {
            assert(isiliteral(ret));
            TYPE_LEN(atype) = ILITERAL_VALUE(ret).i;
            if (ILITERAL_VALUE(ret).i < 0)
                error("array has negative size");
        } else {
            error("expect constant expression");
        }
    } else {
        error("size of array has non-integer type '%s'",
              type2s(EXPR_TYPE(assign)));
    }
}

static struct type *arrays(bool abstract)
{
    struct type *atype = array_type(NULL);

    if (abstract) {
        if (token->id == '*') {
            if (lookahead()->id != ']') {
                parse_assign(atype);
            } else {
                expect('*');
                TYPE_A_STAR(atype) = 1;
            }
        } else if (first_expr(token)) {
            parse_assign(atype);
        }
    } else {
        if (token->id == STATIC) {
            expect(STATIC);
            TYPE_A_STATIC(atype) = 1;
            if (token->kind == CONST)
                array_qualifiers(atype);
            parse_assign(atype);
        } else if (token->kind == CONST) {
            if (token->kind == CONST)
                array_qualifiers(atype);
            if (token->id == STATIC) {
                expect(STATIC);
                TYPE_A_STATIC(atype) = 1;
                parse_assign(atype);
            } else if (token->id == '*') {
                if (lookahead()->id != ']') {
                    parse_assign(atype);
                } else {
                    expect('*');
                    TYPE_A_STAR(atype) = 1;
                }
            } else if (first_expr(token)) {
                parse_assign(atype);
            }
        } else if (token->id == '*') {
            if (lookahead()->id != ']') {
                parse_assign(atype);
            } else {
                expect('*');
                TYPE_A_STAR(atype) = 1;
            }
        } else if (first_expr(token)) {
            parse_assign(atype);
        }
    }

    return atype;
}

static struct type *func_or_array(bool abstract, struct symbol ***params)
{
    struct type *ty = NULL;
    int follow[] = { '[', ID, IF, 0 };

    for (; token->id == '(' || token->id == '[';) {
        if (token->id == '[') {
            struct type *atype;
            expect('[');
            atype = arrays(abstract);
            match(']', follow);
            attach_type(&ty, atype);
        } else {
            struct symbol **args;
            struct type *ftype = func_type();
            expect('(');
            /**
             * To make it easy to distinguish between 'paramaters in parameter'
             * and 'compound statement of function definition', they both may be
             * at scope LOCAL (aka PARAM+1), so enter scope again to make things
             * easy.
             */
            enter_scope();
            if (cscope > PARAM)
                enter_scope();
            args = parameters(ftype);
            if (params && *params == NULL)
                *params = args;
            else
                exit_params(args);
            match(')', follow);
            attach_type(&ty, ftype);
        }
    }

    return ty;
}

/// enum-specifier:
///   'enum' identifier[opt] '{' enumerator-list '}'
///   'enum' identifier[opt] '{' enumerator-list ',' '}'
///   'enum' identifier
///
/// struct-or-union-specifier:
///   struct-or-union identifier[opt] '{' struct-declaration-list '}'
///   struct-or-union identifier
///
/// struct-or-union:
///   'struct'
///   'union'
///
static struct type *tag_decl(void)
{
    int t = token->id;
    const char *id = NULL;
    struct symbol *sym = NULL;
    struct source src = source;
    int follow[] = {INT, CONST, STATIC, IF, 0};

    expect(t);
    if (token->id == ID) {
        id = TOK_ID_STR(token);
        expect(ID);
    }
    if (token->id == '{') {
        expect('{');
        sym = tag_type(t, id, src);
        if (t == ENUM)
            ids(sym);
        else
            fields(sym);
        match('}', follow);
        sym->defined = true;
    } else if (id) {
        sym = lookup(id, tags);
        if (sym) {
            if (is_current_scope(sym) && TYPE_OP(sym->type) != t)
                error_at(src,
                         "use of '%s' with tag type that does not match previous declaration '%s' at %s:%u:%u",
                         id2s(t), type2s(sym->type),
                         sym->src.file,
                         sym->src.line,
                         sym->src.column);
        } else {
            sym = tag_type(t, id, src);
        }
    } else {
        error("expected identifier or '{'");
        sym = tag_type(t, NULL, src);
    }

    return sym->type;
}

/// enumerator-list:
///   enumerator
///   enumerator-list ',' enumerator
///
/// enumerator:
///   enumeration-constant
///   enumeration-constant '=' constant-expression
///
/// enumeration-constant:
///   identifier
///
static void ids(struct symbol *sym)
{
    if (token->id == ID) {
        int val = 0;
        do {
            const char *name = TOK_ID_STR(token);
            struct symbol *s = lookup(name, identifiers);
            if (s && is_current_scope(s))
                redefinition_error(source, s);

            s = install(name, &identifiers, cscope, cscope < LOCAL ? PERM : FUNC);
            s->type = sym->type;
            s->src = source;
            s->sclass = ENUM;
            expect(ID);
            if (token->id == '=') {
                expect('=');
                val = intexpr();
            }
            s->value.u = val++;
            if (token->id != ',')
                break;
            expect(',');
        } while (token->id == ID);
    } else {
        error("expect identifier");
    }
}

static void bitfield(struct field *field)
{
    field->src = source;
    expect(':');
    field->bitsize = intexpr();
    field->isbit = true;
}

/// struct-declaration-list:
///   struct-declaration
///   struct-declaration-list struct-declaration
///
/// struct-declaration:
///   specifier-qualifier-list struct-declarator-list ';'
///
/// specifier-qualifier-list:
///   type-specifier specifier-qualifier-list[opt]
///   type-qualifier specifier-qualifier-list[opt]
///
/// struct-declarator-list:
///   struct-declarator
///   struct-declarator-list ',' struct-declarator
///
/// struct-declarator:
///   declarator
///   declarator[opt] ':' constant-expression
///
static void fields(struct symbol * sym)
{
    int follow[] = {INT, CONST, '}', IF, 0};
    struct type *sty = sym->type;

    if (!first_decl(token)) {
        // supports empty record
        if (token->id != '}')
            error("expect type name or qualifiers");
        return;
    }
    
    struct vector *v = vec_new();
    do {
        struct type *basety = specifiers(NULL, NULL);

        for (;;) {
            struct field *field = alloc_field();
            if (token->id == ':') {
                bitfield(field);
                field->type = basety;
            } else if (token->id == ';' &&
                       isrecord(basety) &&
                       is_anonymous(TYPE_TAG(basety))) {
                //C11: anonymous record
                size_t len = length(TYPE_FIELDS(basety));
                for (int i = 0; i < len; i++) {
                    struct field *field = TYPE_FIELDS(basety)[i];
                    vec_push(v, field);
                    if (i < len - 1)
                        ensure_field(field, vec_len(v), false);
                }
                goto next;
            } else {
                struct type *ty = NULL;
                struct token *id = NULL;
                declarator(&ty, &id, NULL);
                attach_type(&ty, basety);
                if (token->id == ':')
                    bitfield(field);
                field->type = ty;
                if (id) {
                    const char *name = TOK_ID_STR(id);
                    for (int i = 0; i < vec_len(v); i++) {
                        struct field *f = vec_at(v, i);
                        if (f->name && !strcmp(f->name, name)) {
                            error_at(id->src, "redefinition of '%s'", name);
                            break;
                        }
                    }
                    field->name = name;
                    field->src = id->src;
                }
            }

            vec_push(v, field);
            if (token->id != ',')
                break;
            expect(',');
            ensure_field(field, vec_len(v), false);
        }
    next:
        match(';', follow);
        ensure_field(vec_tail(v), vec_len(v), isstruct(sty) && !first_decl(token));
    } while (first_decl(token));

    TYPE_FIELDS(sty) = vtoa(v, PERM);
    set_typesize(sty);
}

/// pointer:
///   '*' type-qualifier-list[opt]
///   '*' type-qualifier-list[opt] pointer
///
/// type-qualifier-list:
///   type-qualifier
///   type-qualifier-list type-qualifier
///
static struct type *ptr_decl(void)
{
    struct type *ret = NULL;
    int con, vol, res, type;

    assert(token->id == '*');

    for (;;) {
        int *p, t = token->id;
        switch (token->id) {
        case CONST:
            p = &con;
            break;

        case VOLATILE:
            p = &vol;
            break;

        case RESTRICT:
            p = &res;
            break;

        case '*':
            {
                struct type *pty = ptr_type(NULL);
                con = vol = res = type = 0;
                p = &type;
                prepend_type(&ret, pty);
            }
            break;

        default:
            p = NULL;
            break;
        }

        if (p == NULL)
            break;

        if (*p != 0)
            warning("duplicate type qulifier '%s'", tok2s(token));

        *p = t;

        if (t == CONST || t == VOLATILE || t == RESTRICT)
            ret = qual(t, ret);

        gettok();
    }

    return ret;
}

static void param_declarator(struct type ** ty, struct token **id)
{
    if (token->id == '*') {
        struct type *pty = ptr_decl();
        prepend_type(ty, pty);
    }

    if (token->id == '(') {
        if (first_decl(lookahead())) {
            abstract_declarator(ty);
        } else {
            struct type *type1 = *ty;
            struct type *rtype = NULL;
            expect('(');
            param_declarator(&rtype, id);
            expect(')');
            if (token->id == '(' || token->id == '[') {
                struct type *faty;
                assert(id);
                if (*id)
                    faty = func_or_array(false, NULL);
                else
                    faty = func_or_array(true, NULL);

                attach_type(&faty, type1);
                attach_type(&rtype, faty);
            } else {
                attach_type(&rtype, type1);
            }
            *ty = rtype;
        }
    } else if (token->id == '[') {
        abstract_declarator(ty);
    } else if (token->id == ID) {
        declarator(ty, id, NULL);
    }
}

/// abstract-declarator:
///   pointer
///   pointer[opt] direct-abstract-declarator
///
/// direct-abstract-declarator:
///   '(' abstract-declarator ')'
///   direct-abstract-declarator[opt] '[' assignment-expression[opt] ']'
///   direct-abstract-declarator[opt] '[' '*' ']'
///   direct-abstract-declarator[opt] '(' parameter-type-list[opt] ')'
///
static void abstract_declarator(struct type ** ty)
{
    assert(ty);

    if (token->id == '*' || token->id == '(' || token->id == '[') {
        if (token->id == '*') {
            struct type *pty = ptr_decl();
            prepend_type(ty, pty);
        }

        if (token->id == '(') {
            if (first_decl(lookahead())) {
                struct type *faty = func_or_array(true, NULL);
                prepend_type(ty, faty);
            } else {
                struct type *type1 = *ty;
                struct type *rtype = NULL;
                expect('(');
                abstract_declarator(&rtype);
                expect(')');
                if (token->id == '[' || token->id == '(') {
                    struct type *faty = func_or_array(true, NULL);
                    attach_type(&faty, type1);
                    attach_type(&rtype, faty);
                } else {
                    attach_type(&rtype, type1);
                }
                *ty = rtype;
            }
        } else if (token->id == '[') {
            struct type *faty = func_or_array(true, NULL);
            prepend_type(ty, faty);
        }
    } else {
        error("expect '(' or '[' at '%s'", tok2s(token));
    }
}

/// declarator:
///   pointer[opt] direct-declarator
///
/// direct-declarator:
///   identifier
///   '(' declarator ')'
///   direct-declarator '[' type-qualifier-list[opt] assignment-expression[opt] ']'
///   direct-declarator '[' 'static' type-qualifier-list[opt] assignment-expression ']'
///   direct-declarator '[' type-qualifier-list 'static' assignment-expression ']'
///   direct-declarator '[' type-qualifier-list[opt] '*' ']'
///   direct-declarator '(' parameter-type-list ')'
///   direct-declarator '(' identifier-list[opt] ')'
///
static void declarator(struct type ** ty, struct token **id, struct symbol ***params)
{
    int follow[] = { ',', '=', IF, 0 };

    assert(ty && id);
    
    if (token->id == '*') {
        struct type *pty = ptr_decl();
        prepend_type(ty, pty);
    }

    if (token->id == ID) {
        *id = token;
        expect(ID);
        if (token->id == '[' || token->id == '(') {
            struct type *faty = func_or_array(false, params);
            prepend_type(ty, faty);
        }
    } else if (token->id == '(') {
        struct type *type1 = *ty;
        struct type *rtype = NULL;
        expect('(');
        declarator(&rtype, id, params);
        match(')', follow);
        if (token->id == '[' || token->id == '(') {
            struct type *faty = func_or_array(false, params);
            attach_type(&faty, type1);
            attach_type(&rtype, faty);
        } else {
            attach_type(&rtype, type1);
        }
        *ty = rtype;
    } else {
        error("expect identifier or '('");
    }
}

/// external-declaration:
///   declaration
///   function-definition
///
/// declaration:
///   declaration-specifier init-declarator-list[opt] ';'
///
/// function-definition:
///   declaration-specifier declarator declaration-list[opt] compound-statement
///
/// init-declarator-list:
///   init-declarator
///   init-declarator-list ',' init-declarator
///
/// init-declarator:
///   declarator
///   declarator '=' initializer
///
static void decls(decl_p * dcl)
{
    struct type *basety;
    int sclass, fspec;
    int level = cscope;
    int follow[] = {STATIC, INT, CONST, IF, '}', 0};

    basety = specifiers(&sclass, &fspec);
    if (token->id == ID || token->id == '*' || token->id == '(') {
        struct token *id = NULL;
        struct type *ty = NULL;
        struct symbol **params = NULL;        // for functioness
        struct source src = source;

        // declarator
        if (level == GLOBAL)
            declarator(&ty, &id, &params);
        else
            declarator(&ty, &id, NULL);
        attach_type(&ty, basety);

        if (level == GLOBAL && params) {
            if (isfunc(ty) && (token->id == '{' ||
                               (first_decl(token) && TYPE_OLDSTYLE(ty)))) {
                if (TYPE_OLDSTYLE(ty))
                    exit_scope();
                
                funcdef(id ? TOK_ID_STR(id) : NULL,
                        ty, sclass, fspec, params, id ? id->src : src);
                return;
            } else {
                exit_params(params);
            }
        }

        for (;;) {
            if (id) {
                if (sclass == TYPEDEF)
                    typedefdecl(TOK_ID_STR(id), ty, fspec, level, id->src);
                else
                    dcl(TOK_ID_STR(id), ty, sclass, fspec, id->src);
            }

            if (token->id != ',')
                break;

            expect(',');
            id = NULL;
            ty = NULL;
            // declarator
            declarator(&ty, &id, NULL);
            attach_type(&ty, basety);
        }
    } else if (isenum(basety) || isstruct(basety) || isunion(basety)) {
        // struct/union/enum
        actions.deftype(TYPE_TSYM(basety));
    } else {
        error("invalid token '%s' in declaration", tok2s(token));
    }
    match(';', follow);
}

/// type-name:
///   specifier-qualifier-list abstract-declarator[opt]
///
struct type *typename(void)
{
    struct type *basety;
    struct type *ty = NULL;

    basety = specifiers(NULL, NULL);
    if (token->id == '*' || token->id == '(' || token->id == '[')
        abstract_declarator(&ty);

    attach_type(&ty, basety);

    return ty;
}

struct symbol *mklocalvar(const char *name, struct type * ty, int sclass)
{
    return localdecl(name, ty, sclass, 0, source);
}

struct symbol *mktmpvar(struct type *ty, int sclass)
{
    return localdecl(gen_tmpname(), ty, sclass, 0, source);
}

void declaration(void)
{
    assert(cscope >= LOCAL);
    decls(localdecl);
}

static void doglobal(struct symbol *sym, void *context)
{
    if (sym->sclass == EXTERN ||
        isfunc(sym->type) ||
        sym->defined)
        return;

    actions.defgvar(sym);
}

/// translation-unit:
///   external-declaration
///   translation-unit external-declaration
///
void translation_unit(void)
{
    for (gettok(); token->id != EOI;) {
        if (first_decl(token)) {
            assert(cscope == GLOBAL);
            decls(globaldecl);
            deallocate(FUNC);
        } else {
            if (token->id == ';')
                // empty declaration
                gettok();
            else
                skipto(first_decl);
        }
    }
    
    foreach(identifiers, GLOBAL, doglobal, NULL);
}

/// decl functions

static void typedefdecl(const char *id, struct type *ty, int fspec, int level, struct source src)
{
    int sclass = TYPEDEF;

    assert(id);

    if (level == PARAM) {
        error("invalid storage class specifier '%s' in function declarator",
              id2s(sclass));
        sclass = 0;
    }

    if (isfunc(ty))
        ensure_func(ty, src);
    else if (isarray(ty))
        ensure_array(ty, src, level);

    ensure_inline(ty, fspec, src);

    struct symbol *sym = lookup(id, identifiers);
    if (sym && is_current_scope(sym))
        redefinition_error(src, sym);
    sym = install(id, &identifiers, cscope, cscope < LOCAL ? PERM : FUNC);
    sym->type = ty;
    sym->src = src;
    sym->sclass = sclass;

    if (token->id == '=') {
        error("illegal initializer (only variable can be initialized)");
        initializer(NULL);
    }

    actions.deftype(sym);
}

// id maybe NULL
static struct symbol *paramdecl(const char *id, struct type * ty, int sclass, int fspec, struct source src)
{
    struct symbol *sym = NULL;

    if (sclass && sclass != REGISTER) {
        error("invalid storage class specifier '%s' in function declarator",
              id2s(sclass));
        sclass = 0;
    }

    if (isfunc(ty)) {
        ensure_func(ty, src);
        ty = ptr_type(ty);
    } else if (isarray(ty)) {
        ensure_array(ty, src, PARAM);
        struct type *aty = ty;
        ty = ptr_type(rtype(ty));
        if (TYPE_A_CONST(aty))
            ty = qual(CONST, ty);
        if (TYPE_A_VOLATILE(aty))
            ty = qual(RESTRICT, ty);
        if (TYPE_A_RESTRICT(aty))
            ty = qual(VOLATILE, ty);
    } else if (isenum(ty) || isstruct(ty) || isunion(ty)) {
        if (!TYPE_TSYM(ty)->defined ||
            TYPE_TSYM(ty)->scope == cscope)
            warning_at(src,
                       "declaration of '%s' will not be visible outside of this function",
                       type2s(ty));
    }

    // check inline after conversion (decay)
    ensure_inline(ty, fspec, src);
        
    if (id) {
        sym = lookup(id, identifiers);
        if (sym && sym->scope == cscope)
            redefinition_error(source, sym);
        sym = install(id, &identifiers, cscope, FUNC);
    } else {
        sym = anonymous(&identifiers, cscope, FUNC);
    }

    sym->type = ty;
    sym->src = src;
    sym->sclass = sclass;
    sym->defined = true;
    
    if (token->id == '=') {
        error("C does not support default arguments");
        initializer(NULL);
    }

    return sym;
}

static struct symbol *localdecl(const char *id, struct type * ty, int sclass, int fspec, struct source src)
{
    struct symbol *sym = NULL;

    assert(id);
    assert(cscope >= LOCAL);

    if (isfunc(ty)) {
        ensure_func(ty, src);
        ensure_main(ty, id, src);
        if (sclass && sclass != EXTERN) {
            error_at(src,
                     "function declared in block scope cannot have '%s' storage class",
                     id2s(sclass));
            sclass = 0;
        }
    } else if (isarray(ty)) {
        ensure_array(ty, src, LOCAL);
    }

    ensure_inline(ty, fspec, src);

    bool globl = isfunc(ty) || sclass == EXTERN;

    sym = lookup(id, identifiers);
    if (sym &&
        (is_current_scope(sym) ||
         (globl && (isfunc(sym->type) ||
                    sym->sclass == EXTERN)))) {
        redefinition_error(src, sym);
    } else {
        sym = install(id, &identifiers, cscope, FUNC);
        sym->type = ty;
        sym->src = src;
        sym->sclass = sclass;
        if (!globl)
            sym->defined = true;
    }

    if (token->id == '=') {
        gettok();
        if (!(isscalar(ty) || isarray(ty) || isrecord(ty))) {
            error("'%s' cannot have an initializer", TYPE_NAME(ty));
            initializer(NULL);
        } else if (sclass == EXTERN) {
            error("'extern' variable cannot have an initializer");
            initializer(NULL);
        } else if (istag(ty) && isincomplete(ty)) {
            error("variable has incomplete type '%s'", type2s(ty));
            initializer(NULL);
        } else {
            struct expr *init = initializer(ty);
            init = ensure_init(init, ty, sym);
            if (init) {
                sym->u.init = init;
                // gen assign expr
                if (sclass != STATIC)
                    actions.gen(assign(sym, init));
            }
        }
    }

    // check incomplete type
    ensure_decl(sym);

    // actions
    if (isfunc(ty))
        actions.dclfun(sym);
    else if (sclass == EXTERN)
        actions.dclgvar(sym);
    else if (sclass == STATIC)
        actions.defsvar(sym, func.name);

    return sym;
}

static struct symbol *globaldecl(const char *id, struct type *ty, int sclass, int fspec, struct source src)
{
    struct symbol *sym = NULL;

    assert(id);
    assert(cscope == GLOBAL);

    if (sclass == AUTO || sclass == REGISTER) {
        error_at(src, "illegal storage class on file-scoped variable");
        sclass = 0;
    }

    if (isfunc(ty)) {
        ensure_func(ty, src);
        ensure_main(ty, id, src);
    } else if (isarray(ty)) {
        ensure_array(ty, src, GLOBAL);
    }

    ensure_inline(ty, fspec, src);

    sym = lookup(id, identifiers);
    if (!sym || sym->scope != cscope) {
        sym = install(id, &identifiers, cscope, PERM);
        sym->type = ty;
        sym->src = src;
        sym->sclass = sclass;
    } else if (eqtype(ty, sym->type)) {
        if (sclass == STATIC && sym->sclass != STATIC)
            error_at(src, "static declaration of '%s' follows non-static declaration", id);
        else if (sym->sclass == STATIC && sclass != STATIC)
            error_at(src, "non-static declaration of '%s' follows static declaration", id);

        if (sclass != EXTERN)
            sym->sclass = sclass;
    } else {
        conflicting_types_error(src, sym);
    }

    if (token->id == '=') {
        gettok();
        if (!(isscalar(ty) || isarray(ty) || isrecord(ty))) {
            error("'%s' cannot have an initializer", TYPE_NAME(ty));
            initializer(NULL);
        } else if (istag(ty) && isincomplete(ty)) {
            error("variable has incomplete type '%s'", type2s(ty));
            initializer(NULL);
        } else {
            struct expr *init = initializer(ty);

            if (sclass == EXTERN)
                warning_at(EXPR_SRC(init), "'extern' variable has an initializer");

            if (sym->defined)
                redefinition_error(src, sym);

            init = ensure_init(init, ty, sym);
            sym->u.init = init;
        }

        sym->defined = true;
    }

    // check incomplete type
    ensure_decl(sym);

    // actions
    if (sym->u.init)
        actions.defgvar(sym);
    else if (isfunc(ty))
        actions.dclfun(sym);
    else
        actions.dclgvar(sym);

    return sym;
}

static void oldparam(struct symbol *sym, void *context)
{
    struct symbol **params = context;

    assert(sym->name);
    if (!isvardecl(sym)) {
        warning_at(sym->src, "empty declaraion");
        return;
    }
        
    int j;
    for (j = 0; params[j]; j++) {
        struct symbol *s = params[j];
        if (s->name && !strcmp(s->name, sym->name))
            break;
    }

    if (params[j])
        params[j] = sym;
    else
        error_at(sym->src, "parameter named '%s' is missing", sym->name);
}

static void make_funcdecl(struct symbol *sym, struct type *ty, int sclass, struct source src)
{
    sym->type = ty;
    sym->src = src;
    sym->defined = true;
    sym->sclass = sclass;
}

// id maybe NULL
static void funcdef(const char *id, struct type *ftype, int sclass, int fspec,
                    struct symbol *params[], struct source src)
{
    struct symbol *sym;
    // cscope == PARAM (prototype)
    // cscope == GLOBAL (oldstyle)

    if (sclass && sclass != EXTERN && sclass != STATIC) {
        error("invalid storage class specifier '%s'", id2s(sclass));
        sclass = 0;
    }
    
    if (id) {
        sym = lookup(id, identifiers);
        if (!sym || sym->scope != GLOBAL) {
            sym = install(id, &identifiers, GLOBAL, PERM);
            make_funcdecl(sym, ftype, sclass, src);
        } else if (eqtype(ftype, sym->type) && !sym->defined) {
            if (sclass == STATIC && sym->sclass != STATIC)
                error_at(src,
                         "static declaaration of '%s' follows non-static declaration",
                         id);
            else
                make_funcdecl(sym, ftype, sclass, src);
        } else {
            redefinition_error(src, sym);
        }

        ensure_func(ftype, src);
        ensure_main(ftype, id, src);
        ensure_inline(ftype, fspec, src);
    } else {
        sym = anonymous(&identifiers, GLOBAL, PERM);
        make_funcdecl(sym, ftype, sclass, src);
    }

    // old style function parameters declaration
    if (TYPE_OLDSTYLE(ftype)) {
        enter_scope();
        assert(cscope == PARAM);
        /// declaration-list:
        ///   declaration
        ///   declaration-list declaration
        ///
        while (first_decl(token))
            decls(paramdecl);

        foreach(identifiers, PARAM, oldparam, params);

        for (int i = 0; params[i]; i++) {
            struct symbol *p = params[i];
            if (!p->defined)
                params[i] = paramdecl(p->name, inttype, 0, 0, p->src);
            // check void
            if (isvoid(p->type)) {
                error_at(p->src, "argument may not have 'void' type");
                p->type = inttype;
            }
        }

        int i;
        struct type **proto = newarray(sizeof(struct type *), length(params) + 1, PERM);
        for (i = 0; params[i]; i++)
            proto[i] = params[i]->type;

        proto[i] = NULL;
        TYPE_PROTO(ftype) = proto;
    
        if (token->id != '{')
            error("expect function body after function declarator");
    }

    TYPE_PARAMS(ftype) = params;
    ensure_params(params);

    if (token->id == '{') {
        // function definition
        func_body(sym);
        exit_scope();
        actions.defun(sym);
    }
}

static void predefined_ids(void)
{
    /**
     * Predefined identifier: __func__
     * The identifier __func__ is implicitly declared by C99
     * implementations as if the following declaration appeared
     * after the opening brace of each function definition:
     *
     * static const char __func__[] = "function-name";
     *
     */
    struct type *type = array_type(qual(CONST, chartype));
    struct symbol *sym = mklocalvar("__func__", type, STATIC);
    sym->predefine = true;
    // initializer
    struct expr *literal = new_string_literal(func.name);
    init_string(type, literal);
    sym->u.init = literal;
}

static void func_body(struct symbol *sym)
{    
    func.gotos = vec_new();
    func.labels = new_table(NULL, LOCAL);
    func.type = sym->type;
    func.name = sym->name;
    func.calls = vec_new();

    // compound statement
    compound_stmt(predefined_ids, 0, 0, NULL);
    // check goto labels
    ensure_gotos();

    // save
    sym->calls = vtoa(func.calls, FUNC);

    free_table(func.labels);
    func.gotos = NULL;
    func.labels = NULL;
    func.type = NULL;
    func.name = NULL;
    func.calls = NULL;
}
