/*
Author: Monica Moniot
Login:  mamoniot
GitHub: mamoniot
*/

#include "basic.h"
#define PARSER_IMPLEMENTATION
#include "parser.hh"



void alpha_rename(TermPos* pos, int64 size, int64 pre_uid, int64 new_uid) {
	auto part = get_term_part(*pos);
	if(part == PART_APP || part == PART_ASSIGN) {
		auto left_size = get_term_info(*pos);
		incr_term(pos);
		alpha_rename(pos, left_size, pre_uid, new_uid);
		alpha_rename(pos, size - 1 - left_size, pre_uid, new_uid);
	} else if(part == PART_FN) {
		auto arg_uid = get_term_info(*pos);
		incr_term(pos);
		if(arg_uid != pre_uid) {
			alpha_rename(pos, size - 1, pre_uid, new_uid);
		}
	} else if(part == PART_VAR) {
		ASSERT(size == 1);
		auto var_uid = get_term_info(*pos);
		if(var_uid == pre_uid) {
			write_term(pos, PART_VAR, new_uid);
		} else {
			incr_term(pos);
		}
	} else ASSERT(0);
}

bool is_var_free(TermPos* source, int64 source_size, int64 uid) {
	auto part = get_term_part(*source);
	if(part == PART_APP || part == PART_ASSIGN) {
		auto left_size = get_term_info(*source);
		incr_term(source);
		return is_var_free(source, left_size, uid) || is_var_free(source, source_size - 1 - left_size, uid);
	} else if(part == PART_FN) {
		auto arg_uid = get_term_info(*source);
		if(arg_uid == uid) {
			incr_term(source, source_size);
			return 0;
		} else {
			incr_term(source);
			return is_var_free(source, source_size - 1, uid);
		}
	} else if(part == PART_VAR) {
		ASSERT(source_size == 1);
		auto var_uid = get_term_info(*source);
		incr_term(source);
		return var_uid == uid;
	} else ASSERT(0);
	return 0;
}


int64 subst(TermPos* dest, TermPos* source, int64 source_size, int64* next_uid, int64 uid, TermPos term_pos, int64 term_size) {
	auto part = get_term_part(*source);
	if(part == PART_APP || part == PART_ASSIGN) {
		auto left_size = get_term_info(*source);
		auto pos = reserve_term(dest);
		incr_term(source);
		int64 new_left_size = subst(dest, source, left_size, next_uid, uid, term_pos, term_size);
		write_term(pos, PART_APP, new_left_size);
		return 1 + new_left_size + subst(dest, source, source_size - 1 - left_size, next_uid, uid, term_pos, term_size);
	} else if(part == PART_FN) {
		auto arg_uid = get_term_info(*source);
		if(arg_uid == uid) {
			copy_term(dest, source, source_size);
			return source_size;
		} else {
			TermPos term_pos_t = term_pos;
			if(is_var_free(&term_pos_t, term_size, arg_uid)) {
				write_term(dest, PART_FN, *next_uid);
				incr_term(source);
				auto source_t = *source;
				alpha_rename(&source_t, source_size - 1, arg_uid, *next_uid);
				*next_uid += 1;
			} else {
				copy_term(dest, source);
			}
			return 1 + subst(dest, source, source_size - 1, next_uid, uid, term_pos, term_size);
		}
	} else if(part == PART_VAR) {
		ASSERT(source_size == 1);
		auto var_uid = get_term_info(*source);
		if(var_uid == uid) {
			TermPos term_pos_t = term_pos;
			copy_term(dest, &term_pos_t, term_size);
			incr_term(source);
			return term_size;
		} else {
			copy_term(dest, source);
			return 1;
		}
	} else ASSERT(0);
	return 0;
}


bool get_num(int64 uid, int32* num) {
	int32 n = 0;
	if(uid&(~UID_NUM_MASK)) {
		*num = uid&UID_NUM_MASK;
		return 1;
	} else {
		return 0;
	}
}

int64 create_num(TermPos* dest, int32 num) {
	write_term(dest, PART_FN, NUM_F_UID);
	write_term(dest, PART_FN, NUM_X_UID);

	for_each_lt(i, num) {
		write_term(dest, PART_APP, 1);
		write_term(dest, PART_VAR, NUM_F_UID);
	}
	if(num == -1) {
		write_term(dest, PART_VAR, NUM_F_UID);
		return 3;
	} else {
		write_term(dest, PART_VAR, NUM_X_UID);
		return 3 + 2*num;
	}
}

int64 subst_num_(TermPos* dest, TermPos* source, int32** ignore_nums) {
	auto part = get_term_part(*source);
	if(part == PART_APP || part == PART_ASSIGN) {
		TermPos app_pos = reserve_term(dest);
		incr_term(source);
		int64 left_size = subst_num_(dest, source, ignore_nums);
		write_term(app_pos, PART_APP, left_size);
		return 1 + left_size + subst_num_(dest, source, ignore_nums);
	} else if(part == PART_FN) {
		auto arg_uid = get_term_info(*source);
		copy_term(dest, source);
		int32 n;
		if(get_num(arg_uid, &n)) {
			tape_push(ignore_nums, n);
			int64 ret = 1 + subst_num_(dest, source, ignore_nums);
			tape_pop(ignore_nums);
			return ret;
		} else {
			return 1 + subst_num_(dest, source, ignore_nums);
		}
	} else if(part == PART_VAR) {
		auto var_uid = get_term_info(*source);
		int32 num;
		bool flag = get_num(var_uid, &num);
		if(flag) {
			for_each_in(n, *ignore_nums, tape_size(ignore_nums)) {
				if(*n == num) {
					flag = 0;
					break;
				}
			}
		}
		if(flag) {
			incr_term(source);
			return create_num(dest, num);
		} else {
			copy_term(dest, source);
			return 1;
		}
	} else ASSERT(0);
	return 0;
}

void subst_num(TermPos* dest, TermPos* source) {
	int32* ignore_nums = 0;
	subst_num_(dest, source, &ignore_nums);
	tape_destroy(&ignore_nums);
}


int64 subst_to_num(TermPos* dest, TermPos* source, int64 source_size) {
	auto part = get_term_part(*source);
	if(part == PART_APP || part == PART_ASSIGN) {
		auto left_size = get_term_info(*source);
		TermPos app_pos = reserve_term(dest);
		incr_term(source);
		int64 new_left_size = subst_to_num(dest, source, left_size);
		write_term(app_pos, PART_APP, new_left_size);
		return 1 + new_left_size + subst_to_num(dest, source, source_size - 1 - left_size);
	} else if(part == PART_FN) {
		bool success = 0;
		int32 num = 0;
		TermPos source_t = *source;
		auto top_arg_uid = get_term_info(source_t);
		incr_term(&source_t);
		auto bot_fn_part = get_term_part(source_t);
		auto bot_arg_uid = get_term_info(source_t);
		if(bot_fn_part == PART_FN && bot_arg_uid != top_arg_uid) {
			incr_term(&source_t);
			while(1) {
				auto body_part = get_term_part(source_t);
				auto body_uid = get_term_info(source_t);
				if(body_part == PART_APP) {
					incr_term(&source_t);
					auto left_part = get_term_part(source_t);
					auto left_uid = get_term_info(source_t);
					if(left_part == PART_VAR && left_uid == top_arg_uid) {
						ASSERT(body_uid == 1);
						incr_term(&source_t);
						num += 1;
					} else break;
				} else if(body_part == PART_VAR) {
					if(body_uid == bot_arg_uid) {
						success = 1;
						break;
					} else if(num == 0 && body_uid == top_arg_uid) {
						success = 1;
						num = -1;
						break;
					} else break;
				} else break;
			}
		}
		if(success) {
			TermPos num_pos = reserve_term(dest);
			incr_term(source);
			write_term(num_pos, PART_VAR, num|(~UID_NUM_MASK));
			return 1;
		} else {
			copy_term(dest, source);
			return 1 + subst_to_num(dest, source, source_size - 1);
		}
	} else if(part == PART_VAR) {
		ASSERT(source_size == 1);
		copy_term(dest, source);
		return 1;
	} else ASSERT(0);
	return 0;
}

bool reduce_step(TermPos* dest, TermPos* source, int64 source_size, int64* next_uid) {
	auto part = get_term_part(*source);
	if(part == PART_APP) {
		auto source_t = *source;
		auto left_size = get_term_info(source_t);
		incr_term(&source_t);
		auto left_part = get_term_part(source_t);
		auto left_arg = get_term_info(source_t);
		incr_term(&source_t, left_size);
		auto right_pos = source_t;
		auto right_part = get_term_part(source_t);
		auto right_size = source_size - 1 - left_size;

		if(left_part == PART_APP) {
			auto left = reserve_term(dest);
			incr_term(source);
			int64 pre_count = dest->count;
			bool success = reduce_step(dest, source, left_size, next_uid);
			write_term(left, PART_APP, dest->count - pre_count);
			if(success) {
				copy_term(dest, source, right_size);
				return 1;
			} else {
				return reduce_step(dest, source, right_size, next_uid);
			}
		} else if(left_part == PART_FN) {
			incr_term(source);
			incr_term(source);
			subst(dest, source, left_size - 1, next_uid, left_arg, right_pos, right_size);
			incr_term(source, right_size);
			return 1;
		} else if(left_part == PART_VAR) {
			ASSERT(left_size == 1);
			copy_term(dest, source);
			copy_term(dest, source);
			return reduce_step(dest, source, right_size, next_uid);
		} else ASSERT(0);
	} else if(part == PART_FN) {
		copy_term(dest, source);
		return reduce_step(dest, source, source_size - 1, next_uid);
	} else if(part == PART_VAR) {
		ASSERT(source_size == 1);
		copy_term(dest, source);
		return 0;
	} else ASSERT(0);
	return 0;
}



bool reduce_assign(TermPos* dest, TermPos* source, int64 source_size, int64* next_uid) {
	if(get_term_part(*source) == PART_ASSIGN) {
		auto source_t = *source;
		auto left_size = get_term_info(source_t);
		incr_term(&source_t);
		auto left_part = get_term_part(source_t);
		auto left_arg = get_term_info(source_t);
		incr_term(&source_t, left_size);
		auto right_pos = source_t;
		auto right_part = get_term_part(source_t);
		auto right_size = source_size - 1 - left_size;

		ASSERT(left_part == PART_FN);
		incr_term(source);
		incr_term(source);
		subst(dest, source, left_size - 1, next_uid, left_arg, right_pos, right_size);
		incr_term(source, right_size);
		return 1;
	} else {
		copy_term(dest, source, source_size);
		return 0;
	}
}

void remove_null(TermPos* dest, TermPos* source) {
	auto part = get_term_part(*source);
	if(part == PART_APP || part == PART_ASSIGN) {
		copy_term(dest, source);
		remove_null(dest, source);
		remove_null(dest, source);
	} else if(part == PART_FN) {
		copy_term(dest, source);
		remove_null(dest, source);
	} else if(part == PART_VAR) {
		copy_term(dest, source);
	} else if(part == PART_NULL) {
		incr_term(source);
		remove_null(dest, source);
	} else ASSERT(0);
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
	char* possible_flags[5] = {"-p", "-nonum", "-noassign", "-noreduce", "-help"};
	uint32 flags;
	char* filename;
	parse_argv(argv, argc, possible_flags, 5, &flags, &filename, 1);
	if(flags & 0b10000) {
		printf("available flags:\n-p : each reduce step is printed out\n-nonum : all numbers are printed as their functional representation\n-noassign : variable declarations are not assigned initially\n-noreduce : the given lambda calculus term is not reduced past assigning variable declarations");
		return 0;
	}
	bool do_print = flags&0b1;
	bool nonum = flags&0b10;
	bool noassign = flags&0b100;
	bool noreduce = flags&0b1000;
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
		Source source_code = {};
		source_code.text = text;
		source_code.size = tape_size(&text);
		Tokens tokens = tokenize(&source_code);

		char* error_msg = 0;
		TermPos source = create_term_list();
		TermPos source_t = source;
		int64 source_size = 0;
		TermPos dest = create_term_list();
		TermPos dest_t = dest;

		#define swap_dest_and_source() {\
			pop_after(dest_t);\
			memswap(&dest, &source);\
			source_size = dest_t.count;\
			memzero(dest.buffer, 1);\
			dest_t = dest;\
			source_t = source;\
		}

		VarSyntaxStack vars;
		if(!parse_all(&source_t, source_code, tokens, &vars, &error_msg)) {
			ASSERT(error_msg && error_msg[0]);
			printf("%.*s", tape_size(&error_msg), error_msg);
			return 0;
		}
		source_size = source_t.count;
		dest_t = dest;
		source_t = source;

		remove_null(&dest_t, &source_t);
		swap_dest_and_source();

		int64 total_uids = FIRST_UID;
		int64 total_steps = 0;
		while(reduce_assign(&dest_t, &source_t, source_size, &total_uids)) {
			swap_dest_and_source();
			if(do_print) {
				print_term(&source_t, &vars);
				printf("==>\n");
				source_t = source;
			}
			total_steps += 1;
		}
		swap_dest_and_source();
		if(!nonum) {
			print_term(&source_t, &vars);
			source_t = source;
			printf("==>\n");
		}
		subst_num(&dest_t, &source_t);
		swap_dest_and_source();
		print_term(&source_t, &vars);
		source_t = source;
		if(!noreduce) {
			while(reduce_step(&dest_t, &source_t, source_size, &total_uids)) {
				swap_dest_and_source();
				if(do_print) {
					printf("==>\n");
					print_term(&source_t, &vars);
					source_t = source;
				}
				total_steps += 1;
			}
			swap_dest_and_source();
			if(!nonum) {
				subst_to_num(&dest_t, &source_t, source_size);
				swap_dest_and_source();
				printf("==>\n");
				print_term(&source_t, &vars);
				source_t = source;
			}
			printf("\nreduction took %lld steps\nused %lld uids", total_steps, total_uids);
		}
	}
	//NOTE: memory is not released before exit
	return 0;
}
