/*
Author: Monica Moniot
Login:  mamoniot
GitHub: mamoniot
*/
#ifdef RELEASE
 #define BASIC_NO_ASSERT
#endif
#include "basic.h"
#define PARSER_IMPLEMENTATION
#include "parser.hh"


void alpha_rename(Ast* term, Uid pre_uid, Uid new_uid) {
	if(term->part == PART_APP || term->part == PART_ASSIGN) {
		alpha_rename(term->app.left, pre_uid, new_uid);
		alpha_rename(term->app.right, pre_uid, new_uid);
	} else if(term->part == PART_FN) {
		if(term->fn.arg_uid != pre_uid) {
			alpha_rename(term->fn.body, pre_uid, new_uid);
		}
	} else if(term->part == PART_VAR) {
		if(term->var_uid == pre_uid) {
			term->var_uid = new_uid;
		}
	}
}

bool is_var_free(Ast* term, Uid uid) {
	if(term->part == PART_APP || term->part == PART_ASSIGN) {
		return is_var_free(term->app.left, uid) || is_var_free(term->app.right, uid);
	} else if(term->part == PART_FN) {
		if(term->fn.arg_uid == uid) {
			return 0;
		} else {
			return is_var_free(term->fn.body, uid);
		}
	} else if(term->part == PART_VAR) {
		return term->var_uid == uid;
	} else ASSERT(0);
	return 0;
}

Ast* copy_tree(AstList* ast_list, Ast* root) {
	Ast* new_root = reserve_term(ast_list);
	memcopy(new_root, root, 1);
	if(root->part == PART_APP || root->part == PART_ASSIGN) {
		new_root->app.left = copy_tree(ast_list, root->app.left);
		new_root->app.right = copy_tree(ast_list, root->app.right);
	} else if(root->part == PART_FN) {
		new_root->fn.body = copy_tree(ast_list, root->fn.body);
	}
	return new_root;
}

bool subst(AstList* ast_list, Uid* next_uid, Ast** root_ptr, Uid uid, Ast* term, bool do_copy) {
	Ast* root = *root_ptr;
	if(root->part == PART_APP || root->part == PART_ASSIGN) {
		do_copy = subst(ast_list, next_uid, &root->app.left, uid, term, do_copy);
		return subst(ast_list, next_uid, &root->app.right, uid, term, do_copy);
	} else if(root->part == PART_FN) {
		if(root->fn.arg_uid != uid) {
			if(is_var_free(term, root->fn.arg_uid)) {
				alpha_rename(root->fn.body, root->fn.arg_uid, *next_uid);
				root->fn.arg_uid = *next_uid;
				*next_uid += 1;
			}
			return subst(ast_list, next_uid, &root->fn.body, uid, term, do_copy);
		}
	} else if(root->part == PART_VAR) {
		if(root->var_uid == uid) {
			free_single_term(ast_list, root);
			if(do_copy) {
				*root_ptr = copy_tree(ast_list, term);
			} else {
				*root_ptr = term;
			}
			return 1;
		}
	}
	return do_copy;
}


bool get_num(Uid uid, int32* num) {
	if(uid&(~NUM_UID_MASK)) {
		*num = uid&NUM_UID_MASK;
		return 1;
	} else {
		return 0;
	}
}

Ast* create_num(AstList* ast_list, int32 num) {
	Ast* top_fn = reserve_term(ast_list);
	top_fn->part = PART_FN;
	top_fn->fn.arg_uid = NUM_F_UID;
	Ast* bot_fn = reserve_term(ast_list);
	bot_fn->part = PART_FN;
	bot_fn->fn.arg_uid = NUM_X_UID;
	Ast* body = reserve_term(ast_list);
	top_fn->fn.body = bot_fn;
	bot_fn->fn.body = body;

	for_each_lt(i, num) {
		Ast* f_var = reserve_term(ast_list);
		f_var->part = PART_VAR;
		Ast* new_body = reserve_term(ast_list);
		body->part = PART_APP;
		f_var->var_uid = NUM_F_UID;
		body->app.left = f_var;
		body->app.right = new_body;
		body = new_body;
	}
	body->part = PART_VAR;
	if(num == -1) {
		body->var_uid = NUM_F_UID;
	} else {
		body->var_uid = NUM_X_UID;
	}
	return top_fn;
}

void subst_num_(AstList* ast_list, int32** ignore_nums, Ast** root_ptr) {
	Ast* root = *root_ptr;
	if(root->part == PART_APP || root->part == PART_ASSIGN) {
		subst_num_(ast_list, ignore_nums, &root->app.left);
		subst_num_(ast_list, ignore_nums, &root->app.right);
	} else if(root->part == PART_FN) {
		int32 n;
		if(get_num(root->fn.arg_uid, &n)) {
			tape_push(ignore_nums, n);
			subst_num_(ast_list, ignore_nums, &root->fn.body);
			tape_pop(ignore_nums);
		} else {
			subst_num_(ast_list, ignore_nums, &root->fn.body);
		}
	} else if(root->part == PART_VAR) {
		int32 num;
		if(get_num(root->var_uid, &num)) {
			for_each_in(n, *ignore_nums, tape_size(ignore_nums)) {
				if(*n == num) return;
			}
			free_single_term(ast_list, root);
			*root_ptr = create_num(ast_list, num);
		}
	}
}

void subst_num(AstList* ast_list, Ast** root_ptr) {
	int32* ignore_nums = 0;
	subst_num_(ast_list, &ignore_nums, root_ptr);
	tape_destroy(&ignore_nums);
}


void subst_to_num(AstList* ast_list, Ast** root_ptr) {
	Ast* root = *root_ptr;
	if(root->part == PART_APP || root->part == PART_ASSIGN) {
		subst_to_num(ast_list, &root->app.left);
		subst_to_num(ast_list, &root->app.right);
	} else if(root->part == PART_FN) {
		bool success = 1;
		int32 num = 0;
		Uid top_arg = root->fn.arg_uid;
		Ast* bot_fn = root->fn.body;
		if(bot_fn->part == PART_FN) {
			Uid bot_arg = bot_fn->fn.arg_uid;
			if(bot_arg == top_arg) success = 0;
			Ast* body = bot_fn->fn.body;
			while(success) {
				if(body->part == PART_APP || body->part == PART_ASSIGN) {
					Ast* left = body->app.left;
					if(left->part != PART_VAR || left->var_uid != top_arg) {
						success = 0;
						break;
					}
					body = body->app.right;
					num += 1;
				} else if(body->part == PART_VAR) {
					if(body->var_uid == bot_arg) {
						break;
					} else if(num == 0 && body->var_uid == top_arg) {
						num = -1;
						break;
					} else success = 0;
				} else success = 0;
			}
		} else success = 0;
		if(success) {
			Ast* new_node = reserve_term(ast_list);
			new_node->part = PART_VAR;
			new_node->var_uid = num|(~NUM_UID_MASK);
			free_ast(ast_list, root);
			*root_ptr = new_node;
		} else {
			subst_to_num(ast_list, &root->fn.body);
		}
	}
}

Ast** reduce_step(AstList* ast_list, Uid* next_uid, Ast** root_ptr) {
	Ast* root = *root_ptr;
	if(root->part == PART_APP || root->part == PART_ASSIGN) {
		Ast* left = root->app.left;
		Ast* right = root->app.right;
		if(left->part == PART_APP || left->part == PART_ASSIGN) {
			auto ret = reduce_step(ast_list, next_uid, &root->app.left);
			if(ret) return ret;
		} else if(left->part == PART_FN) {
			if(!subst(ast_list, next_uid, &left->fn.body, left->fn.arg_uid, right, 0)) {
				free_single_term(ast_list, right);
			}
			*root_ptr = left->fn.body;
			free_single_term(ast_list, root);
			free_single_term(ast_list, left);
			return root_ptr;
		}
		return reduce_step(ast_list, next_uid, &root->app.right);
	} else if(root->part == PART_FN) {
		return reduce_step(ast_list, next_uid, &root->fn.body);
	}
	return 0;
}
// Ast** reduce_step(ast_list, Uid* next_uid, Ast** top_ptr) {
// 	Ast*** stack = 0;
// 	tape_push(&stack, top_ptr);
// 	while(tape_size(&stack) > 0) {
// 		Ast** root_ptr = tape_pop(&stack);
// 		Ast* root = *root_ptr;
// 		if(root->part == PART_APP || root->part == PART_ASSIGN) {
// 			Ast* left = root->app.left;
// 			Ast* right = root->app.right;
// 			if(left->part == PART_APP || left->part == PART_ASSIGN) {
// 				tape_push(&stack, &root->app.right);
// 				tape_push(&stack, &root->app.left);
// 			} else if(left->part == PART_FN) {
// 				subst(ast_list, next_uid, &left->fn.body, left->fn.arg_uid, right, 0);
// 				*root_ptr = left->fn.body;
// 				tape_push(&stack, root_ptr);
// 				// tape_destroy(&stack);
// 				// return root_ptr;
// 			} else if(left->part == PART_VAR) {
// 				tape_push(&stack, &root->app.right);
// 			}
// 		} else if(root->part == PART_FN) {
// 			tape_push(&stack, &root->fn.body);
// 		} else if(root->part == PART_VAR) {
// 		} else ASSERT(0);
// 	}
// 	return 0;
// }

bool reduce_assign(AstList* ast_list, Uid* next_uid, Ast** root_ptr) {
	Ast* root = *root_ptr;
	if(root->part == PART_ASSIGN) {
		Ast* left = root->app.left;
		Ast* right = root->app.right;
		ASSERT(left->part == PART_FN);
		subst(ast_list, next_uid, &left->fn.body, left->fn.arg_uid, right, 0);
		*root_ptr = left->fn.body;
		return 1;
	} else return 0;
}

void parse_argv(char** argv, int32 argc, char** flags, int32 flags_size, uint32* ret_flags, char** ret_non_flags, int32 non_flags_size) {
	*ret_flags = 0;
	int32 non_flags_i = 0;
	for_each_index(i, arg, &argv[1], argc - 1) {
		bool is = 0;
		for_each_index(j, flag, flags, flags_size) {
			// printf("%s==%s\n", *arg, *flag);
			if(strcmp(*arg, *flag) == 0) {
				*ret_flags |= 1<<j;
				is = 1;
				break;
			}
		}
		if(is) continue;
		if(non_flags_i < non_flags_size) {
			ret_non_flags[non_flags_i] = *arg;
			non_flags_i += 1;
		}
	}
	for_each_in_range(i, non_flags_i, non_flags_size - 1) {
		ret_non_flags[i] = 0;
	}
}

int main(int32 argc, char** argv) {
	char* text = 0;
	char* possible_flags[5] = {"-p", "-nonum", "-a", "-noreduce", "-help"};
	uint32 flags;
	char* filename;
	parse_argv(argv, argc, possible_flags, 5, &flags, &filename, 1);
	if(flags & 0b10000) {
		printf("available flags:\n-p : each reduce step is printed out\n-nonum : all numbers are printed as their functional representation\n-a : variable declarations are assigned initially\n-noreduce : the given lambda calculus term is not reduced past assigning variable declarations");
		return 0;
	}
	bool do_print  = (flags&0b1) > 0;
	bool do_num    = (flags&0b10) == 0;
	bool do_assign = (flags&0b100) > 0;
	bool do_reduce = (flags&0b1000) == 0;
	if(filename) {
		FILE* source_file = fopen(filename, "r");
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
			printf("Could not open file: %s.", filename);
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
		VarLexer lexer = {};
		AstList ast_list = {};
		Ast* root = parse_all(source, tokens, &ast_list, &lexer, &error_msg);
		destroy_tokens(tokens);
		if(!root) {
			ASSERT(error_msg && error_msg[0]);
			printf("%.*s", tape_size(&error_msg), error_msg);
			return 0;
		}
		bool skipped_first_arrow = 0;
		Uid total_uids = FIRST_UID;
		int64 total_steps = 0;

		if(!do_num) {
			subst_num(&ast_list, &root);
		}
		if(do_assign) {
			skipped_first_arrow = 1;
			print_term(root, &lexer);
		}
		while(reduce_assign(&ast_list, &total_uids, &root)) {
			if(do_assign) {
				if(do_print) {
					if(!skipped_first_arrow) {
						skipped_first_arrow = 1;
					} else printf("==>\n");
					print_term(root, &lexer);
				}
				total_steps += 1;
			}
		}
		if(!do_assign) {
			skipped_first_arrow = 1;
			print_term(root, &lexer);
		}
		if(do_reduce) {
			if(do_num) {
				subst_num(&ast_list, &root);
				if(!do_assign & do_print) {
					printf("==>\n");
					print_term(root, &lexer);
				}
			}
			Ast** cur_root = 0;
			while(1) {
				Ast** next_root = reduce_step(&ast_list, &total_uids, cur_root ? cur_root : &root);
				if(!next_root && !cur_root) break;
				cur_root = next_root;
				if(next_root && do_print) {
					printf("==>\n");
					print_term(root, &lexer, *next_root);
				}
				total_steps += 1;
			}
			if(do_num) {
				subst_to_num(&ast_list, &root);
				printf("==>\n");
				print_term(root, &lexer);
			} else if(!do_print) {
				printf("==>\n");
				print_term(root, &lexer);
			}
		}
		printf("\nreduction took %lld steps\n%lld variables were alpha renamed", total_steps, total_uids - FIRST_UID);
	}
	//NOTE: memory is not released before exit
	return 0;
}
