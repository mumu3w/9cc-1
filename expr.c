#include "cc.h"

static node_t * cast_expr(void);
static node_t * cond_expr(void);
static node_t * cond_expr1(node_t *o);
static node_t * unary_expr(void);
static node_t * uop(int op, node_t *ty, node_t *l);
static node_t * bop(int op, node_t *l, node_t *r);
static node_t * logicop(int op, node_t *l, node_t *r);
static node_t * commaop(int op, node_t *l, node_t *r);
static node_t * assignop(int op, node_t *l, node_t *r);
static node_t * assignconv(node_t *ty, node_t *node);
static node_t * decay(node_t *node);
static node_t * ltor(node_t *node);
static node_t * conv(node_t *node);
static node_t * conva(node_t *node);
static node_t * conv2(node_t *l, node_t *r);
static node_t * wrap(node_t *ty, node_t *node);
static node_t * bitconv(node_t *ty, node_t *node);
static bool is_nullptr(node_t *node);
static inline node_t * expr_node(int id, int op, node_t *ty, node_t *l, node_t *r);

#define INTEGER_MAX(type)    (VALUE_I(TYPE_LIMITS_MAX(type)))
#define UINTEGER_MAX(type)   (VALUE_U(TYPE_LIMITS_MAX(type)))

#define INCOMPATIBLE_TYPES2   "imcompatible types '%s' and '%s' in conditional expression"

static int splitop(int op)
{
    switch (op) {
    case MULEQ: return '*';
    case DIVEQ: return '/';
    case MODEQ: return '%';
    case ADDEQ: return '+';
    case MINUSEQ: return '-';
    case LSHIFTEQ: return LSHIFT;
    case RSHIFTEQ: return RSHIFT;
    case BANDEQ: return '&';
    case BOREQ: return '|';
    case XOREQ: return '^';
    default: CCAssert(0);
    }
}

static unsigned escape(const char **ps)
{
    unsigned c = 0;
    const char *s = *ps;
    CCAssert(*s == '\\');
    s += 1;
    switch (*s++) {
    case 'a': c = 7; break;
    case 'b': c = '\b'; break;
    case 'f': c = '\f'; break;
    case 'n': c = '\n'; break;
    case 'r': c = '\r'; break;
    case 't': c = '\t'; break;
    case 'v': c = '\v'; break;
    case '\'': case '"':
    case '\\': case '\?':
	c = s[-1];
	break;
    case '0': case '1': case '2':
    case '3': case '4': case '5':
    case '6': case '7':
	c = s[-1] - '0';
	if (*s >= '0' && *s <= '7') {
	    c = (c<<3) + (*s++) - '0';
	    if (*s >= '0' && *s <= '7')
		c = (c<<3) + (*s++) - '0';
	}
	break;
    case 'x':
        {
            bool overflow = 0;
            for (;is_digithex(*s);) {
                if (overflow) {
                    s++;
                    continue;
                }
                if (c >> (BITS(TYPE_SIZE(wchartype)) - 4)) {
                    overflow = 1;
                    error("hex escape sequence out of range");
                } else {
                    if (is_digit(*s))
                        c = (c<<4) + *s - '0';
                    else
                        c = (c<<4) + (*s & 0x5f) - 'A' + 10;
                }
                s++;
            }
        }
	break;
    case 'u': case 'U':
        {
            int x = 0;
            int n = s[-1] == 'u' ? 4 : 8;
            for (;is_digithex(*s); x++, s++) {
                if (x == n)
                    break;
                if (is_digit(*s))
                    c = (c<<4) + *s - '0';
                else
                    c = (c<<4) + (*s & 0x5f) - 'A' + 10;
            }
        }
	break;
    default:
	c = s[-1];
	break;
    }
    
    *ps = s;
    return c;
}

static void char_constant(struct token *t, node_t *sym)
{
    const char *s = t->name;
    bool wide = s[0] == 'L';
    unsigned long long c = 0;
    char ws[MB_LEN_MAX];
    int len = 0;
    bool overflow = 0;
    bool char_rec = 0;
    wide ? (s += 2) : (s += 1);
    
    for (;*s != '\'';) {
        if (char_rec)
            overflow = 1;
        if (*s == '\\') {
            c = escape(&s);
            char_rec = 1;
        } else {
            if (wide) {
                if (len >= MB_LEN_MAX)
                    error("multibyte character overflow");
                else
                    ws[len++] = (char) *s++;
            } else {
                c = *s++;
                char_rec = 1;
            }
        }
    }
    
    if (!char_rec && !len)
        error("incomplete character constant: %s", t->name);
    else if (overflow)
        error("extraneous characters in character constant: %s", t->name);
    else if ((!wide && c > UINTEGER_MAX(unsignedchartype)) ||
             (wide && c > UINTEGER_MAX(wchartype)))
        error("character constant overflow: %s", t->name);
    else if (len && mbtowc((wchar_t *)&c, ws, len) != len)
        error("illegal multi-character sequence");
    
    SYM_VALUE_U(sym) = wide ? (wchar_t)c : (unsigned char)c;
    SYM_TYPE(sym) = wide ? wchartype : unsignedchartype;
}

static void integer_constant(struct token *t, node_t *sym)
{
    const char *s = t->name;
    if (s[0] == '\'' || s[1] == 'L')
        return char_constant(t, sym);
    
    int base;
    node_t *ty;
    bool overflow = 0;
    unsigned long long n = 0;
    
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        s = s + 2;
        for (;is_digithex(*s);) {
            if (n & ~(~0ULL >> 4)) {
                overflow = 1;
            } else {
                int d;
                if (is_hex(*s))
                    d = (*s & 0x5f) - 'A' + 10;
                else
                    d = *s - '0';
                
                n = (n<<4) + d;
            }
            s++;
        }
    } else if (s[0] == '0') {
        base = 8;
        bool err = 0;
        for (;is_digit(*s);) {
            if (*s == '8' || *s == '9')
                err = 1;
            
            if (n & ~(~0ULL >> 3))
                overflow = 1;
            else
                n = (n<<3) + (*s - '0');
            
            s++;
        }
        
        if (err)
            error("invalid octal constant %s", t->name);
    } else {
        base = 10;
        for (;is_digit(*s);) {
            int d = *s - '0';
            if (n > (UINTEGER_MAX(unsignedlonglongtype) - d)/10)
                overflow = 1;
            else
                n = n*10 + (*s - '0');
            
            s++;
        }
    }
    
    int ull = (s[0] == 'u' || s[0] == 'U') &&
	((s[1] == 'l' && s[2] == 'l') || (s[1] == 'L' && s[2] == 'L'));
    int llu = ((s[0] == 'l' && s[1] == 'l') || (s[0] == 'L' && s[1] == 'L')) &&
	(s[2] == 'u' || s[2] == 'U');
    int ll = (s[0] == 'l' && s[1] == 'l') || (s[0] == 'L' && s[1] == 'L');
    int lu = (s[0] == 'l' || s[0] == 'L') && (s[1] == 'u' || s[1] == 'U');
    int ul = (s[0] == 'u' || s[0] == 'U') && (s[1] == 'l' || s[1] == 'L');
    int l = s[0] == 'l' || s[0] == 'L';
    int u = s[0] == 'u' || s[0] == 'U';
    
    if (ull || llu) {
        ty = unsignedlonglongtype;
    } else if (ll) {
        if (n > INTEGER_MAX(longlongtype) && base != 10)
            ty = unsignedlonglongtype;
        else
            ty = longlongtype;
    } else if (lu || ul) {
        if (n > UINTEGER_MAX(unsignedlongtype))
            ty = unsignedlonglongtype;
        else
            ty = unsignedlongtype;
    } else if (l) {
        if (base == 10) {
            if (n > INTEGER_MAX(longtype))
                ty = longlongtype;
            else
                ty = longtype;
        } else {
            if (n > INTEGER_MAX(longlongtype))
                ty = unsignedlonglongtype;
            else if (n > UINTEGER_MAX(unsignedlongtype))
                ty = longlongtype;
            else if (n > INTEGER_MAX(longtype))
                ty = unsignedlongtype;
            else
                ty = longtype;
        }
    } else if (u) {
        if (n > UINTEGER_MAX(unsignedlongtype))
            ty = unsignedlonglongtype;
        else if (n > UINTEGER_MAX(unsignedinttype))
            ty = unsignedlongtype;
        else
            ty = unsignedinttype;
    } else {
        if (base == 10) {
            if (n > INTEGER_MAX(longtype))
                ty = longlongtype;
            else if (n > INTEGER_MAX(inttype))
                ty = longtype;
            else
                ty = inttype;
        } else {
            if (n > INTEGER_MAX(longlongtype))
                ty = unsignedlonglongtype;
            else if (n > UINTEGER_MAX(unsignedlongtype))
                ty = longlongtype;
            else if (n > INTEGER_MAX(longtype))
                ty = unsignedlongtype;
            else if (n > UINTEGER_MAX(unsignedinttype))
                ty = longtype;
            else if (n > INTEGER_MAX(inttype))
                ty = unsignedinttype;
            else
                ty = inttype;
        }
    }
    
    SYM_TYPE(sym) = ty;
    
    switch (TYPE_OP(SYM_TYPE(sym))) {
    case INT:
	if (overflow || n > INTEGER_MAX(longlongtype))
	    error("integer constant overflow: %s", t->name);
	SYM_VALUE_I(sym) = n;
	break;
    case UNSIGNED:
	if (overflow)
	    error("integer constant overflow: %s", t->name);
	SYM_VALUE_U(sym) = n;
	break;
    default:
	CCAssert(0);
    }
}

static void float_constant(struct token *t, node_t *sym)
{
    const char *s = t->name;
    char c = s[strlen(s)-1];
    errno = 0;			// must clear first
    if (c == 'f' || c == 'F') {
        SYM_TYPE(sym) = floattype;
        SYM_VALUE_D(sym) = strtof(s, NULL);
    } else if (c == 'l' || c == 'L') {
        SYM_TYPE(sym) = longdoubletype;
        SYM_VALUE_D(sym) = strtold(s, NULL);
    } else {
        SYM_TYPE(sym) = doubletype;
        SYM_VALUE_D(sym) = strtod(s, NULL);
    }
    
    if (errno == ERANGE)
        error("float constant overflow: %s", s);
}

static void string_constant(struct token *t, node_t *sym)
{
    const char *s = t->name;
    bool wide = s[0] == 'L' ? true : false;
    node_t *ty;
    if (wide) {
        size_t len = strlen(s) - 3;
        wchar_t ws[len+1];
        errno = 0;
        size_t wlen = mbstowcs(ws, s+2, len);
        if (errno == EILSEQ)
            error("invalid multibyte sequence: %s", s);
        CCAssert(wlen<=len+1);
        ty = array_type(wchartype);
        TYPE_LEN(ty) = wlen;
	set_typesize(ty);
    } else {
        ty = array_type(chartype);
        TYPE_LEN(ty) = strlen(s)-1;
	set_typesize(ty);
    }
    SYM_TYPE(sym) = ty;
}

static void ensure_type(node_t *node, bool (*is) (node_t *))
{
    const char *name;
    if (is == isint)
        name = "integer";
    else if (is == isscalar)
        name = "scalar";
    else if (is == isarith)
        name = "arithmetic";
    else if (is == isrecord)
	name = "struct or union";
    else if (is == isfunc)
        name = "function";
    else if (is == isptr)
	name = "pointer";
    else
	CCAssert(0);
    
    if (!is(AST_TYPE(node)))
        error("%s type expected, not type '%s'", name, type2s(AST_TYPE(node)));
}

bool islvalue(node_t *node)
{
    if (AST_ID(node) == STRING_LITERAL)
        return true;
    if (AST_ID(node) == PAREN_EXPR)
	return islvalue(EXPR_OPERAND(node, 0));
    if (AST_ID(node) == UNARY_OPERATOR && EXPR_OP(node) == '*') {
	if (isfunc(AST_TYPE(node)))
	    return false;
	return true;
    }
    if (AST_ID(node) == MEMBER_EXPR)
	return EXPR_OP(node) == DEREF ? true : islvalue(EXPR_OPERAND(node, 0));
    if (AST_ID(node) == REF_EXPR) {
        if (EXPR_OP(node) == ENUM)
            return false;
        if (isfunc(AST_TYPE(node)))
            return false;
        return true;
    }
    
    return false;
}

static void ensure_lvalue(node_t *node)
{
    if (!islvalue(node))
        error("lvalue expect");
}

static const char * is_assignable(node_t *node)
{
    node_t *ty = AST_TYPE(node);
    if (!islvalue(node))
	return "expression is not assignable";
    if (AST_ID(node) == PAREN_EXPR)
	return is_assignable(node);
    if (isarray(ty))
	return format("array type '%s' is not assignable", type2s(ty));
    if (isconst(ty)) {
	if (AST_ID(node) == REF_EXPR) {
	    return format("read-only variable '%s' is not assignable", SYM_NAME(EXPR_SYM(node)));
	} else if (AST_ID(node) == MEMBER_EXPR) {
	    const char *op = EXPR_OP(node) == '.' ? "." : "->";
	    const char *l = SYM_NAME(EXPR_SYM(EXPR_OPERAND(node, 0)));
	    const char *r = AST_NAME(EXPR_OPERAND(node, 1));
	    return format("read-only variable '%s%s%s' is not assignable", l, op, r);
	} else {
	    return "read-only variable is not assignable";
	}
    }
    
    return NULL;
}

static void ensure_assignable(node_t *or)
{
    const char *msg = is_assignable(or);
    if (msg)
        error(msg);
}

static bool is_bitfield(node_t *node)
{
    if (AST_ID(node) != MEMBER_EXPR)
        return false;

    node_t *ty = AST_TYPE(EXPR_OPERAND(node, 0));
    const char *name = AST_NAME(EXPR_OPERAND(node, 1));
    node_t *field = find_field(ty, name);
    return isbitfield(field);
}

static const char * is_castable(node_t *dst, node_t *src)
{
    if (isvoid(dst))
	return NULL;
    if (isarith(dst) && isarith(src))
	return NULL;
    if (isint(dst) && isptr(src))
	return NULL;
    if (isptrto(dst, FUNCTION)) {
	if (isint(src) ||
	    isptrto(src, FUNCTION))
	    return NULL;
    } else if (isptr(dst)) {
	if (isint(src) ||
	    isptrto(src, VOID))
	    return NULL;
	if (isptr(src) && !isfunc(rtype(src)))
	    return NULL;
    }

    return format(INCOMPATIBLE_TYPES, type2s(src), type2s(dst));
}

static void ensure_cast(node_t *dst, node_t *src)
{
    const char *msg = is_castable(dst, src);
    if (msg)
	error(msg);
}

static void argcast1(node_t *fty, node_t **args, struct vector *v)
{
    node_t **params = TYPE_PARAMS(fty);
    int len1 = array_len((void **)params);
    int len2 = array_len((void **)args);
    bool oldstyle = TYPE_OLDSTYLE(fty);
    int cmp1;

    if (oldstyle) {
	cmp1 = MIN(len1, len2);
    } else {
	node_t *last = params[len1 - 1];
	bool vargs = unqual(SYM_TYPE(last)) == vartype;
	cmp1 = vargs ? len1 - 1 : len1;
    }
    
    for (int i = 0; i < cmp1; i++) {
	node_t *dst = SYM_TYPE(params[i]);
	node_t *src = AST_TYPE(args[i]);
	node_t *ret;
	if ((ret = init_conv(dst, args[i]))) {
	    vec_push(v, ret);
	} else {
	    if (oldstyle)
		warning(INCOMPATIBLE_TYPES, type2s(src), type2s(dst));
	    else
		error(INCOMPATIBLE_TYPES, type2s(src), type2s(dst));
	}
    }
    for (int i = cmp1; i < len2; i++)
	vec_push(v, conva(args[i]));
}

static struct vector * argscast(node_t *fty, node_t **args)
{
    struct vector *v = vec_new();
    CCAssert(isfunc(fty));

    /* There are 5 cases:
     *
     * 1. function declaration with prototype
     * 2. function definition with prototype
     * 3. function declaration with oldstyle
     * 4. function definition with oldstyle
     * 5. no function declaration/definition found
     */

    node_t **params = TYPE_PARAMS(fty);
    int len1 = array_len((void **)params);
    int len2 = array_len((void **)args);
    
    if (TYPE_OLDSTYLE(fty)) {
	if (len1 > len2)
	    warning("too few arguments to function call");

        argcast1(fty, args, v);
    } else {
	if (len1 == 0) {
	    if (len2 > 0) {
		error("too many arguments to function call, expected %d, have %d", len1, len2);
		return NULL;
	    }
	    return v;
	}
	
	node_t *last = params[len1 - 1];
	bool vargs = unqual(SYM_TYPE(last)) == vartype;
	if (vargs)
	    len1 -= 1;
	CCAssert(len1 >= 1);
	if (len1 <= len2) {
	    if (!vargs && len1 < len2) {
		error("too many arguments to function call, expected %d, have %d", len1, len2);
		return NULL;
	    }
	    SAVE_ERRORS;
	    argcast1(fty, args, v);
	    if (HAS_ERROR)
		return NULL;
	} else {
	    if (vargs)
		error("too few arguments to function call, expected at least %d, have %d", len1, len2);
	    else
		error("too few arguments to function call, expected %d, have %d", len1, len2);
	    return NULL;
	}
    }
    return v;
}

static node_t * compound_literal(node_t *ty)
{
    node_t * ret;
    node_t * inits;
    
    inits = initializer_list(ty);
    ret = expr_node(COMPOUND_LITERAL, 0, ty, inits, NULL);
    
    return ret;
}

static node_t * cast_type(void)
{
    node_t *ty;
    
    expect('(');
    ty = typename();
    expect(')');
    
    return ty;
}

static node_t * primary_expr(void)
{
    int t = token->id;
    node_t *sym;
    node_t *ret = NULL;
    
    switch (t) {
    case ID:
	sym = lookup(token->name, identifiers);
	if (sym) {
	    SYM_REFS(sym)++;
	    ret = expr_node(REF_EXPR, 0, SYM_TYPE(sym), NULL, NULL);
	    EXPR_SYM(ret) = sym;
	    if (isenum(SYM_TYPE(sym)) && SYM_SCLASS(sym) == ENUM)
		EXPR_OP(ret) = ENUM; // enum ids
	} else {
	    error("use of undeclared identifier '%s'", token->name);
	}
	expect(t);
	break;
    case ICONSTANT:
    case FCONSTANT:
    case SCONSTANT:
        {
	    int id;
            sym = lookup(token->name, constants);
            if (!sym) {
                sym = install(token->name, &constants, CONSTANT);
	        if (t == ICONSTANT)
		    integer_constant(token, sym);
		else if (t == FCONSTANT)
		    float_constant(token, sym);
		else
		    string_constant(token, sym);
            }
            expect(t);
	    if (t == ICONSTANT)
		id = INTEGER_LITERAL;
	    else if (t == FCONSTANT)
		id = FLOAT_LITERAL;
	    else
		id = STRING_LITERAL;
            ret = expr_node(id, 0, SYM_TYPE(sym), NULL, NULL);
	    EXPR_SYM(ret) = sym;
        }
	break;
    case '(':
	if (istypename(lookahead())) {
	    node_t *ty = cast_type();
	    ret = compound_literal(ty);
	} else {
	    expect('(');
	    node_t *e = expression();
	    if (e)
		ret = expr_node(PAREN_EXPR, 0, AST_TYPE(e), e, NULL);
	    expect(')');
	}
	break;
    default:
	error("invalid postfix expression at '%s'", token->name);
	break;
    }
    
    return ret;
}

static node_t ** argument_expr_list(void)
{
    node_t **args = NULL;
    
    if (firstexpr(token)) {
        struct vector *v = vec_new();
        for (;;) {
	    vec_push_safe(v, assign_expr());
	    if (token->id != ',')
		break;
	    expect(',');
        }
        args = (node_t **)vtoa(v);
    } else if (token->id != ')') {
        error("expect assignment expression");
    }
    
    return args;
}

static node_t * subscript(node_t *node)
{
    node_t *e;
    node_t *ret = NULL;
    
    expect('[');
    e = expression();
    expect(']');
    if (node == NULL || e == NULL)
	return ret;

    SAVE_ERRORS;
    if (!isarray(AST_TYPE(node)) && !isptr(AST_TYPE(node)))
	error("subscripted value is not an array or pointer");
    ensure_type(e, isint);
    if (NO_ERROR) {
	ret = bop('+', conv(node), conv(e));
	ret = uop('*', rtype(AST_TYPE(ret)), ret);
    }
    return ret;
}

static node_t * funcall(node_t *node)
{
    node_t **args;
    node_t *ret = NULL;
    
    expect('(');
    args = argument_expr_list();
    expect(')');
    if (node == NULL)
	return ret;

    if (isptrto(AST_TYPE(node), FUNCTION)) {
	node_t *fty = rtype(AST_TYPE(node));
	struct vector *v;
	if ((v = argscast(fty, args))) {
	    ret = expr_node(CALL_EXPR, 0, rtype(fty), node, NULL);
	    EXPR_ARGS(ret) = (node_t **)vtoa(v);
	}
    } else {
	ensure_type(node, isfunc);
    }
    
    return ret;
}

static node_t * direction(node_t *node)
{
    int t = token->id;
    node_t *ret = NULL;
    const char *name = NULL;

    expect(t);
    if (token->id == ID)
	name = token->name;
    expect(ID);
    if (node == NULL || name == NULL)
	return ret;
    
    SAVE_ERRORS;
    node_t *field = NULL;
    node_t *ty = AST_TYPE(node);
    if (t == '.') {
	ensure_type(node, isrecord);
    } else {
	if (!isptr(ty) || !isrecord(rtype(ty)))
	    error("pointer to struct/union type expected, not type '%s'", type2s(ty));
	else
	    ty = rtype(ty);
    }
    if (isrecord(ty)) {
	field = find_field(ty, name);
        if (field == NULL)
	    field_not_found_error(ty, name);
    }
    if (NO_ERROR) {
	node_t *r = expr_node(REF_EXPR, 0,FIELD_TYPE(field), NULL, NULL);
	ret = expr_node(MEMBER_EXPR, t, FIELD_TYPE(field), node, r);
	AST_NAME(EXPR_OPERAND(ret, 1)) = FIELD_NAME(field);
    }
    return ret;
}

static node_t * post_increment(node_t *node)
{
    int t = token->id;
    node_t *ret = NULL;
    
    expect(t);
    if (node == NULL)
	return ret;

    SAVE_ERRORS;
    ensure_type(node, isscalar);
    ensure_assignable(node);
    if (NO_ERROR)
	ret = uop(t, AST_TYPE(node), node);
    return ret;
}

static node_t * postfix_expr1(node_t *ret)
{
    for (;token->id == '[' || token->id == '(' || token->id == '.'
	     || token->id == DEREF || token->id == INCR || token->id == DECR;) {
        switch (token->id) {
	case '[':   ret = subscript(ret); break;
	case '(':   ret = funcall(conv(ret)); break;
	case '.':
	case DEREF: ret = direction(ret); break;
	case INCR:
	case DECR:  ret = post_increment(ret); break;
	default:    CCAssert(0);
        }
    }

    return ret;
}

static node_t * postfix_expr(void)
{
    node_t * expr = primary_expr();
    
    return postfix_expr1(expr);
}

static node_t * sizeof_expr(void)
{
    int t = token->id;
    node_t *ret = NULL;
    
    expect(t);
    
    struct token *ahead = lookahead();
    node_t *n = NULL;
    node_t *ty = NULL;
    
    if (token->id == '(' && istypename(ahead)) {
        ty = cast_type();
        if (token->id == '{') {
            node_t * node = compound_literal(ty);
            n = uop(t, ty, postfix_expr1(node));
        }
    } else {
        n = unary_expr();
    }
    
    ty = n ? AST_TYPE(n) : ty;
    if (ty == NULL)
	return ret;

    SAVE_ERRORS;
    if (isfunc(ty) || isvoid(ty))
        error("'sizeof' to a '%s' type is invalid", type2s(ty));
    else if (isincomplete(ty))
        error("'sizeof' to an incomplete type '%s' is invalid", type2s(ty));
    else if (n && is_bitfield(n))
	error("'sizeof' to a bitfield is invalid");

    if (NO_ERROR)
	ret = uop(t, unsignedinttype, n ? n : ty);

    return ret;
}

static node_t * pre_increment(void)
{
    int t = token->id;
    node_t *ret = NULL;
    
    expect(t);
    node_t *operand = unary_expr();
    if (operand == NULL)
	return ret;

    SAVE_ERRORS;
    ensure_type(operand, isscalar);
    ensure_assignable(operand);
    if (NO_ERROR) {
	ret = uop(t, AST_TYPE(operand), operand);
	EXPR_PREFIX(ret) = true;
    }
    
    return ret;
}

static node_t * minus_plus(void)
{
    int t = token->id;
    node_t *ret = NULL;
    
    expect(t);
    node_t *operand = cast_expr();
    if (operand == NULL)
	return ret;

    SAVE_ERRORS;
    ensure_type(operand, isarith);
    if (NO_ERROR) {
	node_t *c = conv(operand);
	ret = uop(t, AST_TYPE(c), c);
    }

    return ret;
}

static node_t * bitwise_not(void)
{
    int t = token->id;
    node_t *ret = NULL;
    
    expect(t);
    node_t *operand = cast_expr();
    if (operand == NULL)
	return ret;

    SAVE_ERRORS;
    ensure_type(operand, isint);
    if (NO_ERROR) {
	node_t *c = conv(operand);
	ret = uop(t, AST_TYPE(c), c);
    }

    return ret;
}

static node_t * logical_not(void)
{
    int t = token->id;
    node_t *ret = NULL;
    
    expect(t);
    node_t *operand = cast_expr();
    if (operand == NULL)
	return ret;

    SAVE_ERRORS;
    ensure_type(operand, isscalar);
    if (NO_ERROR)
	ret = uop(t, inttype, conv(operand));

    return ret;
}

static node_t * address(void)
{
    int t = token->id;
    node_t *ret = NULL;
    
    expect(t);
    node_t *operand = cast_expr();
    if (operand == NULL)
	return ret;

    SAVE_ERRORS;
    if (!isfunc(AST_TYPE(operand))) {
	ensure_lvalue(operand);
	if (EXPR_SYM(operand) && SYM_SCLASS(EXPR_SYM(operand)) == REGISTER)
	    error("address of register variable requested");
	else if (is_bitfield(operand))
	    error("address of bitfield requested");
    }
    if (NO_ERROR)
	ret = uop(t, ptr_type(AST_TYPE(operand)), operand);

    return ret;
}

static node_t * indirection(void)
{
    int t = token->id;
    node_t *ret = NULL;
    
    expect(t);
    node_t *operand = conv(cast_expr());
    if (operand == NULL)
	return ret;

    SAVE_ERRORS;
    ensure_type(operand, isptr);
    if (NO_ERROR)
	ret = uop(t, rtype(AST_TYPE(operand)), operand);
    
    return ret;
}

static node_t * unary_expr(void)
{
    switch (token->id) {
    case INCR:
    case DECR: 	 return pre_increment();
    case '+':
    case '-':    return minus_plus();
    case '~':    return bitwise_not();
    case '!':    return logical_not();
    case '&':    return address();
    case '*':    return indirection();
    case SIZEOF: return sizeof_expr();
    default:     return postfix_expr();
    }
}

static node_t * cast_expr(void)
{
    struct token * ahead = lookahead();
    
    if (token->id == '(' && istypename(ahead)) {
        node_t *ty = cast_type();
        if (token->id == '{') {
            node_t * node = compound_literal(ty);
            return postfix_expr1(node);
        }
	
	node_t *ret = NULL;
	node_t *cast = cast_expr();
	if (cast) {
	    SAVE_ERRORS;
	    ensure_cast(ty, AST_TYPE(cast));
	    if (NO_ERROR)
		ret = expr_node(CAST_EXPR, 0, ty, cast, NULL);
	}
        
	return ret;
    }
    return unary_expr();
}

static node_t * multiple_expr(void)
{
    node_t * mulp1;
    
    mulp1 = cast_expr();
    while (token->id == '*' || token->id == '/' || token->id == '%') {
        int t = token->id;
        expect(t);
        mulp1 = bop(t, conv(mulp1), conv(cast_expr()));
    }
    
    return mulp1;
}

static node_t * additive_expr(void)
{
    node_t * add1;
    
    add1 = multiple_expr();
    while (token->id == '+' || token->id == '-') {
        int t = token->id;
        expect(t);
        add1 = bop(t, conv(add1), conv(multiple_expr()));
    }
    
    return add1;
}

static node_t * shift_expr(void)
{
    node_t * shift1;
    
    shift1 = additive_expr();
    while (token->id == LSHIFT || token->id == RSHIFT) {
        int t = token->id;
        expect(t);
        shift1 = bop(t, conv(shift1), conv(additive_expr()));
    }
    
    return shift1;
}

static node_t * relation_expr(void)
{
    node_t * rel;
    
    rel = shift_expr();
    while (token->id == '<' || token->id == '>' || token->id == LEQ || token->id == GEQ) {
        int t = token->id;
        expect(t);
        rel = bop(t, conv(rel), conv(shift_expr()));
    }
    
    return rel;
}

static node_t * equality_expr(void)
{
    node_t * equl;
    
    equl = relation_expr();
    while (token->id == EQ || token->id == NEQ) {
        int t = token->id;
        expect(t);
        equl = bop(t, conv(equl), conv(relation_expr()));
    }
    
    return equl;
}

static node_t * and_expr(void)
{
    node_t * and1;
    
    and1 = equality_expr();
    while (token->id == '&') {
        expect('&');
        and1 = bop('&', conv(and1), conv(equality_expr()));
    }
    
    return and1;
}

static node_t * exclusive_or(void)
{
    node_t * eor;
    
    eor = and_expr();
    while (token->id == '^') {
        expect('^');
        eor = bop('^', conv(eor), conv(and_expr()));
    }
    
    return eor;
}

static node_t * inclusive_or(void)
{
    node_t * ior;
    
    ior = exclusive_or();
    while (token->id == '|') {
        expect('|');
        ior = bop('|', conv(ior), conv(exclusive_or()));
    }
    
    return ior;
}

static node_t * logic_and(void)
{
    node_t * and1;
    
    and1 = inclusive_or();
    while (token->id == AND) {
        expect(AND);
	and1 = logicop(AND, conv(and1), conv(inclusive_or()));
    }
    
    return and1;
}

static node_t * logic_or(void)
{
    node_t * or1;
    
    or1 = logic_and();
    while (token->id == OR) {
        expect(OR);
	or1 = logicop(OR, conv(or1), conv(logic_and()));
    }
    
    return or1;
}

static node_t * cond_expr1(node_t *cond)
{
    node_t *ret = NULL;
    node_t *then, *els;
    node_t *ty = NULL;

    expect('?');
    then = conv(expression());
    expect(':');
    els = conv(cond_expr());

    if (cond == NULL || then == NULL || els == NULL)
	return ret;

    node_t *ty1 = AST_TYPE(then);
    node_t *ty2 = AST_TYPE(els);

    SAVE_ERRORS;
    ensure_type(cond, isscalar);

    if (isarith(ty1) && isarith(ty2)) {
        ty = conv2(ty1, ty2);
        then = wrap(ty, then);
        els = wrap(ty, els);
    } else if ((isstruct(ty1) && isstruct(ty2)) ||
               (isunion(ty1) && isunion(ty2))) {
        if (!eqtype(ty1, ty2))
	    error(INCOMPATIBLE_TYPES2, type2s(ty1), type2s(ty2));
	ty = ty1;
    } else if (isvoid(ty1) && isvoid(ty2)) {
	ty = voidtype;
    } else if (isptr(ty1) && isptr(ty2)) {
	if (is_nullptr(then) || is_nullptr(els)) {
	    node_t *nty = is_nullptr(then) ? ty1 : ty2;
	    node_t *tty = nty == ty1 ? ty2 : ty1;
	    ty = ptr_type(compose(rtype(tty), rtype(nty)));
	    then = bitconv(ty, then);
	    els = bitconv(ty, els);
	} else if (isptrto(ty1, VOID) || isptrto(ty2, VOID)) {
	    node_t *vty = isptrto(ty1, VOID) ? ty1 : ty2;
	    node_t *tty = vty == ty1 ? ty2 : ty1;
	    if (isptrto(tty, FUNCTION)) {
	        error(INCOMPATIBLE_TYPES2, type2s(ty1), type2s(ty2));
	    } else {
		ty = ptr_type(compose(rtype(vty), rtype(tty)));
		then = bitconv(ty, then);
		els = bitconv(ty, els);
	    }
	} else {
	    node_t *rty1 = rtype(ty1);
	    node_t *rty2 = rtype(ty2);
	    if (eqtype(unqual(rty1), unqual(rty2))) {
		ty = ptr_type(compose(rty1, rty2));
		then = bitconv(ty, then);
		els = bitconv(ty, els);
	    } else {
		error(INCOMPATIBLE_TYPES2, type2s(ty1), type2s(ty2));
	    }
	}
    } else {
	error("type mismatch in conditional expression: '%s' and '%s'", type2s(ty1), type2s(ty2));
    }
    
    if (NO_ERROR) {
	ret = expr_node(COND_EXPR, 0, ty, NULL, NULL);
	EXPR_COND(ret) = cond;
	EXPR_THEN(ret) = then;
	EXPR_ELSE(ret) = els;
    }
    
    return ret;
}

static node_t * cond_expr(void)
{
    node_t * or1 = logic_or();
    if (token->id == '?')
        return cond_expr1(conv(or1));
    return or1;
}

node_t * assign_expr(void)
{
    node_t *or1 = logic_or();
    if (token->id == '?')
        return cond_expr1(conv(or1));
    if (is_assign_op(token->id)) {
        int t = token->id;
        expect(t);
	return assignop(t, or1, assign_expr());
    }
    return or1;
}

node_t * expression(void)
{
    node_t *assign1;
    
    assign1 = assign_expr();
    while (token->id == ',') {
        expect(',');
	assign1 = commaop(',', assign1, assign_expr());
    }
    
    return assign1;
}

node_t * bool_expr(void)
{
    // Conversion for expression in conditional statement
    node_t *node = expression();
    if (node == NULL)
	return NULL;
    if (islvalue(node))
	node = ltor(node);
    return decay(node);
}

node_t * switch_expr(void)
{
    node_t *node = conv(expression());
    if (node == NULL)
	return NULL;
    if (!isint(AST_TYPE(node))) {
	error("statement requires expression of integer type ('%s' invalid)",
	      type2s(AST_TYPE(node)));
	return NULL;
    }
    return node;
}

static inline node_t * expr_node(int id, int op, node_t *ty, node_t *l, node_t *r)
{
    node_t *node = ast_expr(id, op, l, r);
    AST_TYPE(node) = ty;
    return node;
}

static node_t * uop(int op, node_t *ty, node_t *l)
{
    node_t *node = ast_uop(op, ty, l);
    return node;
}

static node_t * bop(int op, node_t *l, node_t *r)
{
    node_t *node = NULL;
    node_t *ty;

    if (l == NULL || r == NULL)
	return NULL;

    SAVE_ERRORS;
    switch (op) {
    case '*': case '/':
	ensure_type(l, isarith);
	ensure_type(r, isarith);
	if (NO_ERROR) {
	    ty = conv2(AST_TYPE(l), AST_TYPE(r));
	    node = ast_bop(op, ty, wrap(ty, l), wrap(ty, r));
	}
	break;
    case '%':
    case LSHIFT: case RSHIFT:
    case '&': case '^': case '|':
	ensure_type(l, isint);
	ensure_type(r, isint);
	if (NO_ERROR) {
	    ty = conv2(AST_TYPE(l), AST_TYPE(r));
	    node = ast_bop(op, ty, wrap(ty, l), wrap(ty, r));
	}
	break;
    case '+':
	if (isptr(AST_TYPE(l))) {
	    ensure_type(r, isint);
	    if (NO_ERROR)
		node = ast_bop(op, AST_TYPE(l), l, r);
	} else if (isptr(AST_TYPE(r))) {
	    ensure_type(l, isint);
	    if (NO_ERROR)
		node = ast_bop(op, AST_TYPE(r), l, r);
	} else {
	    ensure_type(l, isarith);
	    ensure_type(r, isarith);
	    if (NO_ERROR) {
		ty = conv2(AST_TYPE(l), AST_TYPE(r));
		node = ast_bop(op, ty, wrap(ty, l), wrap(ty, r));
	    }
	}
	break;
    case '-':
	if (isptr(AST_TYPE(l))) {
	    if (isint(AST_TYPE(r)))
		node = ast_bop(op, AST_TYPE(l), l, r);
	    else if (isptr(AST_TYPE(r)))
		node = ast_bop(op, inttype, l, r);
	    else
		error("expect integer or pointer type, not type '%s'", type2s(AST_TYPE(r)));
	} else {
	    ensure_type(l, isarith);
	    ensure_type(r, isarith);
	    if (NO_ERROR) {
		ty = conv2(AST_TYPE(l), AST_TYPE(r));
		node = ast_bop(op, ty, wrap(ty, l), wrap(ty, r));
	    }
	}
	break;
    case '>': case '<': case LEQ: case GEQ:
    case EQ: case NEQ:
	ensure_type(l, isscalar);
	ensure_type(r, isscalar);
	if (NO_ERROR)
	    node = ast_bop(op, inttype, l, r);
	break;
    default:
	error("unknown op '%s'", tname(op));
	CCAssert(0);
    }
    return node;
}

static node_t * logicop(int op, node_t *l, node_t *r)
{
    node_t *ret = NULL;

    if (l == NULL || r == NULL)
	return NULL;

    SAVE_ERRORS;
    ensure_type(l, isscalar);
    ensure_type(r, isscalar);
    if (NO_ERROR)
	ret = ast_bop(op, inttype, l, r);
    
    return ret;
}

static node_t * commaop(int op, node_t *l, node_t *r)
{
    if (l == NULL || r == NULL)
	return NULL;

    if (isarray(AST_TYPE(l)) || isfunc(AST_TYPE(l)))
    	l = decay(l);
    if (isarray(AST_TYPE(r)) || isfunc(AST_TYPE(r)))
    	r = decay(r);
    if (islvalue(l))
	l = ltor(l);
    if (islvalue(r))
	r = ltor(r);
    
    return ast_bop(op, AST_TYPE(r), l, r);
}

static node_t * assignop(int op, node_t *l, node_t *r)
{
    node_t *ret = NULL;

    if (l == NULL || r == NULL)
	return NULL;

    node_t *retty = unqual(AST_TYPE(l));
    SAVE_ERRORS;
    ensure_assignable(l);
    if (HAS_ERROR)
	return NULL;
    if (op != '=') {
	// compound assignment
	int op2 = splitop(op);
	node_t *l1 = conv(l);
	node_t *r1 = conv(r);
	if (op2 == '+' || op2 == '-') {
	    node_t *ty1 = AST_TYPE(l1);
	    node_t *ty2 = AST_TYPE(r1);
	    if (!((isarith(ty1) && isarith(ty2)) ||
		  (isptr(ty1) && isint(ty2))))
	        error(INCOMPATIBLE_TYPES, type2s(ty1), type2s(ty2));
	}
	r = bop(op2, l1, r1);
    }

    if (NO_ERROR) {
	r = assignconv(retty, r);
	if (r)
	    ret = ast_bop('=', retty, l, r);
	else
	    error(INCOMPATIBLE_TYPES, type2s(AST_TYPE(l)), type2s(AST_TYPE(r)));
    }

    return ret;
}

static const char * castname(node_t *ty, node_t *l)
{
    if (isfloat(ty) && isfloat(AST_TYPE(l)))
	return FloatCast;
    else if (isfloat(ty) && isint(AST_TYPE(l)))
        return IntegerToFloatCast;
    else if (isint(ty) && isint(AST_TYPE(l)))
	return IntegralCast;
    else if (isint(ty) && isfloat(AST_TYPE(l)))
	return FloatToIntegerCast;
    else
	return BitCast;
}

static node_t * wrap(node_t *ty, node_t *node)
{
    CCAssert(isarith(ty));
    CCAssert(isarith(AST_TYPE(node)));
    
    if (eqarith(ty, AST_TYPE(node)))
        return node;
    else
        return ast_conv(ty, node, castname(ty, node));
}

static node_t * bitconv(node_t *ty, node_t *node)
{
    if (eqtype(ty, AST_TYPE(node)))
	return node;
    else
	return ast_conv(ty, node, castname(ty, node));
}

static node_t * decay(node_t *node)
{
    switch (TYPE_KIND(AST_TYPE(node))) {
    case FUNCTION:
	return ast_conv(ptr_type(AST_TYPE(node)), node, FunctionToPointerDecay);
            
    case ARRAY:
	return ast_conv(ptr_type(rtype(AST_TYPE(node))), node, ArrayToPointerDecay);

    default:
	return node;
    }
}

static node_t * ltor(node_t *node)
{
    return ast_conv(unqual(AST_TYPE(node)), node, LValueToRValue);
}

// Universal Unary Conversion
static node_t * conv(node_t *node)
{
    if (node == NULL)
	return NULL;
    if (islvalue(node))
	node = ltor(node);
    
    switch (TYPE_KIND(AST_TYPE(node))) {
    case _BOOL: case CHAR: case SHORT:
	return ast_conv(inttype, node, IntegralCast);

    case ENUM:
	return ast_conv(rtype(AST_TYPE(node)), node, IntegralCast);
            
    case FUNCTION:            
    case ARRAY:
	return decay(node);
	
    default:
	return node;
    }
}

// Default function argument conversion
static node_t * conva(node_t *node)
{
    if (node == NULL)
	return NULL;
    if (islvalue(node))
	node = ltor(node);
    
    switch (TYPE_KIND(AST_TYPE(node))) {
    case FLOAT:
	return ast_conv(doubletype, node, FloatCast);
	
    default:
	return conv(node);
    }
}

// Universal Binary Conversion
static node_t * conv2(node_t *l, node_t *r)
{
    CCAssert(isarith(l));
    CCAssert(isarith(r));
    
    CCAssert(TYPE_SIZE(l) >= TYPE_SIZE(inttype));
    CCAssert(TYPE_SIZE(r) >= TYPE_SIZE(inttype));
    
    node_t *max = TYPE_RANK(l) > TYPE_RANK(r) ? l : r;
    if (isfloat(l) || isfloat(r) || TYPE_OP(l) == TYPE_OP(r))
        return max;
    
    node_t *u = TYPE_OP(l) == UNSIGNED ? l : r;
    node_t *s = TYPE_OP(l) == INT ? l : r;
    CCAssert(unqual(s) == s);
    
    if (TYPE_RANK(u) >= TYPE_RANK(s))
        return u;
    
    if (TYPE_SIZE(u) < TYPE_SIZE(s)) {
        return s;
    } else {
        if (s == inttype)
            return unsignedinttype;
        else if (s == longtype)
            return unsignedlongtype;
        else
            return unsignedlonglongtype;
    }
    
    return l;
}

static node_t * assignconv(node_t *ty, node_t *node)
{
    node_t *ty2;

    if (islvalue(node))
	node = ltor(node);

    ty2 = AST_TYPE(node);
    
    if (isarith(ty) && isarith(ty2)) {
	return wrap(ty, node);
    } else if (ty == booltype && isptr(ty2)) {
	return ast_conv(ty, node, PointerToBoolean);
    } else if ((isstruct(ty) && isstruct(ty2)) ||
	       (isunion(ty) && isunion(ty2))) {
        if (eqtype(unqual(ty), unqual(ty2)))
	    return bitconv(ty, node);
    } else if (isptr(ty) && isptr(ty2)) {
	if (is_nullptr(node)) {
	    // always allowed
	} else if (isptrto(ty, VOID) || isptrto(ty2, VOID)) {
	    node_t *vty = isptrto(ty, VOID) ? ty : ty2;
	    node_t *tty = vty == ty ? ty2 : ty;
	    if (isptrto(tty, FUNCTION)) {
		return NULL;
	    } else {
		node_t *rty1 = rtype(ty);
		node_t *rty2 = rtype(ty2);
		if (!qual_contains(rty1, rty2))
		    return NULL;
	    }
	} else {
	    node_t *rty1 = rtype(ty);
	    node_t *rty2 = rtype(ty2);
	    if (eqtype(unqual(rty1), unqual(rty2))) {
		if (!qual_contains(rty1, rty2))
		    return NULL;
	    } else {
		return NULL;
	    }
	}
	return bitconv(ty, node);
    }
    return NULL;
}

static bool is_nullptr(node_t *node)
{
    CCAssert(isptr(AST_TYPE(node)));

    node_t *cnst = eval(node, inttype);
    if (cnst == NULL)
	return false;
    if (isiliteral(cnst))
	return SYM_VALUE_U(EXPR_SYM(cnst)) == 0;
    return false;
}

int intexpr(void)
{
    node_t *cnst = eval(cond_expr(), inttype);
    if (cnst == NULL) {
	error("expression is not a compile-time constant");
	return 0;
    }
    if (isiliteral(cnst))
	return ILITERAL_VALUE(cnst);

    error("expression is not an integer constant");
    if (isfliteral(cnst))
	return FLITERAL_VALUE(cnst);
    
    return 0;
}

node_t * init_conv(node_t *ty, node_t *node)
{
    return assignconv(ty, decay(node));
}
