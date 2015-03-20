/*
 * mcc
 * frontend of the c compiler
 */
#include "mcc.h"

// configs
static struct {
    unsigned E : 1;
    unsigned c : 1;
    unsigned S : 1;
} option;

static char *tmpdir;
static char *output_file;

// options
static unsigned fails;
static unsigned unit;

static const char *version = "0.0";

static void print_opt(const char *opt, const char *message)
{
    fprintf(stderr, "  %-20s%s\n", opt, message);
}

static void usage()
{
    fprintf(stderr,
            "OVERVIEW: mcc - A Standard C Compiler v%s\n\n"
            "USAGE: mcc [options] <inputs>\n\n"
            "OPTIONS:\n", version);
    print_opt("-c",              "Only run preprocess, compile, and assemble steps");
    print_opt("-E",              "Only run the preprocessor");
    print_opt("-h, --help",      "Display available options");
    print_opt("-I <dir>",        "Add directory to include search path");
    print_opt("-o <file>",       "Write output to <file>");
    print_opt("-S",              "Only run preprocess and compilation steps");
    print_opt("-v, --version",   "Display version and options");
}

static void append(struct vector *v, const char *str)
{
    struct string *s = new_string();
    str_cats(s, str);
    vec_push(v, str_to_array(s));
}

static char *tempname(const char *hint)
{
    size_t len = strlen(tmpdir) + strlen(hint) + 128;
    void *p = cc_malloc(len);
    hint = hint ? hint : "tmp";
    snprintf(p, len, "%s/mcc.%u.%s", tmpdir, unit, hint);
    return p;
}

static void translate(void *elem, void *context)
{
    char *inputfile = (char *)elem;
    struct vector *options = (struct vector *)context;
    struct vector *v;
    int argc;
    char **argv;
    static const char *cpp[] = {"cpp", "$in", "-o", "$out", 0};
    static const char *cc[] = {"cc", "$in", "-o", "$out", 0};
    char *ifile = NULL;
    char *sfile = NULL;
    
    unit++;
    
    if (!file_exists(inputfile)) {
        fprintf(stderr, "input file '%s' not exists.\n", inputfile);
        fails++;
        goto end;
    }
    
    // preprocess
    if (option.E) {
        if (output_file)
            cpp[3] = output_file;
        else
            cpp[2] = 0;
    } else {
        ifile = tempname("cpp.i");
        cpp[3] = ifile;
    }
    cpp[1] = inputfile;
    v = new_vector();
    vec_add_from_array(v, (void **)cpp);
    vec_add_from_vector(v, options);
    argc = vec_len(v);
    argv = (char **) vtoa(v);
    if (callsys(cpp[0], argv) == EXIT_FAILURE) {
        fails++;
        goto end;
    }
    if (option.E)
        goto end;
    
    // compile
    if (option.S) {
        if (output_file) {
            cc[3] = output_file;
        }
        else {
            char *new_file = replace_suffix(inputfile, "s");
            sfile = cc_malloc(strlen(new_file)+1);
            strcpy(sfile, new_file);
            free(new_file);
            cc[3] = sfile;
        }
    }
    else {
        sfile = tempname("cc.s");
        cc[3] = sfile;
    }
    cc[1] = ifile;
    v = new_vector();
    vec_add_from_array(v, (void **)cc);
    vec_add_from_vector(v, options);
    argc = vec_len(v);
    argv = (char **) vtoa(v);
    if (cc_main(argc, argv) == EXIT_FAILURE) {
        fails++;
        goto end;
    }
    
end:
    if (ifile)
        cc_free(ifile);
    if (sfile)
        cc_free(sfile);
    unit_exit();
}

int main(int argc, char **argv)
{
    int ret = EXIT_SUCCESS;
    struct vector *inputlist = new_vector();
    struct vector *optionlist = new_vector();
    
    for (int i=1; i < argc; i++) {
        char *arg = argv[i];
        if (!strcmp(arg, "-h") || !strcmp(arg, "--help") ||
            !strcmp(arg, "-v") || !strcmp(arg, "--version")) {
            usage();
            exit(EXIT_FAILURE);
        } else if (!strcmp(arg, "-o")) {
            if (++i >= argc)
                die("missing file name after '-o'");
            output_file = argv[i];
        } else if (!strcmp(arg, "-E")) {
            option.E = 1;
        } else if (!strcmp(arg, "-c")) {
            option.c = 1;
        } else if (!strcmp(arg, "-S")) {
            option.S = 1;
        } else if (arg[0] == '-') {
            append(optionlist, arg);
        } else {
            append(inputlist, arg);
        }
    }
    
    tmpdir = mk_temp_dir();
    if (!tmpdir)
        die("Can't make temporary directory.");
    
    if (vec_len(inputlist) == 0) {
        fprintf(stderr, "no input file.\n");
        ret = EXIT_FAILURE;
        goto end;
    } else if (argc == 1) {
        usage();
        ret = EXIT_FAILURE;
        goto end;
    }
    
    setlocale(LC_ALL, "");
    vec_foreach(inputlist, translate, optionlist);
    
end:
    purge_vector(inputlist);
    purge_vector(optionlist);
    rmdir(tmpdir);
    return ret;
}
