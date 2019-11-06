/*
Author: Monica Moniot
Login:  mamoniot
GitHub: mamoniot
*/

#include "basic.h"
#define PARSER_IMPLEMENTATION
#include "parser.hh"

constexpr int64 NUM_F_UID = 0;
constexpr int64 NUM_X_UID = 1;
constexpr int64 FIRST_UID = 2;

bool is_var_eq(ASTVar var0, ASTVar var1) {
	return var0.size == var1.size && (var0.size > 0 ? (memcmp(var0.str, var1.str, var0.size) == 0) : (var0.uid == var1.uid));
}

void alpha_rename(AST* term, ASTVar var, int64 uid) {
	if(term->part == PART_APP) {
		alpha_rename(term->app.left, var, uid);
		alpha_rename(term->app.right, var, uid);
	} else if(term->part == PART_FN) {
		if(!is_var_eq(term->fn.arg->var, var)) {
			alpha_rename(term->fn.body, var, uid);
		}
	} else if(term->part == PART_VAR) {
		if(is_var_eq(term->var, var)) {
			term->var.uid = uid;
			term->var.size = 0;
		}
	}
}

bool is_var_free(ASTVar var, AST* term) {
	if(term->part == PART_APP) {
		return is_var_free(var, term->app.left) || is_var_free(var, term->app.right);
	} else if(term->part == PART_FN) {
		if(is_var_eq(term->fn.arg->var, var)) {
			return 0;
		} else {
			return is_var_free(var, term->fn.body);
		}
	} else if(term->part == PART_VAR) {
		return is_var_eq(term->var, var);
	}
	ASSERT(0);
	return 0;
}

AST* copy_tree(NodeList* node_list, AST* root) {
	AST* new_root = reserve_node(node_list);
	memcopy(new_root, root, 1);
	if(root->part == PART_APP || root->part == PART_FN) {
		new_root->children[0] = copy_tree(node_list, root->children[0]);
		new_root->children[1] = copy_tree(node_list, root->children[1]);
	}
	return new_root;
}

bool subst(NodeList* node_list, int64* next_uid, AST** root_ptr, ASTVar var, AST* term, bool do_copy) {
	AST* root = *root_ptr;
	if(root->part == PART_APP) {
		do_copy = subst(node_list, next_uid, &root->app.left, var, term, do_copy);
		return subst(node_list, next_uid, &root->app.right, var, term, do_copy);
	} else if(root->part == PART_FN) {
		if(!is_var_eq(root->fn.arg->var, var)) {
			if(is_var_free(root->fn.arg->var, term)) {
				alpha_rename(root->fn.body, root->fn.arg->var, *next_uid);
				root->fn.arg->var.uid = *next_uid;
				root->fn.arg->var.size = 0;
				*next_uid += 1;
			}
			return subst(node_list, next_uid, &root->fn.body, var, term, do_copy);
		}
	} else if(root->part == PART_VAR) {
		if(is_var_eq(root->var, var)) {
			free_node(node_list, root);
			if(do_copy) {
				*root_ptr = copy_tree(node_list, term);
			} else {
				*root_ptr = term;
			}
			return 1;
		}
	}
	return do_copy;
}


bool get_num(ASTVar var, int32* num) {
	int32 n = 0;
	if(var.size > 0) {
		if(strcmp(var.str, var.size, "true")) {
			*num = -1;
			return 1;
		} else if(strcmp(var.str, var.size, "false")) {
			*num = 0;
			return 1;
		}
		for_each_index(i, ch, var.str, var.size) {
			if('0' <= *ch & *ch <= '9') {
				n *= 10;
				n += *ch - '0';
			} else {
				return 0;
			}
		}
		*num = n;
		return 1;
	} else if(var.size == -1) {
		*num = var.uid;
		return 1;
	} else {
		return 0;
	}
}

AST* create_num(NodeList* node_list, int32 num) {
	AST* top_fn = reserve_node(node_list);
	top_fn->part = PART_FN;
	AST* top_arg = reserve_node(node_list);
	top_arg->part = PART_VAR;
	AST* bot_fn = reserve_node(node_list);
	bot_fn->part = PART_FN;
	AST* bot_arg = reserve_node(node_list);
	bot_arg->part = PART_VAR;
	AST* body = reserve_node(node_list);
	top_fn->fn.arg = top_arg;
	top_fn->fn.body = bot_fn;
	bot_fn->fn.arg = bot_arg;
	bot_fn->fn.body = body;
	top_arg->var.uid = NUM_F_UID;
	top_arg->var.size = 0;
	bot_arg->var.uid = NUM_X_UID;
	bot_arg->var.size = 0;

	for_each_lt(i, num) {
		AST* f_var = reserve_node(node_list);
		f_var->part = PART_VAR;
		AST* new_body = reserve_node(node_list);
		body->part = PART_APP;
		f_var->var.uid = NUM_F_UID;
		f_var->var.size = 0;
		body->app.left = f_var;
		body->app.right = new_body;
		body = new_body;
	}
	body->part = PART_VAR;
	if(num == -1) {
		body->var.uid = NUM_F_UID;
	} else {
		body->var.uid = NUM_X_UID;
	}
	body->var.size = 0;
	return top_fn;
}

void subst_num_(int32** ignore_nums, NodeList* node_list, AST** root_ptr) {
	AST* root = *root_ptr;
	if(root->part == PART_APP) {
		subst_num_(ignore_nums, node_list, &root->app.left);
		subst_num_(ignore_nums, node_list, &root->app.right);
	} else if(root->part == PART_FN) {
		int32 n;
		if(get_num(root->fn.arg->var, &n)) {
			tape_push(ignore_nums, n);
			subst_num_(ignore_nums, node_list, &root->fn.body);
			tape_pop(ignore_nums);
		} else {
			subst_num_(ignore_nums, node_list, &root->fn.body);
		}
	} else if(root->part == PART_VAR) {
		int32 num;
		if(get_num(root->var, &num)) {
			for_each_in(n, *ignore_nums, tape_size(ignore_nums)) {
				if(*n == num) return;
			}
			free_node(node_list, root);
			*root_ptr = create_num(node_list, num);
		}
	}
}

void subst_num(NodeList* node_list, AST** root_ptr) {
	int32* ignore_nums = 0;
	subst_num_(&ignore_nums, node_list, root_ptr);
	tape_destroy(&ignore_nums);
}


void subst_to_num(NodeList* node_list, AST** root_ptr) {
	AST* root = *root_ptr;
	if(root->part == PART_APP) {
		subst_to_num(node_list, &root->app.left);
		subst_to_num(node_list, &root->app.right);
	} else if(root->part == PART_FN) {
		ASTVar top_arg = root->fn.arg->var;
		AST* bot_fn = root->fn.body;
		if(bot_fn->part != PART_FN) return;
		ASTVar bot_arg = bot_fn->fn.arg->var;
		if(is_var_eq(bot_arg, top_arg)) return;
		AST* body = bot_fn->fn.body;
		int32 n = 0;
		while(1) {
			if(body->part == PART_APP) {
				AST* left = body->app.left;
				if(left->part != PART_VAR || !is_var_eq(left->var, top_arg)) return;
				body = body->app.right;
				n += 1;
			} else if(body->part == PART_VAR) {
				if(is_var_eq(body->var, bot_arg)) {
					break;
				} else if(n == 0 && is_var_eq(body->var, top_arg)) {
					n = -1;
					break;
				} else return;
			} else return;
		}

		AST* new_node = reserve_node(node_list);
		new_node->part = PART_VAR;
		new_node->var.uid = n;
		new_node->var.size = -1;
		free_node(node_list, root);
		*root_ptr = new_node;
	}
}

bool reduce_step(NodeList* node_list, int64* next_uid, AST** root_ptr) {
	AST* root = *root_ptr;
	if(root->part == PART_APP) {
		AST* left = root->app.left;
		AST* right = root->app.right;
		bool has_reduced = 0;
		if(left->part == PART_APP) {
			has_reduced = reduce_step(node_list, next_uid, &root->app.left);
		} else if(left->part == PART_FN) {
			subst(node_list, next_uid, &left->fn.body, left->fn.arg->var, right, 0);
			*root_ptr = left->fn.body;
			has_reduced = 1;
		}
		return has_reduced || reduce_step(node_list, next_uid, &root->app.right);
	} else if(root->part == PART_FN) {
		return reduce_step(node_list, next_uid, &root->fn.body);
	}
	return 0;
}


int main(int32 argc, char** argv) {
	//TODO: multiple source files?
	char* text = 0;
	if(argc > 1) {
		FILE* source_file = fopen(argv[1], "r");
		if(source_file) {
			int ch = fgetc(source_file);
			while(ch != EOF) {
				if(ch == '\t') {
					//TODO: fix tabs and remove this
					tape_push(&text, ' ');
					tape_push(&text, ' ');
					tape_push(&text, ' ');
					tape_push(&text, ' ');
				} else {
					tape_push(&text, cast(char, ch));
				}
				ch = fgetc(source_file);
			}
			fclose(source_file);
		} else {
			printf("Could not open file: %s.", argv[1]);
		}
	} else {
		while(1) {
	        int c = getchar();
	        if(c == EOF || c == '\n') {
				break;
			} else {
				tape_push(&text, c);
			}
	    }
	}

	if(text) {
		Source source = {};
		source.text = text;
		source.size = tape_size(&text);
		Tokens tokens = tokenize(&source);
		char* error_msg = 0;
		NodeList tree_mem;
		AST* root = parse_all(source, tokens, &error_msg, &tree_mem);
		if(root) {
			subst_num(&tree_mem, &root);
			print_tree_basic(root);
			int64 total_uids = FIRST_UID;
			int64 total_steps = 0;
			while(reduce_step(&tree_mem, &total_uids, &root)) {
				// printf("=");
				// print_tree_basic(root);
				total_steps += 1;
			}
			printf("=");
			print_tree_basic(root);
			subst_to_num(&tree_mem, &root);
			printf("=");
			print_tree_basic(root);
			printf("\nreduction took %lld steps\nused %lld uids", total_steps, total_uids);
		} else {
			ASSERT(error_msg && error_msg[0]);
			printf("%.*s", tape_size(&error_msg), error_msg);
		}
	}
	//NOTE: memory is not released before exit
	return 0;
}
