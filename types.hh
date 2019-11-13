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

struct AST {
	Part part;
	union {
		Uid var_uid;
		struct {
			AST* left;
			AST* right;
		} app;
		struct {
			Uid arg_uid;
			AST* body;
		} fn;
	};
};

struct VarLexer {
	char** strings;
	int32* sizes;
};
