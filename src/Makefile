MAKE = make
OBJECTS_CLIENT = chatpp_client.o
OBJECTS_SERVER = chatpp_server.o
LIBS = 
TARGET_CLIENT_UNIX = chatpp_client
TARGET_CLIENT_WIN32 = chatpp_client.exe
TARGET_SERVER_UNIX = chatpp_server
TARGET_SERVER_WIN32 = chatpp_server.exe
TARGET = 
CC = gcc
RM_UNIX = rm
RM_WIN32 = del
RM = 
LINK_FLAGS_CLIENT_UNIX = -lpthread
LINK_FLAGS_SERVER_UNIX = -lpthread
LINK_FLAGS_CLIENT_WIN32 = -mwindows -mingw32 -lwsock32
LINK_FLAGS_SERVER_WIN32 = -mingw32 -lwsock32
LINK_FLAGS_CLIENT =
LINK_FLAGS_SERVER =
LINK_GTK = `pkg-config gtk+-2.0 --cflags --libs gdk-2.0 gthread-2.0`
CFLAGS_UNIX = -DUNIX
CFLAGS_WIN32 = -DWINDOWS
CFLAGS =
DEBUG_FLAGS = "-Wall -g"
RELEASE_FLAGS = "-Wall -O3"
OS_TYPE = 
RES_WIN32 = chat.res
RES_UNIX = 
RES = 

ifdef SystemRoot
	OS_TYPE = win32
	LINK_FLAGS_CLIENT = $(LINK_FLAGS_CLIENT_WIN32)
	LINK_FLAGS_SERVER = $(LINK_FLAGS_SERVER_WIN32)
	CFLAGS = $(CFLAGS_WIN32)
	TARGET_CLIENT = $(TARGET_CLIENT_WIN32)
	TARGET_SERVER = $(TARGET_SERVER_WIN32)
	RES = $(RES_WIN32)
	RM = rm -f
	FixPath = $(subst /,\,$1)
else
	ifeq ($(shell uname), Linux)
	OS_TYPE = linux
	LINK_FLAGS_CLIENT = $(LINK_FLAGS_CLIENT_UNIX)
	LINK_FLAGS_SERVER = $(LINK_FLAGS_SERVER_UNIX)
	CFLAGS = $(CFLAGS_UNIX)
	TARGET_CLIENT = $(TARGET_CLIENT_UNIX)
	TARGET_SERVER = $(TARGET_SERVER_UNIX)
	RES = $(RES_UNIX)
	RM = rm -f
	FixPath = $1
	endif
endif

default : debug
debug :
	@${MAKE} targets_client BUILD_FLAGS=$(DEBUG_FLAGS)
	@${MAKE} targets_server BUILD_FLAGS=$(DEBUG_FLAGS)
release :
	@${MAKE} targets_client BUILD_FLAGS=$(RELEASE_FLAGS)
	@${MAKE} targets_server BUILD_FLAGS=$(RELEASE_FLAGS)

targets_client : $(OBJECTS_CLIENT)
ifeq ($(OS_TYPE), win32) 
	windres -i chat.rc --input-format=rc -o chat.res -O coff
endif
	$(CC) $(OBJECTS_CLIENT) $(BUILD_FLAGS) -o $(TARGET_CLIENT) $(LINK_FLAGS_CLIENT) $(RES) $(LIBS) $(LINK_GTK) 
targets_server : $(OBJECTS_SERVER)
	$(CC) $(OBJECTS_SERVER) $(BUILD_FLAGS) -o $(TARGET_SERVER) $(LINK_FLAGS_SERVER) $(LIBS)
chatpp_client.o : chatpp_client.c chat.xpm
	$(CC) $(BUILD_FLAGS) $(CFLAGS) $(LINK_GTK) -o chatpp_client.o -c chatpp_client.c
chatpp_server.o : chatpp_server.c
	$(CC) $(BUILD_FLAGS) $(CFLAGS) -o chatpp_server.o -c chatpp_server.c

.PHONY: clean cleanobj
clean :
	$(RM) $(OBJECTS_CLIENT)
	$(RM) $(OBJECTS_SERVER)
	$(RM) $(TARGET_CLIENT)
	$(RM) $(TARGET_SERVER)
cleanobj :
	$(RM) $(OBJECTS_CLIENT)
	$(RM) $(OBJECTS_SERVER)
