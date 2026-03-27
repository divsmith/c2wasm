/* lexer.h — Token, include stack, macro, and string table types */

/* File I/O builtins (implemented in file_io.c) */
int __open_file(char *path, int path_len);
int __read_file(int fd, char *buf, int max_len);
void __close_file(int fd);

struct IncludeStack {
    char *is_src;
    int is_src_len;
    int is_lex_pos;
    int is_lex_line;
    int is_lex_col;
    int macro_idx; /* -1 for file includes, macro index for expansions */
};

struct Token {
    int kind;
    char *text;
    int int_val;
    int line;
    int col;
};

struct Macro {
    char *name;
    int value;
    int is_func_macro;
    int param_count;
    char **param_names;
    char *body;
    int expanding;
};

struct StringEntry {
    char *data;
    int len;
    int offset;
};

/* Lexer public API */
void init_includes(void);
void lex_init(void);
struct Token *next_token(void);
void init_macros(void);
int find_macro(char *name);
void init_strings(void);
int add_string(char *data, int len);
