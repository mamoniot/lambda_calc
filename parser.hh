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



TermPos parse_all(Source source, Tokens tokens, char** error_msg);

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


static int64 get_var_uid(VarSyntaxStack* var_stack, char* varstr, int32 varstr_size) {
	if(strcmp(varstr, varstr_size, "true")) {
		return -1|(~UID_NUM_MASK);
	} else if(strcmp(varstr, varstr_size, "false")) {
		return 0|(~UID_NUM_MASK);
	}
	bool success = 1;
	int32 n = 0;
	for_each_index(i, ch, varstr, varstr_size) {
		if('0' <= *ch & *ch <= '9') {
			n *= 10;
			n += *ch - '0';
		} else {
			success = 0;
			break;
		}
	}
	if(success) return n|(~UID_NUM_MASK);

	int64 uid = -1;
	int32 stack_size = tape_size(&var_stack->strings);
	for_each_index_bw(i, str, var_stack->strings, stack_size) {
		if(strcmp(varstr, varstr_size, *str, var_stack->sizes[i])) {
			uid = i;
			break;
		}
	}
	if(uid == -1) {
		uid = stack_size;
		tape_push(&var_stack->strings, varstr);
		tape_push(&var_stack->sizes, varstr_size);
	}
	return uid|(~UID_VAR_MASK);
}
static void get_var_from_uid(VarSyntaxStack* var_stack, int64 uid, char** ret_str, int32* ret_str_size) {
	ASSERT(uid&(~UID_VAR_MASK));
	*ret_str = var_stack->strings[uid&UID_VAR_MASK];
	*ret_str_size = var_stack->sizes[uid&UID_VAR_MASK];
}
static void write_var(TermPos* pos, VarSyntaxStack* var_stack, char* varstr, int32 varstr_size) {
	write_term(pos, PART_VAR, get_var_uid(var_stack, varstr, varstr_size));
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



#define DECL_PARSER(name) int64 name(TermPos* dest, Source source, Tokens tokens, int32* token_i, VarSyntaxStack* var_stack, char** error_msg)
typedef DECL_PARSER(ParseFunc);


static DECL_PARSER(parse_app);

static DECL_PARSER(parse_term) {
	int32 pre_token_i = *token_i;
	if(eat_token(tokens, token_i, TOKEN_NAME)) {
		write_var(dest, var_stack, tokens.strings[pre_token_i], tokens.sizes[pre_token_i]);
		return 1;
	} else if(eat_token(tokens, token_i, TOKEN_SINGLE_SYM, SYM_FN)) {
		int32 arg_i = *token_i;
		if(eat_token(tokens, token_i, TOKEN_NAME)) {
			if(eat_token(tokens, token_i, TOKEN_SINGLE_SYM, SYM_ARG)) {
				write_term(dest, PART_FN, get_var_uid(var_stack, tokens.strings[arg_i], tokens.sizes[arg_i]));
				int64 body_size = parse_app(dest, source, tokens, token_i, var_stack, error_msg);
				if(body_size) {
					return body_size + 1;
				} else {
					pop_term(dest);
					return 0;
				}
			} else {
				msg_unexpected(source, tokens, *token_i, "\".\"", error_msg);
				return 0;
			}
		} else {
			msg_unexpected(source, tokens, *token_i, "an argument identifier", error_msg);
			return 0;
		}
	} else if(eat_token(tokens, token_i, TOKEN_PAREN, SYM_OPEN_PAREN)) {
		int64 paren_size = parse_app(dest, source, tokens, token_i, var_stack, error_msg);
		if(!paren_size) return 0;
		if(eat_token(tokens, token_i, TOKEN_PAREN, SYM_CLOSE_PAREN)) {
			return paren_size;
		} else {
			pop_term(dest, paren_size);
			msg_unexpected(source, tokens, *token_i, "\")\"", error_msg);
			return 0;
		}
	} else {
		msg_unexpected(source, tokens, *token_i, "literal or variable", error_msg);
		return 0;
	}
}

static void parse_app_loop_(TermPos* app_dest, int64 left_size, TermPos* dest, Source source, Tokens tokens, int32* token_i, VarSyntaxStack* var_stack, char** error_msg) {
	int32 pre_token_i = *token_i;
	//non-deterministically check next token set
	// TermPos pos = reserve_term(dest);
	int64 right_size = parse_term(app_dest, source, tokens, token_i, var_stack, 0);
	if(right_size) {
		parse_app_loop_(app_dest, 1 + left_size + right_size, dest, source, tokens, token_i, var_stack, error_msg);
		write_term(dest, PART_APP, left_size);
	} else {
		*token_i = pre_token_i;
	}
}
static DECL_PARSER(parse_app) {
	int64 pre_count = dest->count;
	TermPos app_dest = create_term_list();
	TermPos app_start = app_dest;

	// TermPos pos = reserve_term(dest);
	int64 left_size = parse_term(&app_dest, source, tokens, token_i, var_stack, error_msg);
	if(!left_size) return 0;
	parse_app_loop_(&app_dest, left_size, dest, source, tokens, token_i, var_stack, error_msg);
	copy_term(dest, &app_start, app_dest.count);
	return dest->count - pre_count;
}
static DECL_PARSER(parse_assign) {
	int32 pre_token_i = *token_i;
	if(eat_token(tokens, token_i, TOKEN_NAME)) {
		if(eat_token(tokens, token_i, TOKEN_SYM, SYM_ASSIGN)) {
			TermPos pos = reserve_term(dest);

			write_term(dest, PART_FN, get_var_uid(var_stack, tokens.strings[pre_token_i], tokens.sizes[pre_token_i]));

			TermPos assign_dest = create_term_list();
			TermPos assign_start = assign_dest;
			TermPos root_dest = create_term_list();
			TermPos root_start = root_dest;

			int64 assign_size = parse_app(&assign_dest, source, tokens, token_i, var_stack, error_msg);
			if(!assign_size) {
				destroy_term_list(assign_dest);
				destroy_term_list(root_dest);
				return 0;
			}
			if(eat_token(tokens, token_i, TOKEN_SINGLE_SYM, SYM_SEMI)) {
				int64 root_size = parse_assign(&root_dest, source, tokens, token_i, var_stack, error_msg);
				if(root_size) {
					copy_term(dest, &root_start, root_size);
					copy_term(dest, &assign_start, assign_size);
					write_term(pos, PART_ASSIGN, root_size + 1);
					destroy_term_list(assign_dest);
					destroy_term_list(root_dest);
					return 2 + assign_size + root_size;
				} else {
					destroy_term_list(assign_dest);
					destroy_term_list(root_dest);
					return 0;
				}
			} else {
				destroy_term_list(assign_dest);
				destroy_term_list(root_dest);
				msg_unexpected(source, tokens, *token_i, "\";\"", error_msg);
				return 0;
			}
		} else {
			*token_i = pre_token_i;
		}
	}
	return parse_app(dest, source, tokens, token_i, var_stack, error_msg);
}


bool parse_all(TermPos* dest, Source source, Tokens tokens, VarSyntaxStack* vars, char** error_msg) {
	int32 token_i = 0;
	memzero(vars, 1);
	int64 size = parse_assign(dest, source, tokens, &token_i, vars, error_msg);
	if(size && tokens.states[token_i] != TOKEN_EOF) {
		pop_term(dest, size);
		msg_unexpected(source, tokens, token_i, "end of file", error_msg);
		return 0;
	}
	ASSERT(size == dest->count);
	return 1;
}


int32 push_uint_alpha(char** tape, uint64 n) {
	constexpr int r = 'z' - 'a' + 1;
	char ch = (n%r) + 'a';
	int32 d = 1;
	if(n >= r) d += push_uint_alpha(tape, n/r - 1);
	tape_push(tape, ch);
	return d;
}
void push_var(char** msg, VarSyntaxStack* vars, int64 uid) {
	if(uid&(~UID_NUM_MASK)) {
		int32 num = uid&UID_NUM_MASK;
		if(num >= 0) {
			push_uint(msg, num);
		} else {
			push_string(msg, "true");
		}
	} else if(uid&(~UID_VAR_MASK)) {
		char* str;
		int32 size;
		get_var_from_uid(vars, uid, &str, &size);
		push_string(msg, str, size);
	} else {
		push_uint_alpha(msg, uid);
		tape_push(msg, '\'');
	}
}
void push_term(char** msg, TermPos* source, VarSyntaxStack* vars, int32 parent_type = 0) {
	auto part = get_term_part(*source);
	if(part == PART_VAR) {
		auto uid = get_term_info(*source);
		incr_term(source);
		push_var(msg, vars, uid);
	} else if(part == PART_FN) {
		auto uid = get_term_info(*source);
		incr_term(source);
		if(parent_type != 0) push_string(msg, '(');
		push_string(msg, '&');
		push_var(msg, vars, uid);
		push_string(msg, '.');
		push_term(msg, source, vars, 0);
		if(parent_type != 0) push_string(msg, ')');
	} else if(part == PART_APP) {
		incr_term(source);
		if(parent_type == 1) push_string(msg, '(');
		push_term(msg, source, vars, 2);
		push_string(msg, SYM_APP);
		push_term(msg, source, vars, 1);
		if(parent_type == 1) push_string(msg, ')');
	} else if(part == PART_NULL) {
		incr_term(source);
		push_term(msg, source, vars, parent_type);
	} else if(part == PART_ASSIGN) {//fix assign
		auto uid = get_term_info(*source);
		incr_term(source);
		push_var(msg, vars, uid);
		push_string(msg, ' ');
		push_string(msg, SYM_ASSIGN);
		push_string(msg, ' ');
		char* temp = 0;
		push_term(&temp, source, vars, 0);
		push_term(msg, source, vars, 0);
		push_string(msg, ";\n");
		memcopy(tape_reserve(msg, tape_size(temp)), temp, tape_size(temp));
		tape_destroy(&temp);
	} else ASSERT(0);
}
void print_term(TermPos* source, VarSyntaxStack* vars) {
	char* msg = 0;
	push_term(&msg, source, vars);
	push_string(&msg, '\0');
	printf("%s;\n", msg);
	tape_destroy(&msg);
}

#endif
