/* See LICENSE for license details. */
#ifndef __BOARD_H__
#define __BOARD_H__

typedef int (*board_rand_int_method)(void);

typedef struct {
	struct {	
		int row;
		int column;
		int value;
	} from, to;
} tile_sliding_t;

typedef void (*tile_sliding_notify)(tile_sliding_t);

typedef struct {
	unsigned int          tiles[4 * 4];
	unsigned int          size;
	board_rand_int_method rand_int;
	tile_sliding_notify   on_sliding;
	int                   reversed;
	int                   transposed;
	int                   last_slide_movements;
} board_t;

board_t board_create(board_rand_int_method rand_int);
int     board_tile_index(board_t *board, int column, int row);
void    board_move_left(board_t *board);
void    board_move_right(board_t *board);
void    board_move_up(board_t *board);
void    board_move_down(board_t *board);

#ifdef BOARD_IMPLEMENTATION
#undef BOARD_IMPLEMENTATION

unsigned int
_board_random_tile(board_t *board, int max_power) {
	int value = board->rand_int() % (max_power + 1);
	if (value == 0) return 0;
	value--;
	return (1 << value);
}

unsigned int
_boad_next_non_empty_column(board_t *board, int row, int first_column) {
	int index = first_column;
	int begin = row * board->size;
	for (;index<board->size;index++){
		if (board->tiles[begin+index] != 0) {
			break;
		}
	}
	return index;
}

void
_board_emit_sliding(board_t *board, tile_sliding_t sliding) {
	// No real sliding
	if (sliding.to.row == sliding.from.row && 
			sliding.to.column == sliding.from.column &&
			sliding.to.value == sliding.from.value) {
		return;
	}

	int temp;

	if (board->reversed) {
		sliding.to.column = board->size - sliding.to.column - 1;
		sliding.from.column = board->size - sliding.from.column - 1;
	}

	if (board->transposed) {
		temp = sliding.to.row;
		sliding.to.row    = sliding.to.column;
		sliding.to.column = temp;

		temp = sliding.from.row;
		sliding.from.row    = sliding.from.column;
		sliding.from.column = temp;
	}

	board->last_slide_movements++;

	if (board->on_sliding) board->on_sliding(sliding);
}

static inline void
_board_swap_tiles(board_t *board, int t1c, int t1r, int t2c, int t2r) {
	int i1 = (board->size * t1r + t1c);
	int i2 = (board->size * t2r + t2c);

	int temp         = board->tiles[i1];
	board->tiles[i1] = board->tiles[i2];
	board->tiles[i2] = temp;
}

void
_boad_transpose(board_t *board) {
	int bsize = board->size;
	for (int i=0;i<bsize;i++) {
		for (int j=0; j<i;j++) {
			_board_swap_tiles(board,j,i,i,j);
		}
	}

	board->transposed = !board->transposed;
}

void
_boad_reverse(board_t *board) {
	int bsize = board->size;
	for (int i=0; i<bsize;i++) {
		for (int j=0; j < bsize/2; j++) {
			_board_swap_tiles(board, j,i,(bsize-j-1),i);
		}
	}
	board->reversed = !board->reversed;
}

int
_board_count_empty(board_t *board) {
	int tiles = board->size * board->size;
	int count = 0;
	for (int i=0; i<tiles; i++) {
		if (board->tiles[i] <= 0) {
			count++;
		}
	}
	return count;
}

void
board_tile_position(board_t *self, int index, int *row, int *column) {
  *row = index / self->size;
  *column = (index - *row * self->size);
}

int
_board_decode_index(board_t *board, int index) {
	int row = index / 4;
	int column = (index - row * 4);
	if (board->reversed) {
		column = board->size - column - 1;
	}
	if (board->transposed) {
		int tmp = row;
		row = column;
		column = tmp;
	}
	return (row * 4 + column);
}

int
_board_add_number(board_t *board) {
	int empty_count = _board_count_empty(board);
	if (empty_count == 0) return -1;
	int to_add = board->rand_int() % empty_count;
	int number = (board->rand_int() % 100) > 50 ? 4 : 2;
	int tiles = board->size * board->size;
	int count = 0;

	int index = -1;

	for (int i=0; i<tiles; i++) {
		if (board->tiles[i] <= 0) {
			if (count == to_add) {
				board->tiles[i] = number;
				index = i;
				break;
			} else {
				count++;
			}
		}
	}
	return index;	
}

board_t 
board_create(board_rand_int_method rand_int) {
	board_t result = {0};
	result.rand_int = rand_int;
	result.size = 4;
	result.reversed = result.transposed = 0;
	_board_add_number(&result);
	_board_add_number(&result);
	return (result);
}

int 
board_tile_index(board_t *board, int column, int row) {
	int bsize = board->size;
	if (column >= bsize || row >= bsize) {
		return -1;
	}
	return (row * board->size + column);
}

int
_board_slide_row_left(board_t *board, int row_index) {
	unsigned int bsize = board->size;
	int score = 0;
	int current_index = 0;
	unsigned int first, second;
	first = _boad_next_non_empty_column(board,row_index,0);
	if (first == bsize) return 0;
	int *row = board->tiles + (row_index * board->size);
	tile_sliding_t tile_sliding;
	while (first < bsize) {
		second = _boad_next_non_empty_column(board,row_index, first+1);
		if (second == bsize || row[first] != row[second]) {
			tile_sliding.from.row    = row_index;
			tile_sliding.from.column = first;
			tile_sliding.from.value  = row[first];
			tile_sliding.to.row      = row_index;
			tile_sliding.to.column   = current_index;
			tile_sliding.to.value    = row[first];
			_board_emit_sliding(board, tile_sliding);
			row[current_index] = row[first];
			current_index++;
			first = second;
		} else {
			tile_sliding.from.row    = row_index;
			tile_sliding.from.column = first;
			tile_sliding.from.value  = row[first];

			tile_sliding.to.row      = row_index;
			tile_sliding.to.column   = current_index;
			tile_sliding.to.value    = row[first] * 2;

			_board_emit_sliding(board, tile_sliding);
			
			tile_sliding.from.row    = row_index;
			tile_sliding.from.column = second;
			tile_sliding.from.value  = row[second];

			_board_emit_sliding(board, tile_sliding);

			row[current_index] = 2 * row[first];
			current_index++;
			score += 2 * row[first];
			first = _boad_next_non_empty_column(board, row_index, second+1);
		}
	}

	for (;current_index < bsize; current_index++) {
		row[current_index] = 0;
	}

	return score;
}

int
_board_slide_left(board_t *board) {
	board->last_slide_movements = 0;
	int score = 0;
	for(int i=0; i<board->size; i++) {
		score += _board_slide_row_left(board,i);
	}
	if (board->last_slide_movements) {
		_board_add_number(board);
	}
	return score;
}

void
board_move_left(board_t *board) {
	_board_slide_left(board);
}

void 
board_move_right(board_t *board) {
	_boad_reverse(board);
	_board_slide_left(board);
	_boad_reverse(board);
}

void
board_move_up(board_t *board) {
	_boad_transpose(board);
	_board_slide_left(board);
	_boad_transpose(board);
}

void
board_move_down(board_t *board) {
	_boad_transpose(board);
	_boad_reverse(board);
	_board_slide_left(board);
	_boad_reverse(board);
	_boad_transpose(board);
}

#endif
#endif
