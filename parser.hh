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



#define DECL_PARSER(name) AST* name(Source source, Tokens tokens, int32* token_i, NodeList* node_list, char** error_msg)
typedef DECL_PARSER(ParseFunc);

AST* parse_all(Source source, Tokens tokens, char** error_msg);

void print_tree_basic(AST* root);
void print_tree(AST* root);

AST* reserve_node(NodeList* node_list);
// void destroy_tree(NodeList* node_list);
void destroy_tokens(Tokens tokens);


void msg_unexpected(Source source, int32 line, int32 column, int32 size, char* error_type, char* unexpected, char* expected, char** error_msg);
inline void msg_unexpected(Source source, SourcePos pos, char* error_type, char* unexpected, char* expected, char** error_msg) {msg_unexpected(source, pos.line, pos.column, pos.size, error_type, unexpected, expected, error_msg);}

#endif

#ifdef PARSER_IMPLEMENTATION
#undef PARSER_IMPLEMENTATION

static int32 min(int32 a, int32 b) {return a < b ? a : b;}
static int32 max(int32 a, int32 b) {return a > b ? a : b;}

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


AST* reserve_node(NodeList* node_list) {
	node_list->size += 1;
	// ASTMem* ret = node_list->free_head;
	// if(ret) {
	// 	node_list->free_head = &ret->next;
	// 	memzero(ret, 1);
	// } else {
	// 	if(node_list->head->size >= NODE_BUFFER_SIZE) {
	// 		NodeBuffer* buffer = talloc(NodeBuffer, 1);
	// 		memzero(buffer, 1);
	// 		buffer->next = node_list->head;
	// 		node_list->head = buffer;
	// 	}
	// 	ret = &node_list->head->nodes[node_list->head->size];
	// 	node_list->head->size += 1;
	// }
	// return ret.node;
	return talloc(AST, 1);
}
static AST* reserve_var(NodeList* node_list, char* str, int32 size) {
	AST* var = reserve_node(node_list);
	var->part = PART_VAR;
	var->var.str = str;
	var->var.size = size;
	return var;
}

void free_node(NodeList* node_list, AST* node) {
	node_list->size -= 1;
	// ASTMem* node = cast(ASTMem*, node_ptr);
	// node->next = node_list->free_head;
	// node_list->free_head = node;
	if(node->part == PART_APP || node->part == PART_FN) {
		free_node(node_list, node->children[0]);
		free_node(node_list, node->children[1]);
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



static DECL_PARSER(parse_app);

static DECL_PARSER(parse_term) {
	int32 pre_token_i = *token_i;
	if(eat_token(tokens, token_i, TOKEN_NAME)) {
		return reserve_var(node_list, tokens.strings[pre_token_i], tokens.sizes[pre_token_i]);
	} else if(eat_token(tokens, token_i, TOKEN_SINGLE_SYM, SYM_FN)) {
		int32 arg_i = *token_i;
		if(eat_token(tokens, token_i, TOKEN_NAME)) {
			if(eat_token(tokens, token_i, TOKEN_SINGLE_SYM, SYM_ARG)) {
				AST* body = parse_app(source, tokens, token_i, node_list, error_msg);
				if(!body) return 0;
				AST* node = reserve_node(node_list);
				node->part = PART_FN;
				node->fn.arg = reserve_var(node_list, tokens.strings[arg_i], tokens.sizes[arg_i]);
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
		AST* node = parse_app(source, tokens, token_i, node_list, error_msg);
		if(!node) return 0;
		if(eat_token(tokens, token_i, TOKEN_PAREN, SYM_CLOSE_PAREN)) {
			return node;
		} else {
			free_node(node_list, node);
			msg_unexpected(source, tokens, *token_i, "\")\"", error_msg);
			return 0;
		}
	} else {
		msg_unexpected(source, tokens, *token_i, "literal or variable", error_msg);
		return 0;
	}
}

static AST* parse_app_loop_(AST* left_node, Source source, Tokens tokens, int32* token_i, NodeList* node_list, char** error_msg) {
	int32 pre_token_i = *token_i;
	//non-deterministically check next token set
	int32 pre_node_list_size = node_list->size;
	AST* right_node = parse_term(source, tokens, token_i, node_list, 0);
	if(right_node) {
		AST* new_node = reserve_node(node_list);
		new_node->part = PART_APP;
		new_node->app.left = left_node;
		new_node->app.right = right_node;
		return parse_app_loop_(new_node, source, tokens, token_i, node_list, error_msg);
	} else {
		*token_i = pre_token_i;
		return left_node;
	}
}
static DECL_PARSER(parse_app) {
	AST* node = parse_term(source, tokens, token_i, node_list, error_msg);
	if(!node) return 0;
	return parse_app_loop_(node, source, tokens, token_i, node_list, error_msg);
}
static DECL_PARSER(parse_assign) {
	//TODO: non-deternimistic name evaluation
	int32 pre_token_i = *token_i;
	if(eat_token(tokens, token_i, TOKEN_NAME)) {
		if(eat_token(tokens, token_i, TOKEN_SYM, SYM_ASSIGN)) {
			AST* term = parse_app(source, tokens, token_i, node_list, error_msg);
			if(!term) return 0;
			if(eat_token(tokens, token_i, TOKEN_SINGLE_SYM, SYM_SEMI)) {
				AST* root = parse_assign(source, tokens, token_i, node_list, error_msg);
				if(!root) {free_node(node_list, term); return 0;}
				AST* new_node = reserve_node(node_list);
				AST* sub_node = reserve_node(node_list);
				new_node->part = PART_ASSIGN;
				new_node->app.left = sub_node;
				new_node->app.right = term;
				sub_node->part = PART_FN;
				sub_node->fn.arg = reserve_var(node_list, tokens.strings[pre_token_i], tokens.sizes[pre_token_i]);
				sub_node->fn.body = root;
				return new_node;
			} else {
				free_node(node_list, term);
				msg_unexpected(source, tokens, *token_i, "literal or variable", error_msg);
				return 0;
			}
		} else {
			*token_i = pre_token_i;
		}
	}
	return parse_app(source, tokens, token_i, node_list, error_msg);
}


AST* parse_all(Source source, Tokens tokens, char** error_msg, NodeList* tree_mem) {
	int32 token_i = 0;
	memzero(tree_mem, 1);
	// tree_mem->head = talloc(NodeBuffer, 1);
	// memzero(tree_mem->head, 1);
	AST* root = parse_assign(source, tokens, &token_i, tree_mem, error_msg);
	if(root) {
		if(tokens.states[token_i] == TOKEN_EOF) {
			return root;
		} else {
			msg_unexpected(source, tokens, token_i, "end of file", error_msg);
			return 0;
		}
	}
	return root;
	//NOTE: Nothing is freed!
}


// void destroy_tree(NodeList* tree_mem) {
// 	NodeBuffer* head = tree_mem->head;
// 	while(head) {
// 		NodeBuffer* cur = head;
// 		head = head->next;
// 		free(cur);
// 	}
// }
void destroy_tokens(Tokens tokens) {
	tape_destroy(&tokens.states);
	tape_destroy(&tokens.strings);
	tape_destroy(&tokens.sizes);
	tape_destroy(&tokens.lines);
	tape_destroy(&tokens.columns);
}


int32 push_uint_alpha(char** tape, uint64 n) {
	constexpr int r = 'z' - 'a' + 1;
	char ch = (n%r) + 'a';
	int32 d = 1;
	if(n >= r) d += push_uint_alpha(tape, n/r - 1);
	tape_push(tape, ch);
	return d;
}
void print_tree_basic_text(char** msg, AST* root, int32 parent_type) {
	if(root->part == PART_VAR) {
		if(root->var.size > 0) {
			push_string(msg, root->var.str, root->var.size);
		} else if(root->var.size == -1) {
			if(root->var.uid == -1) {
				push_string(msg, "true");
			} else {
				push_uint(msg, root->var.uid);
			}
		} else {
			push_uint_alpha(msg, root->var.uid);
			tape_push(msg, '\'');
		}
	} else if(root->part == PART_FN) {
		if(parent_type != 1) push_string(msg, '(');
		push_string(msg, '&');
		print_tree_basic_text(msg, root->fn.arg, 1);
		push_string(msg, '.');
		print_tree_basic_text(msg, root->fn.body, 1);
		if(parent_type != 1) push_string(msg, ')');
	} else if(root->part == PART_APP) {
		if(parent_type == 3) push_string(msg, '(');
		print_tree_basic_text(msg, root->app.left, 2);
		push_string(msg, SYM_APP);
		print_tree_basic_text(msg, root->app.right, 3);
		if(parent_type == 3) push_string(msg, ')');
	} else if(root->part == PART_ASSIGN) {
		// if(parent_type == 3) push_string(msg, '(');
		print_tree_basic_text(msg, root->app.left->fn.arg, 2);
		push_string(msg, ' ');
		push_string(msg, SYM_ASSIGN);
		push_string(msg, ' ');
		print_tree_basic_text(msg, root->app.right, 2);
		push_string(msg, ";\n");
		print_tree_basic_text(msg, root->app.left->fn.body, 2);
		// print_tree_basic_text(msg, root->app.right, 3);
		// if(parent_type == 3) push_string(msg, ')');
	} else ASSERT(0);
}
void print_tree_basic(AST* root) {
	char* msg = 0;
	print_tree_basic_text(&msg, root, 1);
	push_string(&msg, '\0');
	printf("%s;\n", msg);
	tape_destroy(&msg);
}
static int32 print_tree_dim_(AST* root, int32** line_queue, int32* width_start, int32* width_end) {
	int32 start;
	int32 end;
	int32 queue_i = tape_size(line_queue);
	if(root->part == PART_VAR) {
		ASSERT(root->var.size > 0);
		int32 width = root->var.size;
		start = -(width - 1)/2;
		end = width/2;
	} else {
		int32 total_leafs;
		int32 pad[3] = {};
		AST** leafs = root->children;
		if(root->part == PART_FN) {
			total_leafs = 2;
			pad[0] = 2;
		} else if(root->part == PART_APP) {
			total_leafs = 2;
		} else ASSERT(0);
		int32 height = 1;
		int32 leaf_starts[4] = {};
		int32 leaf_ends[4] = {};
		tape_reserve(line_queue, total_leafs);
		for_each_lt(i, total_leafs) {
			height += print_tree_dim_(leafs[i], line_queue, &leaf_starts[i], &leaf_ends[i]);
		}
		if(total_leafs > 1) {
			int32 leaf_column[4] = {};
			leaf_column[0] = -(pad[0] + 1)/2;
			int32 extra = 1;
			for_each_lt(i, total_leafs - 1) {
				extra += (i < total_leafs - 2);
				leaf_column[i + 1] = leaf_column[i] + pad[i] + extra;
				extra = (i < total_leafs - 2 ? 2 : 1);
			}
			int32 overlap[3];
			for_each_lt(i, total_leafs - 1) {
			//find amount tree would be intersecting itself
				int32 line_pad = 2;
				int32 edge_pad = 1;
				int32 leftmost_start = leaf_starts[0] + leaf_column[0];
				int32 left_end = leaf_ends[i] + leaf_column[i];
				int32 right_start = leaf_starts[i + 1] + leaf_column[i + 1];
				overlap[i] = max(left_end - leaf_column[i + 1] + line_pad, leftmost_start - right_start + edge_pad);
			}
			for_each_lt(i, total_leafs - 1) {
				if(overlap[i] > 0) {//tree would be intersecting itself by this much
					if(i == 0) {
						int32 ld = (overlap[i] + 1)/2;
						if(ld > 0) {
							for_each_in_range(j, 0, i) leaf_column[j] -= ld;
							overlap[i] -= ld;
						}
					}
					for_each_in_range(j, i + 1, total_leafs - 1) leaf_column[j] += overlap[i];
				}
			}
			for_each_lt(i, total_leafs) {
				(*line_queue)[queue_i + i] = leaf_column[i];
			}
			*width_start = leaf_starts[0] + leaf_column[0];
			*width_end = leaf_ends[total_leafs - 1] + leaf_column[total_leafs - 1];
		} else {
			(*line_queue)[queue_i] = 0;
			*width_start = min(leaf_starts[0], -(pad[0] + 1)/2);
			*width_end = max(leaf_ends[0], (pad[0] + 2)/2);
		}
		return height;
	}
	tape_reserve(line_queue, 2);
	(*line_queue)[queue_i] = start;
	(*line_queue)[queue_i + 1] = end;
	ASSERT(((*line_queue)[queue_i] + (*line_queue)[queue_i + 1])/2 == 0);
	*width_start = start;
	*width_end = end;
	return 1;
}

static void print_tree_(AST* root, int32* line, int32 column, char* text, int32 width, int32* line_queue, int32* queue_i) {
	char* text_line = &text[(*line)*width];
	*line += 1;

	if(root->part == PART_VAR) {
		int32 left = line_queue[*queue_i] + column;
		*queue_i += 1;
		int32 right = line_queue[*queue_i] + column;
		*queue_i += 1;
		ASSERT(right < width - 1);
		ASSERT(left >= 0);
		for_each_in(ch, root->var.str, root->var.size) {
			text_line[left] = *ch;
			left += 1;
		}
	} else {
		int32 total_leafs = 0;
		AST** leafs = root->children;
		int32 columns[4] = {};
		char* divider[3] = {};

		if(root->part == PART_FN) {
			total_leafs = 2;
			divider[0] = SYM_FN;
		} else if(root->part == PART_APP) {
			total_leafs = 2;
			divider[0] = SYM_APP;
		} else ASSERT(0);
		ASSERT(total_leafs > 0);
		for_each_lt(i, total_leafs) {
			columns[i] = line_queue[*queue_i] + column;
			*queue_i += 1;
			ASSERT(columns[i] < width - 1);
			ASSERT(columns[i] >= 0);
		}
		for_each_in_range(i, columns[0] + 1, columns[total_leafs - 1] - 1) {
			text_line[i] = '_';
		}
		// ASSERT((columns[0] + columns[1])/2 == column);
		if(total_leafs == 1) {
			int32 size = strlen(divider[0]);
			int32 cur_pos = column - (size - 1)/2;
			text_line[cur_pos - 1] = '<';
			for_each_in(ch, divider[0], size) {
				text_line[cur_pos] = *ch;
				cur_pos += 1;
			}
			text_line[cur_pos] = '>';
		} else {
			for_each_lt(i, total_leafs - 1) {
				int32 size = strlen(divider[i]);
				int32 cur_pos = (columns[i] + columns[i + 1])/2 - (size - 1)/2;
				if(i == 0) cur_pos = column - (size - 1)/2;
				text_line[cur_pos - 1] = '/';
				for_each_in(ch, divider[i], size) {
					text_line[cur_pos] = *ch;
					cur_pos += 1;
				}
				text_line[cur_pos] = '\\';
			}
		}

		int32 pre_column = 0;
		int32 pre_line = *line;
		for_each_lt(i, total_leafs) {
			int32 next_line = *line;
			int32 cur_column = columns[i];
			print_tree_(leafs[i], line, cur_column, text, width, line_queue, queue_i);
			if(i > 0) {
				for_each_in_range(j, pre_line, next_line - 1) {
					text[j*width + cur_column] = '|';
				}
				if(i >= total_leafs - 1 && cur_column - pre_column > strlen(divider[i - 1]) + 2) {
					text[pre_line*width + cur_column] = '\\';
				}
			}
			pre_column = cur_column;
		}
	}
}
void print_tree(AST* root) {
	int32 width_left;
	int32 width_right;
	int32* line_queue = 0;
	int32 height = print_tree_dim_(root, &line_queue, &width_left, &width_right);
	int32 width = width_right - width_left + 2;
	char* text = talloc(char, width*height + 1);
	memset(text, ' ', width*height);

	for_each_lt(i, height) {
		text[(i + 1)*width - 1] = '\n';
	}
	text[width*height] = 0;
	int32 line = 0;
	int32 queue_i = 0;
	print_tree_(root, &line, -width_left, text, width, line_queue, &queue_i);

	printf("%s", text);
	free(text);
}

#endif
