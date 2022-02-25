/* Multi-User Chat Client
 * Copyright(C) 2012 y2c2 */

/* Uses TCP protocol for data transmission
 * On multi-threading supporting side, uses pthread for UNIX and
 * Win32 threading for Windows */

/* Graphics User Interface inclueded implemented with GTK+ */

#define SHORT_BLOCK
#define TIME_OUT_TIME 15

/* base */
#include <string.h>
#include <unistd.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>

#include <sys/time.h>

/* network */
#if defined(UNIX)
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/ioctl.h>
#elif defined(WINDOWS)
#include <Winsock2.h>
#define bzero(p, len) memset((p), 0, (len))
#else
#error "Operation System type not defined"
#endif

/* multi-threading */
#if defined(UNIX)
#include <pthread.h>
#elif defined(WINDOWS)
#include <process.h>
#endif

/* GUI */
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

/* icon */
#include "chat.xpm"

/* global constant */
/*#define SERVER_ADDR "127.0.0.1"*/
#define SERVER_PORT 8089
#define BUFFER_SIZE 4096

#define EXIT_STATE_MANUAL 0
#define EXIT_STATE_SERVER_DISCONNECTED 1

/* server commands
 * WARNING: first byte of every message is the command number */
enum {
    CMD_NULL = 0,
    CMD_SET_NICKNAME = 1, /* cmd, name */
    CMD_SEND_MSG = 2, /* cmd, msg */
    CMD_RECV_MSG = 3, /* cmd, u8 name_len, name, u8 msg_len, msg */
};

/* global variables */
int sockfd;
int exit_state;

/*
 *********************************
 * MESSAGE DIALOG & ERROR PROMPT *
 *********************************
 */

/* return response_id of messagebox */
static void messagebox_respond_callback(GtkDialog *dialog, gint response_id, gpointer data)
{
	int *ret_p = (int *)data;
	*ret_p = response_id;
}

/* message box */
static int messagebox(GtkWindow *parent, GtkDialogFlags flags, GtkMessageType type, GtkButtonsType buttons, const char *title, char *msg_fmt, ...)
{
	int ret;
	char buf[1024];
	va_list args;
	va_start(args, msg_fmt);
	vsprintf(buf, msg_fmt, args);
	va_end(args);
	GtkWidget *dialog;
	g_usleep(1);
	gdk_threads_enter();
	dialog = gtk_message_dialog_new(parent, flags, type, buttons, "%s", buf);
	gtk_window_set_title(GTK_WINDOW(dialog), title);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(messagebox_respond_callback), &ret);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	gdk_threads_leave();
	return ret;
}

/* fatal error occurred, 
 * print the error message and exit server program */
void fatal_error(char *msg)
{
	messagebox(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Fatal Error", "%s", msg);
	exit(1);
}

/*
 ********************
 * CHAT MAIN WINDOW *
 ********************
 */

/* Widgets */

GtkWidget *window;

GtkWidget *menu_bar;
GtkWidget *menu_conversation;
GtkWidget *menu_tools;
GtkWidget *menu_help;
GtkWidget *menu_item_conversation;
GtkWidget *menu_item_conversation_save_log;
GtkWidget *menu_item_conversation_clear_log;
GtkWidget *menu_item_conversation_sep1;
GtkWidget *menu_item_conversation_exit;
GtkWidget *menu_item_tools;
GtkWidget *check_menu_item_tools_always_on_top;
GtkWidget *menu_item_help;
GtkWidget *menu_item_help_about;

GtkWidget *text_view;
GtkWidget *scrolled_window;

GtkWidget *entry_msg;
GtkWidget *button_send;
GtkWidget *hbox;

GtkWidget *vbox;

/* callbacks */
static void destroy(GtkWidget *window, gpointer *data)
{
	gtk_main_quit();
}

static gboolean delete_event(GtkWidget *window, GdkEvent *event, gpointer data)
{
	return FALSE;
}

int save_file(char *filename, char *str, unsigned int size)
{
	FILE *fp;
	fp = fopen(filename, "wb+");
	if (fp == NULL) return -1;
	char *str_p = str;
	int remain_size = size;
	int job;
	while (remain_size > 0)
	{
		job = remain_size > 4096 ? 4096 : remain_size;
		fwrite(str_p, job, 1, fp);
		str_p += job;
		remain_size -= job;
	}
	fclose(fp);
	return 0;
}

static void menu_item_conversation_save_log_callback(GtkWidget *widget, gpointer *data)
{
	GtkWidget *dialog;
	dialog = gtk_file_chooser_dialog_new("Save As", GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_SAVE,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
			NULL);
	gtk_dialog_set_default_response(GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	/*g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(messagebox_respond_callback), &ret);*/
	int ret;
	ret = gtk_dialog_run(GTK_DIALOG(dialog));
	if (ret == GTK_RESPONSE_ACCEPT)
	{
		char *filename;
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		printf("%s\n", filename);

		/* get text from textview */
		char *text_p;
		int size;
		GtkTextBuffer *buffer;
		buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
		GtkTextIter iter1, iter2;
		gtk_text_buffer_get_start_iter(buffer, &iter1);
		gtk_text_buffer_get_end_iter(buffer, &iter2);
		size = gtk_text_buffer_get_char_count(buffer);
		text_p = gtk_text_buffer_get_text(buffer, &iter1, &iter2, TRUE);
		/*printf("%s\n", text_p);*/
		save_file(filename, text_p, size);
		g_free(filename);
	}
	gtk_widget_destroy(dialog);
	/*gtk_main_quit();*/
}

static void menu_item_conversation_clear_log_callback(GtkWidget *widget, gpointer *data)
{
	GtkTextBuffer *buffer;
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
	gtk_text_buffer_set_text(buffer, "", 0);
}

static void menu_item_conversation_exit_callback(GtkWidget *widget, gpointer *data)
{
	gtk_main_quit();
}

static void check_menu_item_tools_always_on_top_callback(GtkWidget *widget, gpointer *data)
{
	int *state_always_on_top = (int *)data;
	*state_always_on_top = !(*state_always_on_top);
	gtk_window_set_keep_above(GTK_WINDOW(window), *state_always_on_top);
}

static void menu_item_help_about_callback(GtkWidget *widget, gpointer *data)
{
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_xpm_data((const char **)chat_xpm);

	GtkWidget *dialog = gtk_about_dialog_new();
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_about_dialog_set_name(GTK_ABOUT_DIALOG(dialog), "Chat++");
	gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), "0.0.1"); 
	gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(dialog), 
			"Copyright(c) 2012 jati");
	gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dialog), 
			"A simple chat client program.");
	/*gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(dialog), */
	/*"http://www.batteryhq.net");*/
	gtk_about_dialog_set_logo(GTK_ABOUT_DIALOG(dialog), pixbuf);
	g_object_unref(pixbuf), pixbuf = NULL;
	gtk_dialog_run(GTK_DIALOG (dialog));
	gtk_widget_destroy(dialog);
}

static void button_send_callback(GtkWidget *widget, gpointer *data)
{
	/* get text from entry widget */
	GtkWidget *entry_msg = (GtkWidget *)data;
	const char *msg_p = gtk_entry_get_text(GTK_ENTRY(entry_msg));

	/* copy and send text */
	char send_buf[BUFFER_SIZE];
	char *send_buf_msg_cmd = send_buf;
	char *send_buf_msg_body = send_buf + 1;
	size_t send_len;
	*send_buf_msg_cmd = CMD_SEND_MSG;
	strncpy(send_buf_msg_body, msg_p, strlen(msg_p));
	send_len = strlen(msg_p) + 1;
	if (send(sockfd, (char *)&send_buf, send_len, 0) == -1)
	{

	}
	/* focus */
	gtk_widget_grab_focus(entry_msg);
	/* clean entry */
	gtk_entry_set_text(GTK_ENTRY(entry_msg), "");
}

int entry_msg_key_press_callback(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event->keyval == GDK_Return)
	{
		button_send_callback(widget, data);
	}
	return FALSE;
}

/* create char main window and enter the main loop */
int chat(void)
{
	/* state variable */
	int state_always_on_top = 0;

	/* create window */
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	/* icon */
	GdkPixbuf *pixbuf;
	pixbuf = gdk_pixbuf_new_from_xpm_data((const char **)chat_xpm);
	gtk_window_set_icon(GTK_WINDOW(window), pixbuf);
	gdk_threads_enter();

	/* window setting */
	gtk_window_set_title(GTK_WINDOW(window), "Chat++");
	gtk_widget_set_size_request(window, 400, 320);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(destroy), NULL);
	g_signal_connect(G_OBJECT(window), "delete_event", G_CALLBACK(delete_event), NULL);


	/* show window */
	gtk_widget_show(window);

	/* menu */
	menu_conversation = gtk_menu_new();
	menu_tools = gtk_menu_new();
	menu_help = gtk_menu_new();

	menu_item_conversation_save_log = gtk_menu_item_new_with_mnemonic("_Save log...");
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_conversation), menu_item_conversation_save_log);
	gtk_widget_show(menu_item_conversation_save_log);
	g_signal_connect(G_OBJECT(menu_item_conversation_save_log), "activate", G_CALLBACK(menu_item_conversation_save_log_callback), NULL);

	menu_item_conversation_clear_log = gtk_menu_item_new_with_mnemonic("C_lear log");
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_conversation), menu_item_conversation_clear_log);
	gtk_widget_show(menu_item_conversation_clear_log);
	g_signal_connect(G_OBJECT(menu_item_conversation_clear_log), "activate", G_CALLBACK(menu_item_conversation_clear_log_callback), NULL);

	menu_item_conversation_sep1 = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_conversation), menu_item_conversation_sep1);
	gtk_widget_show(menu_item_conversation_sep1);

	menu_item_conversation_exit = gtk_menu_item_new_with_mnemonic("E_xit");
	gtk_widget_show(menu_item_conversation_exit);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_conversation), menu_item_conversation_exit);
	g_signal_connect(G_OBJECT(menu_item_conversation_exit), "activate", G_CALLBACK(menu_item_conversation_exit_callback), NULL);

	check_menu_item_tools_always_on_top = gtk_check_menu_item_new_with_mnemonic("Always on _Top");
	gtk_widget_show(check_menu_item_tools_always_on_top);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_tools), check_menu_item_tools_always_on_top);
	g_signal_connect(G_OBJECT(check_menu_item_tools_always_on_top), "activate", G_CALLBACK(check_menu_item_tools_always_on_top_callback), &state_always_on_top);

	menu_item_help_about = gtk_menu_item_new_with_mnemonic("_About");
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_help), menu_item_help_about);
	g_signal_connect(G_OBJECT(menu_item_help_about), "activate", G_CALLBACK(menu_item_help_about_callback), NULL);
	gtk_widget_show(menu_item_help_about);

	menu_item_conversation = gtk_menu_item_new_with_mnemonic("_Conversation");
	menu_item_tools = gtk_menu_item_new_with_mnemonic("_Tools");
	menu_item_help = gtk_menu_item_new_with_mnemonic("_Help");

	gtk_widget_show(menu_item_conversation);
	gtk_widget_show(menu_item_tools);
	gtk_widget_show(menu_item_help);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item_conversation), menu_conversation);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item_tools), menu_tools);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item_help), menu_help);

	/* menu bar */
	menu_bar = gtk_menu_bar_new();
	gtk_widget_show(menu_bar);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), menu_item_conversation);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), menu_item_tools);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), menu_item_help);

	/* text view */
	text_view = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
	gtk_widget_show(text_view);

	/* scrolled window */
	scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 5);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), text_view);
	gtk_widget_show(scrolled_window);

	/* message entry and send button */
	entry_msg = gtk_entry_new();
	gtk_widget_show(entry_msg);

	button_send = gtk_button_new_with_label("Send");
	g_signal_connect(G_OBJECT(button_send), "clicked", G_CALLBACK(button_send_callback), entry_msg);

	g_signal_connect(G_OBJECT(entry_msg), "key_press_event", G_CALLBACK(entry_msg_key_press_callback), entry_msg);

	gtk_widget_show(button_send);

	/* hbox */
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
	gtk_box_pack_start(GTK_BOX(hbox), entry_msg, TRUE, TRUE, 2);
	gtk_box_pack_start(GTK_BOX(hbox), button_send, FALSE, FALSE, 2);
	gtk_widget_show(hbox);

	/* vbox */
	vbox = gtk_vbox_new(FALSE, 0);
    gtk_widget_show(vbox);

	gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_box_pack_start(GTK_BOX(vbox), menu_bar, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);


	/* focus */
	gtk_window_set_focus(GTK_WINDOW(window), entry_msg);

	gdk_threads_leave();

	/* main loop */
	gdk_threads_enter();
	gtk_main();
	gdk_threads_leave();

	return 0;
}

static gboolean autoscroll_idle(gpointer data)
{
	GtkAdjustment *vadj;
	gdouble value;
	g_object_get (scrolled_window, "vadjustment", &vadj, NULL);
	value = gtk_adjustment_get_upper (vadj) - gtk_adjustment_get_page_size
		(vadj);
	gtk_adjustment_set_value (vadj, value);
	g_object_unref (vadj);
	return FALSE;
}

/* register nickname */
int register_nickname(int sockfd, char *nickname)
{
	if (strlen(nickname) > 255) return -1; /* nickname too long */
	unsigned char nickname_len = (unsigned char)strlen(nickname);
	char paste_buf[BUFFER_SIZE];
	char *paste_buf_p;
	paste_buf_p = paste_buf;
	/* command */
	*paste_buf_p++ = CMD_SET_NICKNAME;
	/* nickname length */
	*paste_buf_p++ = nickname_len;
	strncpy(paste_buf_p, nickname, strlen(nickname));
	paste_buf_p += strlen(nickname) + 1;
	unsigned int msg_len = paste_buf_p - paste_buf;
	if (send(sockfd, (char *)&paste_buf, msg_len, 0) == -1)
	{
		/* fail to send command */
		return -1;
	}
	return 0;
}

/* message receiving threading */
void *recv_message(void *data)
{
	char recv_buf[BUFFER_SIZE];
	char *recv_buf_p;
	char *msg_cmd = recv_buf;
	char *msg_nickname;
	char *msg_content;
	unsigned char msg_nickname_len;
	unsigned char msg_content_len;
	char paste_buf[BUFFER_SIZE];
	char *paste_buf_p;
	int recv_len;
	while (1)
	{
		/* receive message from socket */
		recv_len = recv(sockfd, recv_buf, BUFFER_SIZE, 0);
		if (recv_len <= 0)
		{
			break;
		}
		switch (*msg_cmd)
		{
			case CMD_RECV_MSG:
				recv_buf_p = recv_buf + 1;
				/* nickname length of sender */
				msg_nickname_len = *recv_buf_p++;
				/* nickname */
				msg_nickname = recv_buf_p;
				recv_buf_p += msg_nickname_len;
				/* content length */
				msg_content_len = *recv_buf_p++;
				/* content */
				msg_content = recv_buf_p;
				recv_buf_p += msg_content_len;
				/* make message */
				paste_buf_p = paste_buf;
				strncpy(paste_buf_p, msg_nickname, msg_nickname_len);
				paste_buf_p += msg_nickname_len;
				*paste_buf_p++ = ':';
				strncpy(paste_buf_p, msg_content, msg_content_len);
				paste_buf_p += msg_content_len;
				*paste_buf_p++ = '\n';
				/* append message into textview widget */
				g_usleep(1);
				gdk_threads_enter();
				GtkTextBuffer *buffer;
				buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
				GtkTextIter iter;
				gtk_text_buffer_get_end_iter(buffer, &iter);
				gtk_text_buffer_insert(buffer, &iter, paste_buf, paste_buf_p - paste_buf);
				/* scroll to buttom */
				g_idle_add(autoscroll_idle, scrolled_window);

				gdk_threads_leave();

				break;
			default:
				/* not supported */
				break;
		}
	}
	exit_state = EXIT_STATE_SERVER_DISCONNECTED;
	gtk_main_quit();
	return NULL;
}

/****************
 * LOGIN WINDOW *
 ****************/

#define LOGIN_OK 0
#define LOGIN_EXIT 1
#define LOGIN_NOT_VALID 2

/* login window */
GtkWidget *window_login;

struct login_info
{
	GtkWidget *entry_server;
	GtkWidget *entry_port;
	GtkWidget *entry_nickname;
	char *server_p;
	char *port_p;
	char *nickname_p;
	int ret;
};

static void login_destroy(GtkWidget *window, gpointer *data)
{
	struct login_info *info = (struct login_info *)data;
	info->ret = LOGIN_EXIT;
	gtk_widget_destroy(window_login);
	gtk_main_quit();
}

static gboolean login_delete_event(GtkWidget *window, GdkEvent *event, gpointer data)
{
	return FALSE;
}

static int login_button_login_callback(GtkWidget *widget, gpointer data)
{
	struct login_info *info = (struct login_info *)data;
	/* get text from entry */
	/*gdk_threads_enter();*/
	const char *server_p = gtk_entry_get_text(GTK_ENTRY(info->entry_server));
	const char *port_p = gtk_entry_get_text(GTK_ENTRY(info->entry_port));
	const char *nickname_p = gtk_entry_get_text(GTK_ENTRY(info->entry_nickname));
	/*gdk_threads_leave();*/
	/* verify login info */
	if (strlen(server_p) == 0 || strlen(port_p) == 0 || strlen(nickname_p) == 0)
	{
		info->ret = LOGIN_NOT_VALID;
		return 0;
	}
	/* copy text */
	strcpy(info->server_p, server_p);
	strcpy(info->port_p, port_p);
	strcpy(info->nickname_p, nickname_p);
	/* close login dialog */
	gtk_widget_destroy(window_login);
	/* mark return state */
	info->ret = LOGIN_OK;
	gtk_main_quit();
	return 0;
}

static int login_button_exit_callback(GtkWidget *widget, gpointer data)
{
	struct login_info *info = (struct login_info *)data;
	/* cancel this login */
	info->ret = LOGIN_EXIT;
	gtk_widget_destroy(window_login);
	gtk_main_quit();
	return 0;
}

int login(char *server, char *port, char *nickname)
{
	struct login_info info;
	info.ret = 0;
	GtkWidget *vbox;

	GtkWidget *table;
	GtkWidget *label_server, *label_nickname, *label_port;
	GtkWidget *entry_server, *entry_nickname, *entry_port;

	GtkWidget *hbox;
	GtkWidget *layout;
	GtkWidget *button_login, *button_exit;

	gdk_threads_enter();
	/* create main window */
	window_login = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	/* setting for login window */
	gtk_window_set_title(GTK_WINDOW(window_login), "Login");
	gtk_widget_set_size_request(window_login, 300, 200);
	gtk_container_set_border_width(GTK_CONTAINER(window_login), 10);
	gtk_window_set_resizable(GTK_WINDOW(window_login), FALSE);
	gtk_window_set_position(GTK_WINDOW(window_login), GTK_WIN_POS_CENTER);
	gtk_window_set_icon(GTK_WINDOW(window_login), NULL);
	g_signal_connect(G_OBJECT(window_login), "destroy", G_CALLBACK(login_destroy), &info);
	g_signal_connect(G_OBJECT(window_login), "delete_event", G_CALLBACK(login_delete_event), NULL);
	gtk_widget_show(window_login);

	/* common widgets */
	label_server = gtk_label_new("Server :");
	label_port = gtk_label_new("Port :");
	label_nickname = gtk_label_new("Nickname :");
	gtk_misc_set_alignment(GTK_MISC(label_server), 1, 0);
	gtk_misc_set_padding(GTK_MISC(label_server), 10, 10);
	gtk_misc_set_alignment(GTK_MISC(label_port), 1, 0);
	gtk_misc_set_padding(GTK_MISC(label_port), 10, 10);
	gtk_misc_set_alignment(GTK_MISC(label_nickname), 1, 0);
	gtk_misc_set_padding(GTK_MISC(label_nickname), 10, 10);
	entry_server = gtk_entry_new();
	entry_port = gtk_entry_new();
	entry_nickname = gtk_entry_new();
	table = gtk_table_new(3, 2, FALSE);
	gtk_table_attach(GTK_TABLE(table), label_server, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND, 5, 5);
	gtk_table_attach(GTK_TABLE(table), label_port, 0, 1, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND, 5, 5);
	gtk_table_attach(GTK_TABLE(table), label_nickname, 0, 1, 2, 3, GTK_EXPAND | GTK_FILL, GTK_EXPAND, 5, 5);
	gtk_table_attach(GTK_TABLE(table), entry_server, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 5, 5);
	gtk_table_attach(GTK_TABLE(table), entry_port, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 5, 5);
	gtk_table_attach(GTK_TABLE(table), entry_nickname, 1, 2, 2, 3, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 5, 5);
	button_login = gtk_button_new_with_label("Login");
	button_exit = gtk_button_new_with_label("Exit");
	info.entry_server = entry_server;
	info.entry_port = entry_port;
	info.entry_nickname = entry_nickname;
	info.nickname_p = nickname;
	info.port_p = port;
	info.server_p = server;
	g_signal_connect(G_OBJECT(button_login), "clicked", G_CALLBACK(login_button_login_callback), &info);
	g_signal_connect(G_OBJECT(button_exit), "clicked", G_CALLBACK(login_button_exit_callback), &info);
	layout = gtk_layout_new(NULL, NULL);

	/* container widgets */
	hbox = gtk_hbox_new(TRUE, 3);
	gtk_box_pack_start(GTK_BOX(hbox), layout, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), button_login, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), button_exit, TRUE, TRUE, 0);

	vbox = gtk_vbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(window_login), vbox);

	/* default value for entry */
	gtk_entry_set_text(GTK_ENTRY(entry_server), "127.0.0.1");
	gtk_entry_set_text(GTK_ENTRY(entry_port), "8089");
	gtk_entry_set_text(GTK_ENTRY(entry_nickname), "guest");

	/* focus */
	gtk_window_set_focus(GTK_WINDOW(window_login), entry_server);
	
	/* show window */
	gtk_widget_show(label_server);
	gtk_widget_show(label_port);
	gtk_widget_show(label_nickname);
	gtk_widget_show(entry_server);
	gtk_widget_show(entry_port);
	gtk_widget_show(entry_nickname);
	gtk_widget_show(table);
	gtk_widget_show(layout);
	gtk_widget_show(button_login);
	gtk_widget_show(button_exit);
	gtk_widget_show(hbox);
	gtk_widget_show(vbox);
	gdk_threads_leave();

	/* main loop */
	gdk_threads_enter();
	gtk_main();
	gdk_threads_leave();

	return info.ret;
}


int main(int argc, const char *argv[])
{
	int ret;

	/* initialize global variables */
	sockfd = 0;
	exit_state = EXIT_STATE_MANUAL;

	/* multi-threading support for gtk */
	if (!g_thread_supported())
		g_thread_init(NULL);
	gdk_threads_init();
	/* initialize gtk */
	gtk_init(&argc, (char ***)&argv);

	char server_addr[256], port_p[256], nickname[256];
	/* login ui */
	int login_ret;
	login_ret = login(server_addr, port_p, nickname);
	if (login_ret == LOGIN_EXIT) exit(1);
	else if (login_ret == LOGIN_OK)
	{
		/* got enough login info */
	}
	else
	{
		exit(1);
	}
	unsigned short port = atoi(port_p);

	/* initialize winsock for windows */
#if defined(WINDOWS)
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
	wVersionRequested = MAKEWORD( 1, 1 );
	err = WSAStartup( wVersionRequested, &wsaData );
	if ( err != 0 ) {
		fatal_error("WSAStartup failed");
	}
#endif
	/* create socket */
	struct sockaddr_in servaddr;
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		fatal_error("create socket failed");
	}
	bzero(&servaddr, sizeof(servaddr));
	/* test destination address */
	/*
	struct hostent *ent;
	ent = gethostbyaddr(server_addr, 4, AF_INET);
	if (ent == NULL)
	{
		fatal_error("invalid server address");
	}
	*/
#if defined(UNIX)
	inet_pton(AF_INET, server_addr, &servaddr.sin_addr);
#elif defined(WINDOWS)
	servaddr.sin_addr.S_un.S_addr = inet_addr(server_addr);
#endif
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);

#if defined(SHORT_BLOCK)
	/* time */
    struct timeval tm;
    fd_set set;
	/* timeout setting */
	unsigned long ul = 1;
#if defined(UNIX)
	ioctl(sockfd, FIONBIO, &ul); /* set sockfd as non block mode */
#elif defined(WINDOWS)
	ioctlsocket(sockfd, FIONBIO, &ul); /* set sockfd as non block mode */
#endif
#endif

	if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(struct sockaddr)) == -1)
	{
#if defined(SHORT_BLOCK)
		tm.tv_sec  = TIME_OUT_TIME;
		tm.tv_usec = 0;
		FD_ZERO(&set);
		FD_SET(sockfd, &set);
		if(select(sockfd+1, NULL, &set, NULL, &tm) > 0)
		{
			int len;
			len = sizeof(int);
#if defined(UNIX)
			int error = -1;
			getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, (socklen_t *)&len);
#elif defined(WINDOWS)
			char error = -1;
			getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, (int *)&len);
#endif
			if(error == 0) ret = 1;
			else ret = 0;
		} else ret = 0;
#else
		fatal_error("connect failed");
#endif
	}
#if defined(SHORT_BLOCK)
	else ret = 0;
	ul = 0;
#if defined(UNIX)
    ioctl(sockfd, FIONBIO, &ul);  /* set fd as block mode */
#elif defined(WINDOWS)
    ioctlsocket(sockfd, FIONBIO, &ul);  /* set fd as block mode */
#endif
	if (!ret)
	{
		fatal_error("connect failed");
	}
#endif

	/* create thread for recive message */
#if defined(UNIX)
	pthread_t thd_recv;
	ret = pthread_create(&thd_recv, NULL, recv_message, &sockfd);
	if (ret != 0)
	{
		fatal_error("start recv thread failed");
	}
#elif defined(WINDOWS)
	HANDLE thd_recv;
	DWORD thd_recv_id;
	thd_recv = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)recv_message, (void *)NULL, 0, &thd_recv_id);
	if (thd_recv == NULL)
	{
		fatal_error("start recv thread failed");
	}
#endif
	/* register nickname */
	register_nickname(sockfd, nickname);

	/* enter chat UI */
	chat();

	/* judge exit state */
	if (exit_state == EXIT_STATE_SERVER_DISCONNECTED)
		fatal_error("server disconnect\n");

	/* close fd and uninitialize winsock for windows */
#if defined(UNIX)
	close(sockfd);
#elif defined(WINDOWS)
	closesocket(sockfd);
	WSACleanup();
#endif
	return 0;
}

