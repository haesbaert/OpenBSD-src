/*	$OpenBSD: ops.pro,v 1.1 1996/09/07 21:40:29 downsj Exp $	*/
/* ops.c */
void do_shift __PARMS((int op, int curs_top, int amount));
void shift_line __PARMS((int left, int round, int amount));
void do_reindent __PARMS((int (*how)(void)));
int is_yank_buffer __PARMS((int c, int writing));
int yank_buffer_mline __PARMS((void));
int do_record __PARMS((int c));
int do_execbuf __PARMS((int c, int colon, int addcr));
int insertbuf __PARMS((int c));
int cmdline_paste __PARMS((int c));
void do_delete __PARMS((void));
void do_tilde __PARMS((void));
void swapchar __PARMS((FPOS *pos));
int do_change __PARMS((void));
void init_yank __PARMS((void));
int do_yank __PARMS((int deleting, int mess));
void do_put __PARMS((int dir, long count, int fix_indent));
int get_register_name __PARMS((int num));
void do_dis __PARMS((char_u *arg));
void dis_msg __PARMS((char_u *p, int skip_esc));
void do_do_join __PARMS((long count, int insert_space, int redraw));
int do_join __PARMS((int insert_space, int redraw));
void do_format __PARMS((void));
int do_addsub __PARMS((int command, linenr_t Prenum1));
int read_viminfo_register __PARMS((char_u *line, FILE *fp, int force));
void write_viminfo_registers __PARMS((FILE *fp));
void gui_free_selection __PARMS((void));
void gui_get_selection __PARMS((void));
void gui_yank_selection __PARMS((int type, char_u *str, long_u len));
int gui_convert_selection __PARMS((char_u **str, long_u *len));
