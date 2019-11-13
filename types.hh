//By Monica Moniot
#include "basic.h"
#include "stdio.h"
#include "math.h"


static int32 min(int32 a, int32 b) {return a < b ? a : b;}
static int32 max(int32 a, int32 b) {return a > b ? a : b;}

typedef int64 Uid;

struct Source {
	char* text;
	int32* lines;
	int32 size;
};

struct SourcePos {
	int32 line;
	int32 column;
	int32 size;
};

enum TokenState : int32 {
	TOKEN_NULL,
	TOKEN_BASE,
	TOKEN_EOF,
	TOKEN_NAME,
	TOKEN_SINGLE_SYM,
	TOKEN_SYM,
	TOKEN_PAREN,
};
struct Tokens {
	TokenState* states;
	char** strings;
	int32* sizes;
	int32* lines;
	int32* columns;
};

enum Part : int32 {
	PART_NULL = 0,
	PART_VAR,
	PART_FN,
	PART_APP,
	PART_ASSIGN,
};


constexpr Uid NUM_F_UID = 0;
constexpr Uid NUM_X_UID = 1;
constexpr Uid FIRST_UID = 2;
constexpr Uid NUM_UID_MASK = ~(1ll<<62);
constexpr Uid VAR_UID_MASK = ~(1ll<<63);


struct AstLoc {
	struct AstBuffer* buffer;
	int32 i;
};

union AstData {
	Uid var_uid;
	struct {
		AstBuffer* left_buffer;
		AstBuffer* right_buffer;
		int32 left_i;
		int32 right_i;
	} app;
	struct {
		Uid arg_uid;
		AstLoc body;
	} fn;
	struct {
		AstBuffer* next_buffer;
		AstBuffer* prev_buffer;
		int32 next_i;
		int32 prev_i;
	} free_list;
};

constexpr int32 AST_BUFFER_SIZE = KILOBYTE;
struct AstBuffer {
	AstBuffer* next;
	AstBuffer* prev;
	int32 size;
	Part parts[AST_BUFFER_SIZE];
	AstData data[AST_BUFFER_SIZE];
};

// struct Ast {
// 	Part part;
// 	union AstData {
// 		Uid var_uid;
// 		struct {
// 			AST* left;
// 			AST* right;
// 		} app;
// 		struct {
// 			Uid arg_uid;
// 			AST* body;
// 		} fn;
// 	};
// };

struct AstList {
	AstBuffer* head;
	AstLoc free_head;
};

struct VarLexer {
	char** strings;
	int32* sizes;
};


Part* get_part(AstLoc root) {
	return &root.buffer->parts[root.i];
}
AstData* get_data(AstLoc root) {
	return &root.buffer->data[root.i];
}

AstLoc reserve_term(AstList* ast_list) {
	if(!ast_list->free_head.buffer) {
		auto buffer = talloc(AstBuffer, 1);
		if(ast_list->head) {

		} else {
			ast_list->head = buffer;
			buffer->next = buffer;
			buffer->prev = buffer;
		}
	}
	auto new_term = ast_list->free_head;
	ast_list->free_head = get_data(new_term)->next_free;
	return new_term;
}
