//By Monica Moniot
#include "basic.h"
#include "stdio.h"
#include "math.h"



inline int32 min(int32 a, int32 b) {return a < b ? a : b;}
inline int32 max(int32 a, int32 b) {return a > b ? a : b;}

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


constexpr int32 TERM_BUFFER_SIZE = KILOBYTE;
struct TermBuffer {
	// int32 size;
	TermBuffer* next;
	TermBuffer* pre;
	Part parts[TERM_BUFFER_SIZE];
	union {
		int64 jump_dists[TERM_BUFFER_SIZE];
		int64 var_uids[TERM_BUFFER_SIZE];
	};
};
// struct TermList {
// 	int64 size;
// 	TermBuffer* head;
// };

struct TermPos {
	TermBuffer* buffer;
	int32 i;
	int64 count;
};



constexpr int64 NUM_F_UID = 0;
constexpr int64 NUM_X_UID = 1;
constexpr int64 FIRST_UID = 2;
constexpr int64 UID_VAR_MASK = ~(1ll<<63);
constexpr int64 UID_NUM_MASK = ~(1ll<<62);

struct VarSyntaxStack {
	char** strings;
	int32* sizes;
};

TermPos create_term_list() {
	TermPos ret = {talloc(TermBuffer, 1), 0, 0};
	ret.buffer->next = 0;
	ret.buffer->pre = 0;
	return ret;
}
void destroy_term_list(TermPos pos) {
	TermBuffer* cur = pos.buffer->next;
	while(cur) {
		auto pre = cur;
		cur = cur->next;
		free(pre);
	}
	cur = pos.buffer;
	while(cur) {
		auto pre = cur;
		cur = cur->pre;
		free(pre);
	}
}

void incr_term(TermPos* pos) {
	pos->i += 1;
	auto buffer = pos->buffer;
	if(pos->i >= TERM_BUFFER_SIZE) {
		pos->i = 0;
		if(!buffer->next) {
			auto new_buffer = talloc(TermBuffer, 1);
			// new_buffer->size = 0;
			new_buffer->next = 0;
			new_buffer->pre = buffer;
			buffer->next = new_buffer;
		}
		pos->buffer = buffer->next;
	}
}
void incr_term(TermPos* pos, int64 dist) {
	int64 i = pos->i + dist;
	while(i >= TERM_BUFFER_SIZE) {
		auto buffer = pos->buffer;
		i -= TERM_BUFFER_SIZE;
		if(!buffer->next) {
			ASSERT(i == 0);
			auto new_buffer = talloc(TermBuffer, 1);
			// new_buffer->size = 0;
			new_buffer->next = 0;
			new_buffer->pre = buffer;
			buffer->next = new_buffer;
		}
		pos->buffer = buffer->next;
	}
	pos->i = i;
}
Part get_term_part(TermPos pos) {
	return pos.buffer->parts[pos.i];
}
int64 get_term_info(TermPos pos) {
	return pos.buffer->var_uids[pos.i];
}


TermPos reserve_term(TermPos* pos) {
	TermPos ret = *pos;
	incr_term(pos);
	pos->count += 1;
	return ret;
}
void write_term(TermPos pos, Part part, int64 info) {
	pos.buffer->parts[pos.i] = part;
	pos.buffer->var_uids[pos.i] = info;
}
void write_term(TermPos* pos, Part part, int64 info) {
	write_term(*pos, part, info);
	incr_term(pos);
	pos->count += 1;
}

void copy_term(TermPos* dest, TermPos* source) {
	write_term(*dest, source->buffer->parts[source->i], source->buffer->var_uids[source->i]);
	incr_term(dest);
	incr_term(source);
	dest->count += 1;
}
void copy_term(TermPos* dest, TermPos* source, int64 size) {
	dest->count += size;
	while(size > 0) {
		int32 copy_size = min(size, TERM_BUFFER_SIZE - max(dest->i, source->i));
		memcopy(&dest->buffer->parts[dest->i], &source->buffer->parts[source->i], copy_size);
		memcopy(&dest->buffer->var_uids[dest->i], &source->buffer->var_uids[source->i], copy_size);
		incr_term(source, copy_size);
		incr_term(dest, copy_size);
		size -= copy_size;
	}
}

void pop_term(TermPos* pos) {
	auto buffer = pos->buffer;
	pos->i -= 1;
	if(pos->i < 0) {
		ASSERT(buffer->pre);
		buffer->pre->next = buffer->next;
		if(buffer->next) buffer->next->pre = buffer->pre;
		pos->i = TERM_BUFFER_SIZE - 1;
		pos->buffer = buffer->pre;
		free(buffer);
	}
	pos->count -= 1;
}
void pop_term(TermPos* pos, int32 total) {
	//TODO: optimize this?
	for_each_lt(i, total) pop_term(pos);
}
void pop_after(TermPos pos) {
	TermBuffer* cur = pos.buffer->next;
	while(cur) {
		auto pre = cur;
		cur = cur->next;
		free(pre);
	}
}
