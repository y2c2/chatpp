/* Multi-User Chat Server
 * Copyright(C) 2012 y2c2 */

/* Uses TCP protocol for data transmission
 * On multi-threading supporting side, uses pthread for UNIX and
 * Win32 threading for Windows */

/* base */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>

/* network */
#if defined(UNIX)
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
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

/* general constants */
#define BUFFER_SIZE 4096
#define SERVER_PORT_DEFAULT 8089

/* mini shell thread */
#if defined(UNIX)
pthread_t thd_shell;
#elif defined(WINDOWS)
HANDLE thd_shell;
DWORD thd_shell_id;
#endif

/* server commands */
enum {
	CMD_NULL = 0,
	CMD_SET_NICKNAME = 1,
	CMD_SEND_MSG = 2,
	CMD_RECV_MSG = 3,
};

#define NICKNAME_LEN_MAX 50

/* sub server */
struct sub_server
{
	int client_fd; /* client socket */
#if defined(UNIX)
	pthread_t thd; /* client socket */
	pthread_t thd_id;
#elif defined(WINDOWS)
	HANDLE thd;
	DWORD thd_id;
#endif
	char nickname[NICKNAME_LEN_MAX];
	unsigned char nickname_len;
	struct sub_server *next;
	char client_ip_addr[16];
};

/* sub server list */
struct sub_server_list
{
	struct sub_server *begin;
	struct sub_server *final;
	unsigned int size;
};

struct sub_server_list *sub_server_list_new()
{
	struct sub_server_list *new_list = (struct sub_server_list *)malloc(sizeof(struct sub_server_list));
	if (new_list == NULL) return NULL;
	new_list->begin = NULL;
	new_list->final = NULL;
	new_list->size = 0;
	return new_list;
}

#if defined(UNIX)
pthread_mutex_t mutex_server_list = PTHREAD_MUTEX_INITIALIZER;
#elif defined(WINDOWS)
CRITICAL_SECTION cs_server_list;
#endif

struct sub_server *sub_server_list_push_back(struct sub_server_list *list, struct sub_server *server)
{
#if defined(UNIX)
	pthread_mutex_lock(&mutex_server_list);
#elif defined(WINDOWS)
	EnterCriticalSection(&cs_server_list);
#endif
	struct sub_server *new_node = (struct sub_server *)malloc(sizeof(struct sub_server));
	if (new_node == NULL) goto done;
	new_node->client_fd = server->client_fd;
	strncpy(new_node->nickname, server->nickname, NICKNAME_LEN_MAX);
	new_node->nickname_len = server->nickname_len;
	new_node->thd = server->thd;
	new_node->thd_id = server->thd_id;
	strncpy(new_node->client_ip_addr, server->client_ip_addr, 16);
	new_node->next = NULL;
	list->size++;
	if (list->begin == NULL)
	{
		/* no existed node */
		list->begin = list->final = new_node;
	}
	else
	{
		list->final->next = new_node;
		list->final = new_node;
	}
done:
#if defined(UNIX)
	pthread_mutex_unlock(&mutex_server_list);
#elif defined(WINDOWS)
	LeaveCriticalSection(&cs_server_list);
#endif
	return new_node;
}

int sub_server_list_delete(struct sub_server_list *list, struct sub_server *server)
{
#if defined(UNIX)
	pthread_mutex_lock(&mutex_server_list);
#elif defined(WINDOWS)
	EnterCriticalSection(&cs_server_list);
#endif
	struct sub_server *sav, *next;
	if (list == NULL) goto done;
	if (list->begin == NULL) goto done;
	struct sub_server *cur = list->begin, *target;
	target = NULL;
	sav = NULL;
	/* find node to be delete */
	while (cur != NULL)
	{
		if (cur == server)
		{
			target = cur;
			break;
		}
		sav = cur;
		cur = cur->next;
	}
	/* can't find target */
	if (target == NULL)
	{
		goto done;
	}
	/* set new begin and final */
	struct sub_server *next_begin = list->begin, *next_final = list->final;
	if (target == list->begin) next_begin = list->begin->next;
	if (target == list->final) next_final = sav;
	/* delete node */
	next = cur->next;
	/* the thread will exit it by itself */
#if 0
#if defined(UNIX)
	pthread_cancel(cur->thd);
#elif defined(WINDOWS)
	TerminateThread(&cur->thd, 0);
#endif
#endif
	close(cur->client_fd);
	free(cur);
	if (sav != NULL) sav->next = next;
	/* update begin and final */
	list->begin = next_begin;
	list->final = next_final;
	/* update size */
	list->size--;
done:
#if defined(UNIX)
	pthread_mutex_unlock(&mutex_server_list);
#elif defined(WINDOWS)
	LeaveCriticalSection(&cs_server_list);
#endif
	return 0;
}

int sub_server_list_walk(struct sub_server_list *list)
{
#if defined(UNIX)
	pthread_mutex_lock(&mutex_server_list);
#elif defined(WINDOWS)
	EnterCriticalSection(&cs_server_list);
#endif
	struct sub_server *cur = list->begin;
	int idx = 0;
	while (cur != NULL)
	{
		idx++;
		printf("#%4d: address=%s, thread id=%lu, fd=%d\n", idx, cur->client_ip_addr, cur->thd_id, cur->client_fd);
		cur = cur->next;
	}
#if defined(UNIX)
	pthread_mutex_unlock(&mutex_server_list);
#elif defined(WINDOWS)
	LeaveCriticalSection(&cs_server_list);
#endif
	return 0;
}

int sub_server_list_destroy(struct sub_server_list *list)
{
#if defined(UNIX)
	pthread_mutex_lock(&mutex_server_list);
#elif defined(WINDOWS)
	EnterCriticalSection(&cs_server_list);
#endif
	struct sub_server *sav, *cur = list->begin;
	while (cur != NULL)
	{
		sav = cur->next;
#if defined(UNIX)
		pthread_cancel(cur->thd);
#elif defined(WINDOWS)
		TerminateThread(&cur->thd, 0);
#endif
#if defined(UNIX)
		close(cur->client_fd);
#elif defined(WINDOWS)
		closesocket(cur->client_fd);
		WSACleanup();
#endif
		free(cur);
		cur = sav;
	}
	free(list);
#if defined(UNIX)
	pthread_mutex_unlock(&mutex_server_list);
#elif defined(WINDOWS)
	LeaveCriticalSection(&cs_server_list);
#endif
	return 0;
}

int sub_server_list_sendmsg_to_all(struct sub_server_list *list, char *msg, size_t len)
{
	char send_buf[BUFFER_SIZE];
	size_t send_len;
	strncpy(send_buf, msg, len);
	send_len = len;
#if defined(UNIX)
	pthread_mutex_lock(&mutex_server_list);
#elif defined(WINDOWS)
	EnterCriticalSection(&cs_server_list);
#endif
	struct sub_server *cur = list->begin;
	while (cur != NULL)
	{
		if (send(cur->client_fd, (char *)&send_buf, send_len, 0) == -1)
		{
			/* do nothing */
		}
		cur = cur->next;
	}
#if defined(UNIX)
	pthread_mutex_unlock(&mutex_server_list);
#elif defined(WINDOWS)
	LeaveCriticalSection(&cs_server_list);
#endif
	return 0;
}

/* GLOBAL variables */
int server_fd;
struct sub_server_list *server_list;

/* clean work before exit server program */
int clean(void)
{
	/* close sub servers */
	sub_server_list_destroy(server_list);
	/* close mini shell */
#if defined(UNIX)
	pthread_cancel(thd_shell);
#elif defined(WINDOWS)
	TerminateThread(&thd_shell, 0);
#endif
	return 0;
}

/* fatal error occurred, 
 * print the error message and exit server program */
void fatal_error(char *msg)
{
	printf("Error : %s\n", msg);
	clean();
	exit(1);
}

int set_nickname(struct sub_server *server, char *nickname, unsigned char nickname_len)
{
#if defined(UNIX)
	pthread_mutex_lock(&mutex_server_list);
#elif defined(WINDOWS)
	EnterCriticalSection(&cs_server_list);
#endif
	server->nickname_len = nickname_len;
	strncpy(server->nickname, nickname, nickname_len);
#if defined(UNIX)
	pthread_mutex_unlock(&mutex_server_list);
#elif defined(WINDOWS)
	LeaveCriticalSection(&cs_server_list);
#endif
	return 0;
}

/* sub server working threading */
void *sub_server_start(void *data)
{
	/* get thread id */
#if defined(UNIX)
	((struct sub_server *)data)->thd_id = pthread_self();
#elif defined(WINDOWS)
	((struct sub_server *)data)->thd_id = GetCurrentThreadId();
#endif
	/* add current server to server list */
	struct sub_server *server;
	server = sub_server_list_push_back(server_list, data);
	if (server == NULL)
	{
		fatal_error("fork server failed");
	}
	char send_buf[BUFFER_SIZE], recv_buf[BUFFER_SIZE];
	int send_len, recv_len;
	char *send_buf_p;
	char *msg_cmd = recv_buf;
	char *msg_body = recv_buf + 1;
	/* message loop */
	while (1)
	{
		/* receive message */
		recv_len = recv(server->client_fd, recv_buf, BUFFER_SIZE, 0);
		if (recv_len <= 0)
		{
			break;
		}
		/* verify command length */
		if (recv_len > 1)
		{
			switch (*msg_cmd)
			{
				case CMD_NULL:
					/* do nothing */
					break;
				case CMD_SET_NICKNAME:
					if (recv_len > 2) /* length check */
					{
						unsigned char nickname_len = *((unsigned char *)recv_buf + 1);
						char *nickname_p = recv_buf + 2;
						set_nickname(server, nickname_p, nickname_len);
					}
					break;
				case CMD_SEND_MSG:
					send_buf_p = send_buf;
					/* command */
					*send_buf_p++ = CMD_RECV_MSG;
					/* nickname length */
					*send_buf_p++ = server->nickname_len;
					/* nickname */
					strncpy(send_buf_p, server->nickname, server->nickname_len);
					send_buf_p += server->nickname_len;
					/* message_len */
					*send_buf_p++ = (unsigned char)(recv_len - 1);
					/* message */
					strncpy(send_buf_p, msg_body, recv_len - 1);
					send_buf_p += recv_len - 1;
					/* whole command length */
					send_len = send_buf_p - send_buf;
					/* send received message to all clients */
					sub_server_list_sendmsg_to_all(server_list, send_buf, send_len);
					break;
				default:
					/* not supported */
					break;
			}
		}
	}
	/* to delete this server */
	sub_server_list_delete(server_list, server);
	return NULL;
}

/* signal handler */
static void sig_int(int signo)
{
	if (signo == SIGINT) /* C-c */
	{
		/* close server fd and exit program */
		printf("\nControl-c received, exit server\n");
		clean();
		/*
		   if (server_fd > 0)
		   {
#if defined(UNIX)
close(server_fd);
#elif defined(WINDOWS)

closesocket(server_fd);
WSACleanup();
#endif
#if defined(UNIX)
pthread_cancel(thd_shell);
#elif defined(WINDOWS)
TerminateThread(&thd_shell, 0);
#endif
}
*/
		exit(0);
		}
}

#define CMD_LEN_MAX 512

/* server shell */
#if defined(UNIX)
	void *mini_shell(void *data)
#elif defined(WINDOWS)
DWORD WINAPI mini_shell(void *data)
#endif
{
	char *help_info = ""
		"jobs          -- list all running clients\n"
		"quit          -- quit server program\n"
		"help          -- show this information\n";
	char cmd[CMD_LEN_MAX];
	while (1)
	{
		printf("$ ");
		fgets(cmd, BUFFER_SIZE, stdin);
		if (strlen(cmd) > 0) cmd[strlen(cmd) - 1] = '\0';
		if (!strncmp(cmd, "help", CMD_LEN_MAX))
		{
			printf("%s", help_info);
		}
		else if (!strncmp(cmd, "quit", CMD_LEN_MAX) || !strncmp(cmd, "exit", CMD_LEN_MAX))
		{
			exit(1);
		}
		else if (!strncmp(cmd, "jobs", CMD_LEN_MAX))
		{
			if (server_list->size == 0)
			{
				printf("no job\n");
			}
			else
			{
				printf("%d job(s)\n", server_list->size);
				sub_server_list_walk(server_list);
			}
		}
		else
		{
			printf("%s: command not found\n", cmd);
		}
	}
#if defined(UNIX)
	return NULL;
#elif defined(WINDOWS)
	return 0;
#endif
}

/* main routine */
int main(int argc, const char *argv[])
{
	unsigned short port; /* listening port */
	port = SERVER_PORT_DEFAULT;

	/* parser argv */
	if (argc > 1)
	{
		if (!strcmp(argv[1], "-p") && argc > 2)
		{
			port = atoi(argv[2]);
		}
	}

#if defined(WINDOWS)
	/* need to initialize critical section for Windows*/
	if (InitializeCriticalSectionAndSpinCount(&cs_server_list, 4000) != TRUE)
	{
		fatal_error("initialize critical section failed");
	}
#endif

	/* initialize global variables */
	thd_shell = 0;
	server_fd = 0;
	server_list = sub_server_list_new();
	if (server_list == NULL) fatal_error("initialize server list error");

	printf("Install signal..");
	/* install signal */
	if (signal(SIGINT, sig_int) == SIG_ERR)
	{
		fatal_error("install signal failed");
	}
	printf("ok\n");

	/* initialize winsock */
#if defined(WINDOWS)
	printf("Initialize winsock..");
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
	wVersionRequested = MAKEWORD(1, 1);
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		fatal_error("WSAStartup failed");
	}
	printf("ok\n");
#endif

	int client_fd;
	struct sockaddr_in servaddr, cliaddr;

	/* create server socket */
	printf("Create server socket..");
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		fatal_error("create server socket failed");
	}
	printf("ok\n");
	printf("listening port is %d\n", port);

	/* setting socket */
	printf("Setting socket..");
	/* allow relistening */
#if defined(UNIX)
	int opt = 1;
#elif defined(WINDOWS)
	const char opt = 1;
#endif
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	/* bind */
	bzero(&(servaddr.sin_zero), sizeof(servaddr.sin_zero));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(server_fd, (struct sockaddr *)&servaddr, sizeof(servaddr));
	printf("ok\n");

	/* listen */
	printf("Listen..");
	if (listen(server_fd, 5) == -1)
	{
		fatal_error("server socket listen failed");
	}
	printf("ok\n");

	printf("Server is now ready to work\n");

	/* for a server shell */
#if defined(UNIX)
	int ret;
	ret = pthread_create(&thd_shell, NULL, mini_shell, (void *)NULL);
	if (ret != 0)
	{
		fatal_error("start mini shell failed");
	}
#elif defined(WINDOWS)
	thd_shell = CreateThread(NULL, 0, mini_shell, (void *)NULL, 0, &thd_shell_id);
	if (thd_shell == NULL)
	{
		fatal_error("start mini shell failed");
	}
#endif

	/* main loop for listen */
	while (1)
	{
#if defined(UNIX)
		unsigned int sin_size = sizeof(struct sockaddr_in);
#elif defined(WINDOWS)
		int sin_size = sizeof(struct sockaddr_in);
#endif
		if ((client_fd = accept(server_fd, (struct sockaddr *)&cliaddr, &sin_size)) == -1)
		{
			continue;
		}
		/* make setting for client threading */
		struct sub_server server;
		server.client_fd = client_fd;
		strcpy(server.nickname, "guest");
		server.nickname_len = strlen("guest");

		char *client_ip_addr_buffer;
		/*struct sockaddr_in cliaddr;*/
		client_ip_addr_buffer = inet_ntoa(cliaddr.sin_addr);
		strncpy(server.client_ip_addr, client_ip_addr_buffer, 16);

		/* fork a sub server threading */
#if defined(UNIX)
		ret = pthread_create(&server.thd, NULL, sub_server_start, (void *)&server);
		if (ret != 0)
		{
			fatal_error("fork sub server failed");
		}
#elif defined(WINDOWS)
		DWORD sub_server_thd_id;
		server.thd = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)sub_server_start, (void *)&server, 0, (PDWORD)&sub_server_thd_id);
		if (server.thd == NULL)
		{
			fatal_error("fork sub server failed");
		}
#endif
	}
	clean();
	/* close server fd and exit program */
#if defined(UNIX)
	close(server_fd);
#elif defined(WINDOWS)
	closesocket(server_fd);
	WSACleanup();
#endif

#if defined(WINDOWS)
	/* need to delete critical section for Windows*/
	DeleteCriticalSection(&cs_server_list);
#endif
	return 0;
}

