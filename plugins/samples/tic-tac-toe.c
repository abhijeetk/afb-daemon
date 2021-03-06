/*
 * Copyright (C) 2016 "IoT.bzh"
 * Author José Bollo <jose.bollo@iot.bzh>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <json-c/json.h>

#include <afb/afb-plugin.h>

/*
 * the interface to afb-daemon
 */
const struct AFB_interface *afbitf;

/*
 * definition of waiters
 */
struct waiter
{
	struct waiter *next;
	struct afb_req req;
};

/*
 * definition of a board
 */
struct board
{
	struct board *next;
	int use_count;
	int moves;
	int history[9];
	int id;
	int level;
	char board[9];
	struct waiter *waiters;
};

/*
 * list of boards
 */
static struct board *all_boards;

/*
 * Searchs a board having the 'id'.
 * Returns it if found or NULL otherwise.
 */
static struct board *search_board(int id)
{
	struct board *board = all_boards;
	while (board != NULL && board->id != id)
		board = board->next;
	return board;
}

/*
 * Creates a new board and returns it.
 */
static struct board *get_new_board()
{
	/* allocation */
	struct board *board = calloc(1, sizeof *board);

	/* initialisation */
	memset(board->board, ' ', sizeof board->board);
	board->use_count = 1;
	board->level = 1;
	board->moves = 0;
	do {
		board->id = (rand() >> 2) % 1000;
	} while(board->id == 0 || search_board(board->id) != NULL);

	/* link */
	board->next = all_boards;
	all_boards = board;
	return board;
}

/*
 * Release a board
 */
static void release_board(struct board *board)
{
	/* decrease the reference count ... */
	if (--board->use_count == 0) {
		/* ... no more use */
		/* unlink from the list of boards */
		struct board **prv = &all_boards;
		while (*prv != NULL && *prv != board)
			prv = &(*prv)->next;
		if (*prv != NULL)
			*prv = board->next;
		/* release the used memory */
		free(board);
	}
}

/*
 * Checks who wins
 * Returns zero if there is no winner
 * Returns the char of the winner if a player won
 */
static char winner(const char b[9])
{
	int i;
	char c;

	/* check diagonals */
	c = b[4];
	if (c != ' ') {
		if (b[0] == c && b[8] == c)
			return c;
		if (b[2] == c && b[6] == c)
			return c;
	}

	/* check lines */
	for (i = 0 ; i <= 6 ; i += 3) {
		c = b[i];
		if (c != ' ' && b[i+1] == c && b[i+2] == c)
			return c;
	}

	/* check columns */
	for (i = 0 ; i <= 2 ; i++) {
		c = b[i];
		if (c != ' ' && b[i+3] == c && b[i+6] == c)
			return c;
	}

	return 0;
}

/* get the color (X or 0) of the move of index 'move' */
static char color(int move)
{
	return (move & 1) == 0 ? 'X' : '0';
}

/* adds the move to the board */
static void add_move(struct board *board, int index)
{
	int imove = board->moves++;
	board->history[imove] = index;
	board->board[index] = color(imove);
}

/* get a random possible move index from the board described by 'b' */
static int get_random_move(char b[9])
{
	int index = rand() % 9;
	while (b[index] != ' ')
		index = (index + 1) % 9;
	return index;
}

/*
 * Scores the position described by 'b'
 * for the player of color 'c' using an analysis of 'depth'.
 * Returns 1 if player 'c' will win.
 * Returns -1 if opponent of player 'c' will win.
 * returns 0 otherwise.
 */
static int score_position(char b[9], char c, int depth)
{
	int i, t, r;

	/* check if winner */
	if (winner(b) == c)
		return 1;

	/* when depth of analysis is reached return unknown case */
	if (--depth == 0)
		return 0;

	/* switch to the opponent */
	c = (char)('O' + 'X' - c);

	/* inspect opponent moves */
	r = 1;
	for (i = 0 ; i < 9 ; i++) {
		if (b[i] == ' ') {
			b[i] = c;
			t = score_position(b, c, depth);
			b[i] = ' ';
			if (t > 0)
				return -1; /* opponent will win */

			if (t == 0)
				r = 0; /* something not clear */
		}
	}
	return r;
}

/* get one move: return the computed index of the move */
static int get_move(struct board *board)
{
	int index, depth, t, f;
	char c;
	char b[9];

	/* compute the depth */
	depth = board->level - 1;
	if (board->moves + depth > 9)
		depth = 9 - board->moves;

	/* case of null depth */
	if (depth == 0)
		return get_random_move(board->board);

	/* depth and more */
	memcpy(b, board->board, 9);
	c = color(board->moves);
	f = 0;
	for (index = 0 ; index < 9 ; index++) {
		if (board->board[index] == ' ') {
			board->board[index] = c;
			t = score_position(board->board, c, depth);
			board->board[index] = ' ';
			if (t > 0)
				return index;
			if (t < 0)
				b[index] = '+';
			else
				f = 1;
		}
	}
	return get_random_move(f ? b : board->board);
}

/*
 * get the board description
 */
static struct json_object *describe(struct board *board)
{
	int i;
	char w;
	struct json_object *resu, *arr;

	resu = json_object_new_object();

	json_object_object_add(resu, "boardid", json_object_new_int(board->id));
	json_object_object_add(resu, "level", json_object_new_int(board->level));

	arr = json_object_new_array();
	json_object_object_add(resu, "board", arr);
	for (i = 0 ; i < 9 ; i++)
		json_object_array_add(arr,
				json_object_new_string_len(&board->board[i], 1));

	arr = json_object_new_array();
	json_object_object_add(resu, "history", arr);
	for (i = 0 ; i < board->moves ; i++)
		json_object_array_add(arr, json_object_new_int(board->history[i]));

	w = winner(board->board);
	if (w)
		json_object_object_add(resu, "winner", json_object_new_string_len(&w, 1));
	else if (board->moves == 9)
		json_object_object_add(resu, "winner", json_object_new_string("none"));

	return resu;
}

/*
 * signals a change of the board
 */
static void changed(struct board *board, const char *reason)
{
	struct waiter *waiter, *next;
	struct json_object *description;

	/* get the description */
	description = describe(board);

	waiter = board->waiters;
	board->waiters = NULL;
	while (waiter != NULL) {
		next = waiter->next;
		afb_req_success(waiter->req, json_object_get(description), reason);
		afb_req_unref(waiter->req);
		free(waiter);
		waiter = next;
	}

	afb_daemon_broadcast_event(afbitf->daemon, reason, description);
}

/*
 * retrieves the board of the request
 */
static inline struct board *board_of_req(struct afb_req req)
{
	return afb_req_context(req, (void*)get_new_board, (void*)release_board);
}

/*
 * start a new game
 */
static void new(struct afb_req req)
{
	struct board *board;

	/* retrieves the context for the session */
	board = board_of_req(req);
	INFO(afbitf, "method 'new' called for boardid %d", board->id);

	/* reset the game */
	memset(board->board, ' ', sizeof board->board);
	board->moves = 0;

	/* replies */
	afb_req_success(req, NULL, NULL);

	/* signal change */
	changed(board, "new");
}

/*
 * get the board
 */
static void board(struct afb_req req)
{
	struct board *board;
	struct json_object *description;

	/* retrieves the context for the session */
	board = board_of_req(req);
	INFO(afbitf, "method 'board' called for boardid %d", board->id);

	/* describe the board */
	description = describe(board);

	/* send the board's description */
	afb_req_success(req, description, NULL);
}

/*
 * move a piece
 */
static void move(struct afb_req req)
{
	struct board *board;
	int i;
	const char *index;

	/* retrieves the context for the session */
	board = board_of_req(req);
	INFO(afbitf, "method 'move' called for boardid %d", board->id);

	/* retrieves the arguments of the move */
	index = afb_req_value(req, "index");
	i = index == NULL ? -1 : atoi(index);

	/* checks validity of arguments */
	if (i < 0 || i > 8) {
		WARNING(afbitf, "can't move to %s: %s", index?:"?", index?"wrong value":"not set");
		afb_req_fail(req, "error", "bad request");
		return;
	}

	/* checks validity of the state */
	if (winner(board->board) != 0) {
		WARNING(afbitf, "can't move to %s: game is terminated", index);
		afb_req_fail(req, "error", "game terminated");
		return;
	}

	/* checks validity of the move */
	if (board->board[i] != ' ') {
		WARNING(afbitf, "can't move to %s: room occupied", index);
		afb_req_fail(req, "error", "occupied");
		return;
	}

	/* applies the move */
	INFO(afbitf, "method 'move' for boardid %d, index=%s", board->id, index);
	add_move(board, i);

	/* replies */
	afb_req_success(req, NULL, NULL);

	/* signals change */
	changed(board, "move");
}

/*
 * set the level
 */
static void level(struct afb_req req)
{
	struct board *board;
	int l;
	const char *level;

	/* retrieves the context for the session */
	board = board_of_req(req);
	INFO(afbitf, "method 'level' called for boardid %d", board->id);

	/* retrieves the arguments */
	level = afb_req_value(req, "level");
	l = level == NULL ? -1 : atoi(level);

	/* check validity of arguments */
	if (l < 1 || l > 10) {
		WARNING(afbitf, "can't set level to %s: %s", level?:"?", level?"wrong value":"not set");
		afb_req_fail(req, "error", "bad request");
		return;
	}

	/* set the level */
	INFO(afbitf, "method 'level' for boardid %d, level=%d", board->id, l);
	board->level = l;

	/* replies */
	afb_req_success(req, NULL, NULL);

	/* signals change */
	changed(board, "level");
}

/*
 * Join a board
 */
static void join(struct afb_req req)
{
	struct board *board, *new_board;
	const char *id;

	/* retrieves the context for the session */
	board = board_of_req(req);
	INFO(afbitf, "method 'join' called for boardid %d", board->id);

	/* retrieves the arguments */
	id = afb_req_value(req, "boardid");
	if (id == NULL)
		goto bad_request;

	/* none is a special id for joining a new session */
	if (strcmp(id, "none") == 0) {
		new_board = get_new_board();
		goto success;
	}

	/* searchs the board to join */
	new_board = search_board(atoi(id));
	if (new_board == NULL)
		goto bad_request;

	/*
	 * joining its board is stupid but possible
	 * however when called with the same stored pointer
	 * afb_req_context_set will not call the release
	 * function 'release_board'. So the use_count MUST not
	 * be incremented.
	 */
	if (new_board != board)
		new_board->use_count++;

success:
	/* set the new board (and leaves the previous one) */
	afb_req_context_set(req, new_board, (void*)release_board);

	/* replies */
	afb_req_success(req, NULL, NULL);
	return;

bad_request:
	WARNING(afbitf, "can't join boardid %s: %s", id ? : "?", !id ? "no boardid" : atoi(id) ? "not found" : "bad boardid");
	afb_req_fail(req, "error", "bad request");
	return;
}

/*
 * Undo the last move
 */
static void undo(struct afb_req req)
{
	struct board *board;
	int i;

	/* retrieves the context for the session */
	board = board_of_req(req);
	INFO(afbitf, "method 'undo' called for boardid %d", board->id);

	/* checks the state */
	if (board->moves == 0) {
		WARNING(afbitf, "can't undo");
		afb_req_fail(req, "error", "bad request");
		return;
	}

	/* undo the last move */
	i = board->history[--board->moves];
	board->board[i] = ' ';

	/* replies */
	afb_req_success(req, NULL, NULL);

	/* signals change */
	changed(board, "undo");
}

/*
 * computer plays
 */
static void play(struct afb_req req)
{
	struct board *board;
	int index;

	/* retrieves the context for the session */
	board = board_of_req(req);
	INFO(afbitf, "method 'play' called for boardid %d", board->id);

	/* checks validity of the state */
	if (winner(board->board) != 0 || board->moves == 9) {
		WARNING(afbitf, "can't play: game terminated (%s)", winner(board->board) ? "has winner" : "no room left");
		afb_req_fail(req, "error", "game terminated");
		return;
	}

	/* gets the move and plays it */
	index = get_move(board);
	add_move(board, index);

	/* replies */
	afb_req_success(req, describe(board), NULL);

	/* signals change */
	changed(board, "play");
}

static void wait(struct afb_req req)
{
	struct board *board;
	struct waiter *waiter;

	/* retrieves the context for the session */
	board = board_of_req(req);
	INFO(afbitf, "method 'wait' called for boardid %d", board->id);

	/* creates the waiter and enqueues it */
	waiter = calloc(1, sizeof *waiter);
	waiter->req = req;
	waiter->next = board->waiters;
	afb_req_addref(req);
	board->waiters = waiter;
}

/*
 * array of the verbs exported to afb-daemon
 */
static const struct AFB_verb_desc_v1 plugin_verbs[] = {
   /* VERB'S NAME     SESSION MANAGEMENT          FUNCTION TO CALL  SHORT DESCRIPTION */
   { .name= "new",   .session= AFB_SESSION_NONE, .callback= new,   .info= "Starts a new game" },
   { .name= "play",  .session= AFB_SESSION_NONE, .callback= play,  .info= "Asks the server to play" },
   { .name= "move",  .session= AFB_SESSION_NONE, .callback= move,  .info= "Tells the client move" },
   { .name= "board", .session= AFB_SESSION_NONE, .callback= board, .info= "Get the current board" },
   { .name= "level", .session= AFB_SESSION_NONE, .callback= level, .info= "Set the server level" },
   { .name= "join",  .session= AFB_SESSION_CHECK,.callback= join,  .info= "Join a board" },
   { .name= "undo",  .session= AFB_SESSION_NONE, .callback= undo,  .info= "Undo the last move" },
   { .name= "wait",  .session= AFB_SESSION_NONE, .callback= wait,  .info= "Wait for a change" },
   { .name= NULL } /* marker for end of the array */
};

/*
 * description of the plugin for afb-daemon
 */
static const struct AFB_plugin plugin_description =
{
   /* description conforms to VERSION 1 */
   .type= AFB_PLUGIN_VERSION_1,
   .v1= {				/* fills the v1 field of the union when AFB_PLUGIN_VERSION_1 */
      .prefix= "tictactoe",		/* the API name (or plugin name or prefix) */
      .info= "Sample tac-tac-toe game",	/* short description of of the plugin */
      .verbs = plugin_verbs		/* the array describing the verbs of the API */
   }
};

/*
 * activation function for registering the plugin called by afb-daemon
 */
const struct AFB_plugin *pluginAfbV1Register(const struct AFB_interface *itf)
{
   afbitf = itf;         // records the interface for accessing afb-daemon
   return &plugin_description;  // returns the description of the plugin
}

