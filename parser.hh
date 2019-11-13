/*
Author: Monica Moniot
Login:  mamoniot
GitHub: mamoniot
...
*/
#ifndef PARSER__H_INCLUDE
#define PARSER__H_INCLUDE

#include "types.hh"

inline bool strcmp(char* str0, int32 size0, char* str1, int32 size1) {return size0 == size1 ? memcmp(str0, str1, size0) == 0 : 0;}
inline bool strcmp(char* str0, int32 size0, char* str1) {return size0 == strlen(str1) ? memcmp(str0, str1, size0) == 0 : 0;}

static char* SYM_FN = "&";
static char* SYM_ARG = ".";
static char* SYM_ASSIGN = ":=";
static char* SYM_APP = " ";
static char* SYM_SEMI = ";";
static char* SYM_OPEN_PAREN = "(";
static char* SYM_CLOSE_PAREN = ")";


Tokens tokenize(char* str, int32 str_size);


void push_string(char** tape, char* str, int32 size);
void push_string(char** tape, char* str);
void push_string(char** tape, char ch);

int32 push_uint(char** tape, uint32 n);
void push_uint_and_pad(char** tape, uint32 n, int32 pad);



AST* parse_all(Source source, Tokens tokens, char** error_msg);

void print_tree_basic(AST* root);
void print_tree(AST* root);

AST* reserve_node();
void destroy_tokens(Tokens tokens);


void msg_unexpected(Source source, int32 line, int32 column, int32 size, char* error_type, char* unexpected, char* expected, char** error_msg);
inline void msg_unexpected(Source source, SourcePos pos, char* error_type, char* unexpected, char* expected, char** error_msg) {msg_unexpected(source, pos.line, pos.column, pos.size, error_type, unexpected, expected, error_msg);}

#endif

#ifdef PARSER_IMPLEMENTATION
#undef PARSER_IMPLEMENTATION

static bool is_whitespace(char ch) {return ch == ' ' | ch == '\t' | ch == '\n';}
static bool is_var(char ch) {return ('a' <= ch & ch <= 'z') | ('A' <= ch & ch <= 'Z') | ('0' <= ch & ch <= '9') | ch == '_';}
static bool is_single_symbol(char ch) {return ch == '.' | ch == '&' | ch == ';';}
static bool is_symbol(char ch) {return ch == '=' | ch == ':';}

Tokens tokenize(Source* source) {
	source->lines = 0;
	Tokens tokens = {};
	TokenState state = TOKEN_BASE;
	char* str = source->text;
	int32 str_size = source->size;
	int32 i = 0;
	int32 cur_line = 1;
	int32 cur_column = 1;
	tape_push(&source->lines, -1);
	tape_push(&source->lines, 0);
	while(i < str_size) {
		char ch = str[i];
		if(state == TOKEN_BASE) {
			if(ch == 0) {
				state = TOKEN_NULL;
			} else if(is_var(ch)) {
				state = TOKEN_NAME;
			} else if(is_single_symbol(ch)) {
				state = TOKEN_SINGLE_SYM;
			} else if(is_symbol(ch)) {
				state = TOKEN_SYM;
			} else if(ch == '(' | ch == ')') {
				state = TOKEN_PAREN;
			}
			if(state != TOKEN_BASE) {
				tape_push(&tokens.strings, &str[i]);
				tape_push(&tokens.sizes, 1);
				tape_push(&tokens.states, state);
				tape_push(&tokens.lines, cur_line);
				tape_push(&tokens.columns, cur_column);
			}
		} else if(state == TOKEN_NAME) {
			int32 token_i = tape_size(&tokens.sizes) - 1;
			if(is_var(ch)) {
				tokens.sizes[token_i] += 1;
			} else {
				state = TOKEN_BASE;
				continue;
			}
		} else if(state == TOKEN_SYM) {
			int32 token_i = tape_size(&tokens.sizes) - 1;
			if(is_symbol(ch)) {
				tokens.sizes[token_i] += 1;
			} else {
				state = TOKEN_BASE;
				continue;
			}
		} else {
			state = TOKEN_BASE;
			continue;
		}

		i += 1;
		if(ch == '\n') {
			cur_line += 1;
			cur_column = 0;
			tape_push(&source->lines, i);
		}
		cur_column += 1;
	}
	tape_push(&tokens.strings, &str[str_size]);
	tape_push(&tokens.sizes, 0);
	tape_push(&tokens.states, TOKEN_EOF);
	tape_push(&tokens.lines, cur_line);
	tape_push(&tokens.columns, cur_column);
	return tokens;
}


void destroy_tokens(Tokens tokens) {
	tape_destroy(&tokens.states);
	tape_destroy(&tokens.strings);
	tape_destroy(&tokens.sizes);
	tape_destroy(&tokens.lines);
	tape_destroy(&tokens.columns);
}


void push_string(char** tape, char* str, int32 size) {
	memcpy(tape_reserve(tape, size), str, size);
}
void push_string(char** tape, char* str) {
	push_string(tape, str, strlen(str));
}
void push_string(char** tape, char ch) {
	tape_push(tape, ch);
}

int32 push_uint(char** tape, uint32 n) {
	char ch = (n%10) + '0';
	int32 d = 1;
	if(n > 9) d += push_uint(tape, n/10);
	tape_push(tape, ch);
	return d;
}
void push_uint_and_pad(char** tape, uint32 n, int32 pad) {
	pad -= push_uint(tape, n);
	for_each_lt(i, pad) tape_push(tape, ' ');
}

void msg_unexpected(Source source, int32 line, int32 column, int32 size, char* error_type, char* unexpected, char* expected, char** error_msg) {
	if(!error_msg) return;
	char* text = source.text;
	if(error_type) {
		push_string(error_msg, error_type);
		push_string(error_msg, "; ");
	}
	push_string(error_msg, "Unexpected ");
	push_string(error_msg, unexpected);
	push_string(error_msg, " at line ");
	push_uint(error_msg, line);
	push_string(error_msg, ", column ");
	push_uint(error_msg, column);
	if(expected) {
		push_string(error_msg, ". Expected ");
		push_string(error_msg, expected);
	}
	push_string(error_msg, ".\n");
	//source display
	int32 total_lines = tape_size(&source.lines);
	int32 saw = source.lines[line] + column;
	int32 upper = 0;
	int32 upper_size = 0;
	int32 middle = source.lines[line];
	int32 middle_size = 0;
	int32 lower = 0;
	int32 lower_size = 0;
	if(line - 1 > 0) {
		upper = source.lines[line - 1];
		upper_size = source.lines[line] - upper - 1;
	}
	if(line + 1 < total_lines) {
		lower = source.lines[line + 1];
		if(line + 2 < total_lines) {
			lower_size = source.lines[line + 2] - lower;
		} else {
			lower_size = source.size - lower;
		}
		middle_size = lower - middle - 1;
	} else {
		middle_size = source.size - middle;
	}
	{
		int32 padding = max(cast(int32, log10(line + 1)) + 2, 4);
		if(line <= 2) {
			push_string(error_msg, '\n');
		} else {
			for_each_lt(i, padding - 1) push_string(error_msg, ' ');
			push_string(error_msg, "...\n");
		}
		if(upper) {
			push_uint_and_pad(error_msg, line - 1, padding);
			push_string(error_msg, text + upper, upper_size);
			push_string(error_msg, '\n');
		}
		push_uint_and_pad(error_msg, line, padding);
		push_string(error_msg, text + middle, middle_size);
		push_string(error_msg, '\n');
		for_each_lt(i, column - 1 + padding) {
			push_string(error_msg, '~');
		}
		push_string(error_msg, '^');
		for_each_lt(i, size - 1) {
			push_string(error_msg, '^');
		}
		push_string(error_msg, '\n');
		if(lower) {
			push_uint_and_pad(error_msg, line + 1, padding);
			push_string(error_msg, text + lower, lower_size);
			push_string(error_msg, '\n');
		}
		for_each_lt(i, padding - 1) push_string(error_msg, ' ');
		push_string(error_msg, "...\n");
	}
}
static void msg_unexpected(Source source, Tokens tokens, int32 token_i, char* expected, char** error_msg) {
	TokenState state = tokens.states[token_i];
	char* unexpected;
	if(state == TOKEN_NULL) {
		unexpected = "null character";
	} else if(state == TOKEN_EOF) {
		unexpected = "end of file";
	} else if(state == TOKEN_SINGLE_SYM || state == TOKEN_SYM) {
		unexpected = "symbol";
	} else if(state == TOKEN_PAREN) {
		unexpected = "parathesis";
	} else {
		// ASSERT(0);
		unexpected = "token";
	}
	msg_unexpected(source, tokens.lines[token_i], tokens.columns[token_i], tokens.sizes[token_i], "Syntax Error", unexpected, expected, error_msg);
}


AST* reserve_node() {
	return talloc(AST, 1);
}
static Uid get_var_uid(VarLexer* lexer, char* var, int32 var_size) {
	if(strcmp(var, var_size, "true")) {
		return -1|(~NUM_UID_MASK);
	} else if(strcmp(var, var_size, "false")) {
		return 0|(~NUM_UID_MASK);
	}
	bool success = 1;
	Uid n = 0;
	for_each_index(i, ch, var, var_size) {
		if('0' <= *ch & *ch <= '9') {
			n *= 10;
			n += *ch - '0';
		} else {
			success = 0;
			break;
		}
	}
	if(success) return n|(~NUM_UID_MASK);

	Uid uid = -1;
	int32 stack_size = tape_size(&lexer->strings);
	for_each_index_bw(i, str, lexer->strings, stack_size) {
		if(strcmp(var, var_size, *str, lexer->sizes[i])) {
			uid = i;
			break;
		}
	}
	if(uid == -1) {
		uid = stack_size;
		tape_push(&lexer->strings, var);
		tape_push(&lexer->sizes, var_size);
	}
	return uid|(~VAR_UID_MASK);
}
static void get_var_from_uid(VarLexer* lexer, Uid uid, char** ret_str, int32* ret_str_size) {
	ASSERT(uid&(~VAR_UID_MASK));
	*ret_str = lexer->strings[uid&VAR_UID_MASK];
	*ret_str_size = lexer->sizes[uid&VAR_UID_MASK];
}
static AST* reserve_var(VarLexer* lexer, char* str, int32 size) {
	AST* var = reserve_node();
	var->part = PART_VAR;
	var->var_uid = get_var_uid(lexer, str, size);
	return var;
}

void free_node(AST* node) {
	if(node->part == PART_APP || node->part == PART_ASSIGN) {
		free_node(node->app.left);
		free_node(node->app.right);
	} else if(node->part == PART_FN) {
		free_node(node->fn.body);
	}
	free(node);
}


static bool eat_token(Tokens tokens, int32* token_i, TokenState state) {
	TokenState cur_state = tokens.states[*token_i];
	if(cur_state == state) {
		*token_i += 1;
		return 1;
	} else {
		return 0;
	}
}
static bool eat_token(Tokens tokens, int32* token_i, TokenState state, char* str) {
	TokenState cur_state = tokens.states[*token_i];
	char* cur_token = tokens.strings[*token_i];
	int32 cur_token_size = tokens.sizes[*token_i];
	if(cur_state == state && strcmp(cur_token, cur_token_size, str)) {
		*token_i += 1;
		return 1;
	} else {
		return 0;
	}
}


#define DECL_PARSER(name) AST* name(Source source, Tokens tokens, int32* token_i, VarLexer* lexer, char** error_msg)
typedef DECL_PARSER(ParseFunc);

static DECL_PARSER(parse_app);

static DECL_PARSER(parse_term) {
	int32 pre_token_i = *token_i;
	if(eat_token(tokens, token_i, TOKEN_NAME)) {
		return reserve_var(lexer, tokens.strings[pre_token_i], tokens.sizes[pre_token_i]);
	} else if(eat_token(tokens, token_i, TOKEN_SINGLE_SYM, SYM_FN)) {
		int32 arg_i = *token_i;
		if(eat_token(tokens, token_i, TOKEN_NAME)) {
			if(eat_token(tokens, token_i, TOKEN_SINGLE_SYM, SYM_ARG)) {
				AST* body = parse_app(source, tokens, token_i, lexer, error_msg);
				if(!body) return 0;
				AST* node = reserve_node();
				node->part = PART_FN;
				node->fn.arg_uid = get_var_uid(lexer, tokens.strings[arg_i], tokens.sizes[arg_i]);
				node->fn.body = body;
				return node;
			} else {
				msg_unexpected(source, tokens, *token_i, "\".\"", error_msg);
				return 0;
			}
		} else {
			msg_unexpected(source, tokens, *token_i, "an argument identifier", error_msg);
			return 0;
		}
	} else if(eat_token(tokens, token_i, TOKEN_PAREN, SYM_OPEN_PAREN)) {
		AST* node = parse_app(source, tokens, token_i, lexer, error_msg);
		if(!node) return 0;
		if(eat_token(tokens, token_i, TOKEN_PAREN, SYM_CLOSE_PAREN)) {
			return node;
		} else {
			free_node(node);
			msg_unexpected(source, tokens, *token_i, "\")\"", error_msg);
			return 0;
		}
	} else {
		msg_unexpected(source, tokens, *token_i, "literal or variable", error_msg);
		return 0;
	}
}

static AST* parse_app_loop_(AST* left_node, Source source, Tokens tokens, int32* token_i, VarLexer* lexer, char** error_msg) {
	int32 pre_token_i = *token_i;
	//non-deterministically check next token set
	int32 pre_lexer_size = tape_size(&lexer->strings);
	AST* right_node = parse_term(source, tokens, token_i, lexer, 0);
	if(right_node) {
		AST* new_node = reserve_node();
		new_node->part = PART_APP;
		new_node->app.left = left_node;
		new_node->app.right = right_node;
		return parse_app_loop_(new_node, source, tokens, token_i, lexer, error_msg);
	} else {
		for_each_in_range(i, pre_lexer_size, tape_size(&lexer->strings) - 1) {
			tape_pop(&lexer->strings);
			tape_pop(&lexer->sizes);
		}
		*token_i = pre_token_i;
		return left_node;
	}
}
static DECL_PARSER(parse_app) {
	AST* node = parse_term(source, tokens, token_i, lexer, error_msg);
	if(!node) return 0;
	return parse_app_loop_(node, source, tokens, token_i, lexer, error_msg);
}
static DECL_PARSER(parse_assign) {
	//TODO: non-deternimistic name evaluation
	int32 pre_token_i = *token_i;
	if(eat_token(tokens, token_i, TOKEN_NAME)) {
		if(eat_token(tokens, token_i, TOKEN_SYM, SYM_ASSIGN)) {
			AST* term = parse_app(source, tokens, token_i, lexer, error_msg);
			if(!term) return 0;
			if(eat_token(tokens, token_i, TOKEN_SINGLE_SYM, SYM_SEMI)) {
				AST* root = parse_assign(source, tokens, token_i, lexer, error_msg);
				if(!root) {free_node(term); return 0;}
				AST* new_node = reserve_node();
				AST* sub_node = reserve_node();
				new_node->part = PART_ASSIGN;
				new_node->app.left = sub_node;
				new_node->app.right = term;
				sub_node->part = PART_FN;
				sub_node->fn.arg_uid = get_var_uid(lexer, tokens.strings[pre_token_i], tokens.sizes[pre_token_i]);
				sub_node->fn.body = root;
				return new_node;
			} else {
				free_node(term);
				msg_unexpected(source, tokens, *token_i, "literal or variable", error_msg);
				return 0;
			}
		} else {
			*token_i = pre_token_i;
		}
	}
	return parse_app(source, tokens, token_i, lexer, error_msg);
}


AST* parse_all(Source source, Tokens tokens, VarLexer* lexer, char** error_msg) {
	int32 token_i = 0;
	memzero(lexer, 1);
	AST* root = parse_assign(source, tokens, &token_i, lexer, error_msg);
	if(root) {
		if(tokens.states[token_i] == TOKEN_EOF) {
			return root;
		} else {
			msg_unexpected(source, tokens, token_i, "end of file", error_msg);
			return 0;
		}
	}
	return root;
}



int32 push_uint_alpha(char** tape, uint64 n) {
	constexpr int r = 'z' - 'a' + 1;
	char ch = (n%r) + 'a';
	int32 d = 1;
	if(n >= r) d += push_uint_alpha(tape, n/r - 1);
	tape_push(tape, ch);
	return d;
}
void push_var(char** msg, VarLexer* vars, Uid uid) {
	if(uid&(~NUM_UID_MASK)) {
		int32 num = uid&NUM_UID_MASK;
		if(num >= 0) {
			push_uint(msg, num);
		} else {
			push_string(msg, "true");
		}
	} else if(uid&(~VAR_UID_MASK)) {
		char* str;
		int32 size;
		get_var_from_uid(vars, uid, &str, &size);
		push_string(msg, str, size);
	} else {
		push_uint_alpha(msg, uid);
		tape_push(msg, '\'');
	}
}
void push_term(char** msg, AST* root, VarLexer* vars, int32 parent_type = 0) {
	if(root->part == PART_VAR) {
		push_var(msg, vars, root->var_uid);
	} else if(root->part == PART_FN) {
		if(parent_type != 0) push_string(msg, '(');
		push_string(msg, '&');
		push_var(msg, vars, root->fn.arg_uid);
		push_string(msg, '.');
		push_term(msg, root->fn.body, vars, 0);
		if(parent_type != 0) push_string(msg, ')');
	} else if(root->part == PART_APP) {
		if(parent_type == 3) push_string(msg, '(');
		push_term(msg, root->app.left, vars, 2);
		push_string(msg, SYM_APP);
		push_term(msg, root->app.right, vars, 3);
		if(parent_type == 3) push_string(msg, ')');
	} else if(root->part == PART_ASSIGN) {
		// if(parent_type == 3) push_string(msg, '(');
		push_var(msg, vars, root->app.left->fn.arg_uid);
		push_string(msg, ' ');
		push_string(msg, SYM_ASSIGN);
		push_string(msg, ' ');
		push_term(msg, root->app.right, vars, 2);
		push_string(msg, ";\n");
		push_term(msg, root->app.left->fn.body, vars, 2);
		// push_term(msg, root->app.right, 3);
		// if(parent_type == 3) push_string(msg, ')');
	} else ASSERT(0);
}
void print_term(AST* root, VarLexer* vars) {
	char* msg = 0;
	push_term(&msg, root, vars);
	push_string(&msg, '\0');
	printf("%s;\n", msg);
	tape_destroy(&msg);
}

#endif
