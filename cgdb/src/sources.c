/* sources.c:
 * ----------
 * 
 * Source file management routines for the GUI.  Provides the ability to
 * add files to the list, load files, and display within a curses window.
 * Files are buffered in memory when they are displayed, and held in
 * memory for the duration of execution.  If memory consumption becomes a
 * problem, this can be optimized to unload files which have not been
 * displayed recently, or only load portions of large files at once. (May
 * affect syntax hilighting.)
 *
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

/* System Includes */
#if HAVE_MATH_H
#include <math.h>
#endif

#if HAVE_STDIO_H
#include <stdio.h> 
#endif /* HAVE_STDIO_H */

#if HAVE_STDLIB_H 
#include <stdlib.h>
#endif  /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#endif  /* HAVE_UNISTD_H */

/* Local Includes */
#include "highlight.h"
#include "sources.h"
#include "cgdb.h"
#include "logo.h"
#include "sys_util.h"

int sources_syntax_on = 1;
int auto_source_reload = 0;

/* --------------- */
/* Local Functions */
/* --------------- */

/* verify_file_exists: Checks to see if a file exists
 * -------------------
 *
 * Return Value: 0 if does not exist, 1 if exists 
 */
static int verify_file_exists(const char *path) {
   struct stat st;

   /* Check for read permission of file, already exists */
   if ( stat( path, &st ) == -1 )
        return 0;

    return 1;
}

/* source_queue_init: Initializes the queue to be empty.
 * ------------------
 *
 *   sview:  The source viewer object
 */
void source_queue_init(struct sviewer *sview) {
    int i;
    
    for ( i = 0; i < QUEUE_SIZE; i++ )
        sview->queue[i] = NULL;
    sview->queue_index = 0;
    sview->queue_top_index = 0;
}

/* source_queue_remove: Removes a specific file from the queue
 * --------------------
 *
 *   sview:  The source viewer object
 */
void source_queue_remove(struct sviewer *sview, struct list_node *node) {
    int i, j;
    
    for ( i = 0; i < sview -> queue_top_index; i++ ) {
        if ( sview->queue[i] == node ) {    /* Found */
            sview->queue[i] = NULL;
            for ( j = i; j < sview -> queue_top_index; j++)
               sview->queue[j] = sview->queue[j + 1]; 

            for ( j = sview ->queue_top_index; j < QUEUE_SIZE - 1; j++)
               sview->queue[j] = NULL; 
            sview->queue_top_index--;
            break;
        }
    }
}

/* source_queue_push: Removes a specific file from the queue
 * --------------------
 *
 *   sview:  The source viewer object
 */
void source_queue_push(struct sviewer *sview, struct list_node *node) {
    int i = sview -> queue_top_index, j, k;

    /* If the file is already in the queue, just move it to the top */
    for ( j = 0; j < i; j++ ) {
        /* Found the file, swap the rest down and put it on top */
        if ( sview->queue[j] == node ) {
            for ( k = j; k < i; k++ )
               sview->queue[k] = sview->queue[k + 1]; 

            sview->queue[i - 1] = node;
            return;
        }
    }

    /* Add to the non-full queue */
    if ( i < QUEUE_SIZE ) {
        sview->queue[i] = node;    
        sview->queue_index = i;
        sview->queue_top_index++;
        for ( j = i + 1; j < QUEUE_SIZE; j++)
            sview->queue[j] = NULL;
    } else if ( i == QUEUE_SIZE - 1) { 
        for ( j = 1; j < QUEUE_SIZE - 1; j++)
            sview->queue[j - 1] = sview->queue[j];
        sview->queue[j] = node;
    }
}

void source_previous(struct sviewer *sview) {
    /* Only one file, don't switch */
    if ( sview->queue_index < 1 )
        return;

    sview->queue_index--;
    sview->cur = sview->queue[sview->queue_index];
}

void source_next(struct sviewer *sview) {
    /* Check if we are at the last file viewed */
    if ( sview->queue_index == QUEUE_SIZE - 1 || 
         sview->queue[sview->queue_index + 1] == NULL )
        return;

    sview->queue_index++;
    sview->cur = sview->queue[sview->queue_index];
}

/* get_relative_node:  Returns a pointer to the node that matches the 
 * ------------------  given relative path.
 
 *   lpath:  Full path to source file
 *
 * Return Value:  Pointer to the matching node, or NULL if not found.
 */
static struct list_node *get_relative_node(struct sviewer *sview, const char *lpath)
{
    struct list_node *cur;
    
    for (cur = sview->list_head; cur != NULL; cur = cur->next)
         if (strcmp(lpath, cur->lpath) == 0)
             return cur;
    
    return NULL; 
}

/* get_node:  Returns a pointer to the node that matches the given path.
 * ---------
 
 *   path:  Full path to source file
 *
 * Return Value:  Pointer to the matching node, or NULL if not found.
 */
static struct list_node *get_node(struct sviewer *sview, const char *path)
{
    struct list_node *cur;
    
    for (cur = sview->list_head; cur != NULL; cur = cur->next)
         if (strcmp(path, cur->path) == 0)
             return cur;
    
    return NULL; 
}

/**
 * Get's the timestamp of a particular file.
 *
 * \param path
 * The path to the file to get the timestamp of
 *
 * \param timestamp
 * The timestamp of the file, or 0 on error.
 * 
 * \return
 * 0 on success, -1 on error.
 */
static int get_timestamp ( const char *path, time_t *timestamp ) {
	struct stat s;
	int val;

	if ( !path )
		return -1;

	if ( !timestamp )
		return -1;

	*timestamp = 0;

	val = stat ( path, &s );

	if ( val ) /* Error on timestamp */
		return -1;

	*timestamp = s.st_mtime;

	return 0;
}

static int release_file_buffer ( struct buffer *buf ) {
	int i;

	/* Nothing to free */
	if ( !buf ) 
		return 0;

	for ( i = 0; i < buf->length; ++i ) {
		free ( buf->tlines[i] );
		buf->tlines[i] = NULL;
	}

	free ( buf->tlines );
	buf->tlines = NULL;
	buf->length = 0;
	buf->cur_line = NULL;	
	buf->max_width = 0;
    free ( buf->breakpts );
	buf->breakpts = NULL;

	return 0; 
}

/** 
 * Remove's the memory related to a file.
 *
 * \param node
 * The node who's file buffer data needs to be freed.
 *
 * \return
 * 0 on success, or -1 on error.
 */
static int release_file_memory ( struct list_node *node ) {
	if ( !node )
		return -1;
	
	/* Free the buffer */
	if ( release_file_buffer ( &node->buf ) == -1 )
		return -1;

	if ( release_file_buffer ( &node->orig_buf ) == -1 )
		return -1;

	return 0;
}

/* load_file:  Loads the file in the list_node into its memory buffer.
 * ----------
 *
 *   node:  The list node to work on
 *
 * Return Value:  Zero on success, non-zero on error.
 */
static int load_file(struct list_node *node)
{
    FILE *file;
    char line[MAX_LINE];
    int i;

    node->buf.length    = 0;
    node->buf.tlines    = NULL;
    node->buf.breakpts  = NULL;
    node->buf.cur_line  = NULL;
    node->buf.max_width = 0;

	/* Open file and save in original buffer.
	 * I am not sure if this should be done this way in the future.
	 * Maybe this data should be recieved from flex.
	 */
	node->orig_buf.length = 0;
	node->orig_buf.tlines = NULL;
    node->orig_buf.breakpts  = NULL;
    node->orig_buf.cur_line  = NULL;
	node->orig_buf.max_width = 0;

	/* Stat the file to get the timestamp */
	if ( get_timestamp ( node->path, &(node->last_modification) ) == -1 )
		return 2;

    if (!(file = fopen(node->path, "r")))
        return 1;

	while (!feof(file)){
		if (fgets(line, MAX_LINE, file)){
            int length = strlen ( line );
            if ( length > 0 ) {
                if (line[length-1] == '\n')
                    line[length-1] = 0;
                if (line[length-1] == '\r')
                    line[length-1] = 0;
            }
			if (strlen(line) > node->orig_buf.max_width)
				node->orig_buf.max_width = strlen(line);

			/* Inefficient - Reallocates memory at each line */
			node->orig_buf.length++;
			node->orig_buf.tlines   = realloc(node->orig_buf.tlines, 
										 sizeof(char *) * node->orig_buf.length);
			node->orig_buf.tlines[node->orig_buf.length-1] = strdup(line);
		}
	}

	fclose(file);

	node->language = tokenizer_get_default_file_type ( strrchr(node->path, '.') );
    
	/* Add the highlighted lines */
	if (has_colors())
        highlight(node);
	else {
		/* Just copy the lines from the original buffer if no highlighting 
		 * is possible */
		int i;
		node->buf.length = node->orig_buf.length;
		node->buf.max_width = node->orig_buf.max_width;
		node->buf.tlines = xmalloc ( sizeof ( char * ) * node->orig_buf.length );
		for ( i = 0; i < node->orig_buf.length; i++ )
			node->buf.tlines[i] = xstrdup ( node->orig_buf.tlines[i] );
	}

    /* Allocate the breakpoints array */
    node->buf.breakpts = malloc(sizeof(char) * node->buf.length);
    for (i = 0; i < node->buf.length; i++)
       node->buf.breakpts[i] = 0;

    return 0;
}

/* --------- */
/* Functions */
/* --------- */

/* Descriptive comments found in header file: sources.h */

struct sviewer *source_new(int pos_r, int pos_c, int height, int width)
{
    struct sviewer *rv;

    /* Allocate a new structure */
    if ((rv = malloc(sizeof(struct sviewer))) == NULL)
        return NULL;

    /* Initialize the structure */
    rv->win       = newwin(height, width, pos_r, pos_c);
    rv->cur       = NULL;
    rv->list_head = NULL;

    source_queue_init(rv);

    return rv;
}

int source_add(struct sviewer *sview, const char *path)
{
    struct list_node *new_node; 

    
    
    new_node = malloc(sizeof(struct list_node));
    new_node->path         = strdup(path);
    new_node->buf.length   = 0;
    new_node->buf.tlines    = NULL;    /* This signals an empty buffer */
    new_node->sel_line     = 0;
    new_node->sel_col      = 0;
    new_node->sel_col_rbeg = 0;
    new_node->sel_col_rend = 0;
    new_node->sel_rline    = 0;
    new_node->exe_line     = 0;
	new_node->last_modification = 0; /* No timestamp yet */

    if (sview->list_head == NULL){
        /* List is empty, this is the first node */
        new_node->next = NULL;
        sview->list_head = new_node;
    }
    else{
        /* Insert at the front of the list (easy) */
        new_node->next = sview->list_head;
        sview->list_head = new_node;
    }
    
    return 0;
}

int source_set_relative_path(
            struct sviewer *sview, 
            const char *path, 
            const char *lpath) {

    struct list_node *node = sview->list_head;

    while ( node != NULL ) {
        if ( strcmp(node->path, path ) == 0) {
            node->lpath = strdup(lpath);
            return 0;
        }

        node = node->next;
    } 

    return -1;
}

int source_del(struct sviewer *sview, const char *path)
{
    struct list_node *cur;
    struct list_node *prev = NULL;
    int i; 

    /* Find the target node */
    for (cur = sview->list_head; cur != NULL; cur = cur->next){
        if (strcmp(path, cur->path) == 0)
            break;
        prev = cur;
    }
    
    if (cur == NULL)
        return 1;      /* Node not found */

    source_queue_remove(sview, cur);

    /* Release file buffer, if one is in memory */   
    if (cur->buf.tlines){
        for (i = 0; i < cur->buf.length; i++) {
            free(cur->buf.tlines[i]);
            if ( cur->buf.cur_line ) {
                free(cur->buf.cur_line);
                cur->buf.cur_line = NULL;
            }
            free(cur->orig_buf.tlines[i]);
        }
    }
    free(cur->buf.tlines);
    free(cur->orig_buf.tlines);

    /* Release file name */
    free(cur->path);
        
    /* Remove link from list */
    if (cur == sview->list_head)
        sview->list_head = sview->list_head->next;
    else
        prev->next = cur->next;
        
    /* Free the node */
    free(cur);
    
    return 0;
}

int source_length(struct sviewer *sview, const char *path)
{
    struct list_node *cur = get_node(sview, path);

    if (!cur){
        /* Load the file if it's not already */
        if (!cur->buf.tlines && load_file(cur))
            return -1;
    }

    return cur->buf.length;
}

char *source_current_file(struct sviewer *sview, char *path) {
   if (sview == NULL || sview->cur == NULL)
      return NULL;

   strcpy(path, sview->cur->path);
   return path;
}

int source_display(struct sviewer *sview, int focus)
{
    char fmt[5];
    int width, height;
    int lwidth;
    int line;
    int i;

    /* Check that a file is loaded */
    if (sview->cur == NULL || sview->cur->buf.tlines == NULL){
        logo_display(sview->win);
        wrefresh(sview->win);
        return 0;
    }

    /* Make sure cursor is visible */
    if ( focus )
        curs_set(1);
    else
        curs_set(0);

    /* Initialize variables */
    getmaxyx(sview->win, height, width);

    /* Set starting line number (center source file if it's small enough) */
    if (sview->cur->buf.length < height)
        line = (sview->cur->buf.length - height) / 2;
    else{
        line = sview->cur->sel_line - height / 2;
        if (line > sview->cur->buf.length - height)
            line = sview->cur->buf.length - height;
        else if (line < 0)
            line = 0;
    }

    /* Print 'height' lines of the file, starting at 'line' */
    lwidth = (int) log10(sview->cur->buf.length)+1;
    sprintf(fmt, "%%%dd", lwidth);

    for (i = 0; i < height; i++, line++){
        wmove(sview->win, i, 0);
        if (has_colors()){
            /* Outside of file, just finish drawing the vertical line */
            if (line < 0 || line >= sview->cur->buf.length){
                int j;
                for (j = 1; j < lwidth; j++)
                    waddch(sview->win, ' ');
                waddch(sview->win, '~');
                if (focus)
                    wattron(sview->win, A_BOLD);
                waddch(sview->win, VERT_LINE);
                if (focus)
                    wattroff(sview->win, A_BOLD);
                for (j = 2+lwidth; j < width; j++)
                    waddch(sview->win, ' ');
            }
            /* Mark the current line with an arrow */
            else if (line == sview->cur->exe_line){
                switch(sview->cur->buf.breakpts[line]){
                    case 0:
                        wattron(sview->win, COLOR_PAIR(CGDB_COLOR_GREEN));
                        break;
                    case 1:
                        wattron(sview->win, COLOR_PAIR(CGDB_COLOR_RED));
                        break;
                    case 2:
                        wattron(sview->win, COLOR_PAIR(CGDB_COLOR_YELLOW));
                        break;
                }
                wattron(sview->win, A_BOLD);
                wprintw(sview->win, fmt, line+1);
                switch(sview->cur->buf.breakpts[line]){
                    case 1:
                        wattroff(sview->win, COLOR_PAIR(CGDB_COLOR_RED));
                        break;
                    case 2:
                        wattroff(sview->win, COLOR_PAIR(CGDB_COLOR_YELLOW));
                        break;
                }
                wattron(sview->win, COLOR_PAIR(CGDB_COLOR_GREEN));
                waddch(sview->win, ACS_LTEE);
                waddch(sview->win, '>');
                wattroff(sview->win, COLOR_PAIR(CGDB_COLOR_GREEN));
                wattroff(sview->win, A_BOLD);
				/* I know this is rediculous, it needs to be reimplemented */
				if ( sources_syntax_on ) {
					if ( line == sview->cur->sel_line && sview->cur->buf.cur_line != NULL )
						hl_wprintw(sview->win, sview->cur->buf.cur_line, width-lwidth-2, sview->cur->sel_col);
					else
						hl_wprintw(sview->win, sview->cur->buf.tlines[line], width-lwidth-2, sview->cur->sel_col);
				} else {
					if ( line == sview->cur->sel_line && sview->cur->buf.cur_line != NULL )
						hl_wprintw(sview->win, sview->cur->orig_buf.cur_line, width-lwidth-2, sview->cur->sel_col);
					else
						hl_wprintw(sview->win, sview->cur->orig_buf.tlines[line], width-lwidth-2, sview->cur->sel_col);
				}
            }
            /* Look for breakpoints */
            else if (sview->cur->buf.breakpts[line]){
                if (sview->cur->buf.breakpts[line] == 1)
                    wattron(sview->win, COLOR_PAIR(CGDB_COLOR_RED));
                else
                    wattron(sview->win, COLOR_PAIR(CGDB_COLOR_YELLOW));
                wattron(sview->win, A_BOLD);
                wprintw(sview->win, fmt, line+1);
                if (sview->cur->buf.breakpts[line] == 1)
                    wattroff(sview->win, COLOR_PAIR(CGDB_COLOR_RED));
                else
                    wattroff(sview->win, COLOR_PAIR(CGDB_COLOR_YELLOW));
                if (!focus)
                    wattroff(sview->win, A_BOLD);
                waddch(sview->win, VERT_LINE);
                if (focus)
                    wattroff(sview->win, A_BOLD);
                waddch(sview->win, ' ');

				/* I know this is rediculous, it needs to be reimplemented */
				if ( sources_syntax_on ) {
					if ( line == sview->cur->sel_line && sview->cur->buf.cur_line != NULL )
						hl_wprintw(sview->win, sview->cur->buf.cur_line, width-lwidth-2, sview->cur->sel_col);
					else
						hl_wprintw(sview->win, sview->cur->buf.tlines[line], width-lwidth-2, sview->cur->sel_col);
				} else {
					if ( line == sview->cur->sel_line && sview->cur->buf.cur_line != NULL )
						hl_wprintw(sview->win, sview->cur->orig_buf.cur_line, width-lwidth-2, sview->cur->sel_col);
					else
						hl_wprintw(sview->win, sview->cur->orig_buf.tlines[line], width-lwidth-2, sview->cur->sel_col);
				}
            }
            /* Ordinary lines */
            else{
                if ( focus && sview->cur->sel_line == line )
                    wattron(sview->win, A_BOLD);
                    
                wprintw(sview->win, fmt, line+1);
                
                if ( focus && sview->cur->sel_line == line )
                    wattroff(sview->win, A_BOLD);

                if (focus)
                    wattron(sview->win, A_BOLD);
                waddch(sview->win, VERT_LINE);
                if (focus)
                    wattroff(sview->win, A_BOLD);
                waddch(sview->win, ' ');
                
				/* I know this is rediculous, it needs to be reimplemented */
				if ( sources_syntax_on ) {
					/* No special line information */
					if ( line == sview->cur->sel_line && sview->cur->buf.cur_line != NULL )
						hl_wprintw(sview->win, sview->cur->buf.cur_line, width-lwidth-2, sview->cur->sel_col);
					else
						hl_wprintw(sview->win, sview->cur->buf.tlines[line], width-lwidth-2, sview->cur->sel_col);
				} else {
					/* No special line information */
					if ( line == sview->cur->sel_line && sview->cur->buf.cur_line != NULL )
						hl_wprintw(sview->win, sview->cur->orig_buf.cur_line, width-lwidth-2, sview->cur->sel_col);
					else
						hl_wprintw(sview->win, sview->cur->orig_buf.tlines[line], width-lwidth-2, sview->cur->sel_col);
				}
            }
        }
        else{
            wprintw(sview->win, "%s\n", sview->cur->buf.tlines[line]);
        }
    }
    
    wmove(sview->win, height - (line - sview->cur->sel_line) , lwidth+2);
    wrefresh(sview->win);

    return 0;
}

void source_move(struct sviewer *sview,
                 int pos_r, int pos_c, int height, int width)
{
    delwin(sview->win);
    sview->win = newwin(height, width, pos_r, pos_c);
    wclear(sview->win);
}

void source_vscroll(struct sviewer *sview, int offset)
{
    if (sview->cur){
        sview->cur->sel_line += offset;
        if (sview->cur->sel_line < 0)
            sview->cur->sel_line = 0;
        if (sview->cur->sel_line >= sview->cur->buf.length)
            sview->cur->sel_line = sview->cur->buf.length - 1;

        sview->cur->sel_rline = sview->cur->sel_line;
    }
}

void source_hscroll(struct sviewer *sview, int offset)
{
    int lwidth;
    int max_width;

    if (sview->cur){
        lwidth    = (int) log10(sview->cur->buf.length)+1;
        max_width = sview->cur->buf.max_width - COLS + lwidth + 6;
        
        sview->cur->sel_col += offset;
        if (sview->cur->sel_col > max_width)
            sview->cur->sel_col = max_width;
        if (sview->cur->sel_col < 0)
            sview->cur->sel_col = 0;
    }
}

void source_set_sel_line(struct sviewer *sview, int line)
{
    if (sview->cur){
        /* Set line (note correction for 0-based line counting) */
        sview->cur->sel_line = line-1;
        if (sview->cur->sel_line < 0)
            sview->cur->sel_line = 0;
        if (sview->cur->sel_line >= sview->cur->buf.length)
            sview->cur->sel_line = sview->cur->buf.length - 1;

        sview->cur->sel_rline = sview->cur->sel_line;
    }
}

int source_set_exec_line(struct sviewer *sview, const char *path, int line)
{
    if ( path && !verify_file_exists(path) )
        return 5;

    /* Locate node, if path has changed */
    if (path != NULL && !(sview->cur = get_node(sview, path))){
        /* Not found -- attempt to add */
        if (source_add(sview, path))
            return 1;
        else if (!(sview->cur = get_node(sview, path)))
            return 2;
    }
    else if (path == NULL && sview->cur == NULL)
        return 3;

    /* Buffer the file if it's not already */
    if (!sview->cur->buf.tlines && load_file(sview->cur))
        return 4;

    /* Push file on queue */
    source_queue_push(sview, sview->cur);

    /* Update line, if set */
    if (line--){
        /* Check bounds of line */
        if (line < 0)
            line = 0;
        if (line >= sview->cur->buf.length)
            line = sview->cur->buf.length - 1;
        sview->cur->sel_line = sview->cur->exe_line = line;
    }

    return 0;
}

void source_free(struct sviewer *sview)
{
    /* Free all file buffers */
    while (sview->list_head != NULL)
        source_del(sview, sview->list_head->path);

    source_queue_init(sview);

    delwin(sview->win);
}

void source_search_regex_init(struct sviewer *sview) {
    if ( sview == NULL || sview->cur == NULL)
        return;

    /* Start from beginning of line if not at same line */
    if ( sview->cur->sel_rline != sview->cur->sel_line ) {
        sview->cur->sel_col_rend = 0;
        sview->cur->sel_col_rbeg = 0;
    }
    
    /* Start searching at the beginning of the selected line */
    sview->cur->sel_rline    = sview->cur->sel_line;
}

int source_search_regex(struct sviewer *sview, 
                        const char *regex, int opt,
                        int direction, int icase) {

    if ( sview == NULL || sview->cur == NULL || regex == NULL ||
         strlen(regex) == 0 )
        return -1;

    return hl_regex(regex, 
                    (const char ** )sview->cur->buf.tlines, 
                    (const char ** )sview->cur->orig_buf.tlines, 
                    sview->cur->orig_buf.length, 
                    &sview->cur->buf.cur_line, &sview->cur->sel_line,
                    &sview->cur->sel_rline, &sview->cur->sel_col_rbeg, 
                    &sview->cur->sel_col_rend, 
                    opt, direction, icase);
}

void source_disable_break(struct sviewer *sview, const char *path, int line)
{
    struct list_node *node;

    if ((node = get_relative_node(sview, path)) == NULL)
        return;

    if (node->buf.tlines == NULL)
        if (load_file(node))
            return;
    
    if (line > 0 && line <= node->buf.length)
        node->buf.breakpts[line-1] = 2;
}

void source_enable_break(struct sviewer *sview, const char *path, int line)
{
    struct list_node *node;

    if ((node = get_relative_node(sview, path)) == NULL)
        return;

    if (node->buf.tlines == NULL)
        if (load_file(node))
            return;
    
    if (line > 0 && line <= node->buf.length){
        node->buf.breakpts[line-1] = 1;
    }
}

void source_clear_breaks(struct sviewer *sview)
{
    struct list_node *node;

    for (node = sview->list_head; node != NULL; node = node->next)
        memset(node->buf.breakpts, 0, node->buf.length);
        
}

int source_reload ( struct sviewer *sview, const char *path, int force ) {
	time_t timestamp;
    struct list_node *cur;
    struct list_node *prev = NULL;

	if ( !path )
		return -1;

	if ( get_timestamp ( path, &timestamp ) == -1 )
		return -1;

	/* Find the target node */
	for (cur = sview->list_head; cur != NULL; cur = cur->next){
		if (strcmp(path, cur->path) == 0)
			break;
		prev = cur;
	}
	
	if (cur == NULL)
		return 1;      /* Node not found */

	if ( ( auto_source_reload || force ) && cur->last_modification < timestamp ) {

		if ( release_file_memory ( sview->cur ) == -1)
			return -1;

		if ( load_file ( cur ) )
			return -1;
	}

	return 0;
}
