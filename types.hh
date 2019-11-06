//By Monica Moniot
#include "basic.h"
#include "stdio.h"
#include "math.h"



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

struct ASTVar {
	union {
		char* str;
		int64 uid;
	};
	int32 size;
};

struct AST {
	Part part;
	union {
		AST* children[2];
		ASTVar var;
		struct {
			AST* left;
			AST* right;
		} app;
		struct {
			AST* arg;
			AST* body;
		} fn;
	};
};
// union ASTMem {
// 	ASTMem* next;
// 	AST node;
// };

// constexpr int32 NODE_BUFFER_SIZE = KILOBYTE;
// struct NodeBuffer {
// 	NodeBuffer* next;
// 	int32 size;
// 	ASTMem nodes[NODE_BUFFER_SIZE];
// };
struct NodeList {
	int32 size;
	// NodeBuffer* head;
	// ASTMem* free_head;
};
