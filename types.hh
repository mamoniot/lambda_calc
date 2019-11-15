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

struct Ast {
	Part part;
	union {
		Uid var_uid;
		struct {
			Ast* left;
			Ast* right;
		} app;
		struct {
			Uid arg_uid;
			Ast* body;
		} fn;
		int32 free_next_i;
	};
};

struct AstMem {
	Ast ast;
	struct AstBuffer* buffer;
};


constexpr int32 AST_BUFFER_SIZE = (4*KILOBYTE - 2*sizeof(AstBuffer*) - 4*sizeof(int32))/sizeof(AstMem);
struct AstBuffer {
	AstBuffer* free_pre;
	AstBuffer* free_next;
	int32 free_head_i;
	int32 size;
	int32 total;
	AstMem asts[AST_BUFFER_SIZE];
};

struct AstList {
	AstBuffer* free_head;
};

struct VarLexer {
	char** strings;
	int32* sizes;
};



Ast* reserve_term(AstList* ast_list) {
	if(!ast_list->free_head) {
		auto buffer = talloc(AstBuffer, 1);
		buffer->free_next = 0;
		buffer->free_pre = 0;
		buffer->free_head_i = -1;
		buffer->size = 0;
		buffer->total = 0;
		ast_list->free_head = buffer;
	}
	auto free_buffer = ast_list->free_head;
	AstMem* free_ast;
	if(free_buffer->free_head_i == -1) {
		ASSERT(free_buffer->size < AST_BUFFER_SIZE);
		int32 free_i = free_buffer->size;
		free_buffer->size += 1;
		free_ast = &free_buffer->asts[free_i];
	} else {
		int32 free_i = free_buffer->free_head_i;
		free_ast = &free_buffer->asts[free_i];
		free_buffer->free_head_i = free_ast->ast.free_next_i;
	}
	free_ast->buffer = free_buffer;
	if(free_buffer->free_head_i == -1 && free_buffer->size == AST_BUFFER_SIZE) {
		//the buffer shouldn't be on the free list, it's full
		ast_list->free_head = free_buffer->free_next;
		if(free_buffer->free_next) free_buffer->free_next->free_pre = 0;
		free_buffer->free_next = 0;
	}
	free_buffer->total += 1;
	return &free_ast->ast;
}

void free_single_term(AstList* ast_list, Ast* term) {
	auto buffer = cast(AstMem*, term)->buffer;

	buffer->total -= 1;
	if(buffer->total == 0) {
		if(buffer->free_next) buffer->free_next->free_pre = buffer->free_pre;
		if(buffer->free_pre) buffer->free_pre->free_next = buffer->free_next;
		if(ast_list->free_head == buffer) ast_list->free_head = buffer->free_next;
		free(buffer);
	} else {
		if(buffer->free_head_i == -1 && buffer->size == AST_BUFFER_SIZE) {
			//the buffer isn't on the free list, so add it
			buffer->free_next = ast_list->free_head;
			if(ast_list->free_head) ast_list->free_head->free_pre = buffer;
			buffer->free_pre = 0;
			ast_list->free_head = buffer;
		}
		//link this node onto the buffer's free list
		term->free_next_i = buffer->free_head_i;
		buffer->free_head_i = ptr_dist(&buffer->asts, term)/sizeof(AstMem);
	}
}

void free_ast(AstList* ast_list, Ast* term) {
	auto part = term->part;
	if(part == PART_APP || part == PART_ASSIGN) {
		free_ast(ast_list, term->app.left);
		free_ast(ast_list, term->app.right);
	} else if(part == PART_FN) {
		free_ast(ast_list, term->fn.body);
	}
	free_single_term(ast_list, term);
}
