/*
** Copyright (C) 2002-2004 Erik de Castro Lopo <erikd@mega-nerd.com>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

/*
**	Utility functions to make writing the test suite easier.
**
**	The .c and .h files were generated automagically with Autogen from
**	the files utils.def and utils.tpl.
*/


#define SF_COUNT_TO_LONG(x)	((long) (x))
#define	ARRAY_LEN(x)		((int) (sizeof (x)) / (sizeof ((x) [0])))

#define	PIPE_INDEX(x)	((x) + 500)
#define	PIPE_TEST_LEN	12345

void	gen_windowed_sine (double *data, int len, double maximum) ;

void	check_file_hash_or_die	(const char *filename, unsigned int target_hash, int line_num) ;

void	print_test_name (const char *test, const char *filename) ;

/*
**	Functions for saving two vectors of data in an ascii text file which
**	can then be loaded into GNU octave for comparison.
*/

int	oct_save_short	(short *a, short *b, int len) ;
int	oct_save_int	(int *a, int *b, int len) ;
int	oct_save_float	(float *a, float *b, int len) ;
int	oct_save_double	(double *a, double *b, int len) ;


#ifdef SNDFILE_H

void 	dump_log_buffer (SNDFILE *file) ;
void 	check_log_buffer_or_die (SNDFILE *file, int line_num) ;
int 	string_in_log_buffer (SNDFILE *file, const char *s) ;
void	hexdump_file (const char * filename, sf_count_t offset, sf_count_t length) ;

SNDFILE *test_open_file_or_die
			(const char *filename, int mode, SF_INFO *sfinfo, int line_num) ;

void 	test_read_write_position_or_die
			(SNDFILE *file, int line_num, int pass, sf_count_t read_pos, sf_count_t write_pos) ;

void	test_seek_or_die
			(SNDFILE *file, sf_count_t offset, int whence, sf_count_t new_pos, int channels, int line_num) ;


void 	test_read_short_or_die
			(SNDFILE *file, int pass, short *test, sf_count_t items, int line_num) ;
void 	test_read_int_or_die
			(SNDFILE *file, int pass, int *test, sf_count_t items, int line_num) ;
void 	test_read_float_or_die
			(SNDFILE *file, int pass, float *test, sf_count_t items, int line_num) ;
void 	test_read_double_or_die
			(SNDFILE *file, int pass, double *test, sf_count_t items, int line_num) ;

void 	test_readf_short_or_die
			(SNDFILE *file, int pass, short *test, sf_count_t frames, int line_num) ;
void 	test_readf_int_or_die
			(SNDFILE *file, int pass, int *test, sf_count_t frames, int line_num) ;
void 	test_readf_float_or_die
			(SNDFILE *file, int pass, float *test, sf_count_t frames, int line_num) ;
void 	test_readf_double_or_die
			(SNDFILE *file, int pass, double *test, sf_count_t frames, int line_num) ;

void 	test_write_short_or_die
			(SNDFILE *file, int pass, short *test, sf_count_t items, int line_num) ;
void 	test_write_int_or_die
			(SNDFILE *file, int pass, int *test, sf_count_t items, int line_num) ;
void 	test_write_float_or_die
			(SNDFILE *file, int pass, float *test, sf_count_t items, int line_num) ;
void 	test_write_double_or_die
			(SNDFILE *file, int pass, double *test, sf_count_t items, int line_num) ;

void 	test_writef_short_or_die
			(SNDFILE *file, int pass, short *test, sf_count_t frames, int line_num) ;
void 	test_writef_int_or_die
			(SNDFILE *file, int pass, int *test, sf_count_t frames, int line_num) ;
void 	test_writef_float_or_die
			(SNDFILE *file, int pass, float *test, sf_count_t frames, int line_num) ;
void 	test_writef_double_or_die
			(SNDFILE *file, int pass, double *test, sf_count_t frames, int line_num) ;


#endif




