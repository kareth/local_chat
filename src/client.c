#include "libraries.h"

int my_server;
char my_login[2000];
int pids[200], my_queue;

// REQUESTS


//SERVER LIST
int request_server_list(SERVER_LIST_RESPONSE *servers){
  int list_id = msgget(SERVER_LIST_MSG_KEY, 0666);
  if( list_id == -1) return -1;
  int my_id = msgget(my_queue, 0666 | IPC_CREAT);

  SERVER_LIST_REQUEST req;
  req.type = SERVER_LIST;
  req.client_msgid = my_queue;

  msgsnd(list_id, &req, sizeof(req), 0);
  my_id = msgget(my_queue, 0666 | IPC_CREAT);

  SERVER_LIST_RESPONSE res;
  msgrcv(my_id, &res, sizeof(res), SERVER_LIST, 0);

  *servers = res;
  return 0;
}

// LOGIN
int request_login(STATUS_RESPONSE *status, char * login, int server){
  int server_id = msgget(server, 0666);
  if( server_id == -1) return -1;
  int my_id = msgget(my_queue, 0666 | IPC_CREAT);

  CLIENT_REQUEST req;
  req.type = LOGIN;
  req.client_msgid = my_queue;
  strcpy(req.client_name, login);

  msgsnd(server_id, &req, sizeof(req), 0);

  STATUS_RESPONSE res;
  msgrcv(my_id, &res, sizeof(res), STATUS, 0);

  *status = res;
  return 0;
}

// LOGOUT
int request_logout(){
  int server_id = msgget(my_server, 0666);
  if( server_id == -1) return -1;

  CLIENT_REQUEST req;
  req.type = LOGOUT;
  req.client_msgid = my_queue;
  strcpy(req.client_name, my_login);

  msgsnd(server_id, &req, sizeof(req), 0);

  my_server = INF;
  return 0;
}

// JOIN ROOM
int request_join_room(STATUS_RESPONSE *status, char * channel){
  int server_id = msgget(my_server, 0666);
  if( server_id == -1) return -1;

  int my_id = msgget(my_queue, 0666 | IPC_CREAT);

  CHANGE_ROOM_REQUEST req;
  req.type = CHANGE_ROOM;
  req.client_msgid = my_queue;
  strcpy(req.room_name, channel);
  strcpy(req.client_name, my_login);

  msgsnd(server_id, &req, sizeof(req), 0);

  STATUS_RESPONSE res;
  msgrcv(my_id, &res, sizeof(res), STATUS, 0);

  *status = res;
  return 0;
}


// ROOM LIST
int request_room_list(ROOM_LIST_RESPONSE *list){
  int server_id = msgget(my_server, 0666);
  if( server_id == -1) return -1;
  int my_id = msgget(my_queue, 0666 | IPC_CREAT);

  CLIENT_REQUEST req;
  req.type = ROOM_LIST;
  req.client_msgid = my_queue;
  strcpy(req.client_name, my_login);

  msgsnd(server_id, &req, sizeof(req), 0);

  ROOM_LIST_RESPONSE res;
  msgrcv(my_id, &res, sizeof(res), ROOM_LIST, 0);

  *list = res;
  return 0;
}


// USERS LIST
int request_users_here(ROOM_CLIENT_LIST_RESPONSE *users){
  int server_id = msgget(my_server, 0666);
  if( server_id == -1) return -1;
  int my_id = msgget(my_queue, 0666 | IPC_CREAT);

  CLIENT_REQUEST req;
  req.type = ROOM_CLIENT_LIST;
  req.client_msgid = my_queue;
  strcpy(req.client_name, my_login);

  msgsnd(server_id, &req, sizeof(req), 0);

  ROOM_CLIENT_LIST_RESPONSE res;
  msgrcv(my_id, &res, sizeof(res), ROOM_CLIENT_LIST, 0);

  *users = res;
  return 0;
}

int request_all_users(GLOBAL_CLIENT_LIST_RESPONSE *users){
  int server_id = msgget(my_server, 0666);
  if( server_id == -1) return -1;
  int my_id = msgget(my_queue, 0666 | IPC_CREAT);

  CLIENT_REQUEST req;
  req.type = GLOBAL_CLIENT_LIST;
  req.client_msgid = my_queue;
  strcpy(req.client_name, my_login);

  msgsnd(server_id, &req, sizeof(req), 0);

  GLOBAL_CLIENT_LIST_RESPONSE res;
  msgrcv(my_id, &res, sizeof(res), GLOBAL_CLIENT_LIST, 0);

  *users = res;
  return 0;
}




// SEND MESSAGE

int send_message(char * text){
  int server_id = msgget(my_server, 0666);
  if( server_id == -1) return -1;
  printf("sending: %s\n", text);

  TEXT_MESSAGE req;
  req.type = PUBLIC;
  req.from_id = my_queue;
  req.time = time(0);
  strcpy(req.from_name, my_login);
  strcpy(req.text, text);

  msgsnd(server_id, &req, sizeof(req), 0);
  return 0;
}


// Others

void close_client(){
  int id = msgget(my_queue, 0666);
  request_logout();
  msgctl(id, IPC_RMID, 0);
}

void handle_command(char * cd, char * in);
void handle_send_msg(char * in);

void handle_help();
void handle_server_list();
void handle_login(char * input);
void handle_logout();
void handle_join();
void handle_whisper();
void handle_exit();
void handle_unknown();
void handle_room_list();
void handle_room_client_list();
void handle_global_client_list();


void wait_for_public(){
    TEXT_MESSAGE text_message;
    int my_id = msgget(my_queue, 0666 | IPC_CREAT);
    printf("%d %d\n",my_queue, my_id);

    while (1) {
        int res = msgrcv(my_id, &text_message, sizeof(text_message), PUBLIC, 0);
        //int res = msgrcv(my_id, &text_message, sizeof(TEXT_MESSAGE)-sizeof(long), PUBLIC, 0);
        if( res == -1) break;
        printf("CLIENT Private message from %s to %s at %s\n", text_message.from_name, text_message.to, ctime(&text_message.time));
        printf("text: %s\n", text_message.text);
    }
}


void wait_for_private(){}
void heartbeat(){}

// MAIN

int main() {
  signal(SIGINT, close_client);
  signal(SIGTERM, close_client);

  my_server = INF;
  my_queue = getpid();
  msgget(my_queue, 0666 | IPC_CREAT);

  SERVER_LIST_RESPONSE servers;
  if( request_server_list(&servers) == -1){
    printf("No servers, sorry\n");
    exit(1);
  }
  else{
    printf("Active servers: %d\n", servers.active_servers);
    REP(i, servers.active_servers)
      printf("Server %d: %d users online\n", servers.servers[i], servers.clients_on_servers[i]);
  }

  int pc = 0;

  if((pids[pc++] = fork()) == 0){
    wait_for_public();
  }
  else if ((pids[pc++] = fork()) == 0){
    wait_for_private();
  }
  else if ((pids[pc++] = fork()) == 0){
    heartbeat();
  }
  else{
    char input[3000];
    char command[3000];

    while(1){
      printf("> ");
      scanf("%s",command);
      gets(input);
      if( command[0] == '/') handle_command(command, input);
      else handle_send_msg( strcat(command, input));
    }
  }
  close_client();
  return 0;
}





// Commands handlers

void handle_command(char * cd, char * in){
       if( !strcmp(cd, "/help") )        handle_help();
  else if( !strcmp(cd, "/server_list") ) handle_server_list();
  else if( !strcmp(cd, "/users_here") )  handle_room_client_list();
  else if( !strcmp(cd, "/all_users") )   handle_global_client_list();
  else if( !strcmp(cd, "/login") )       handle_login(in);
  else if( !strcmp(cd, "/room_list") )   handle_room_list();
  else if( !strcmp(cd, "/logout") )      handle_logout();
  else if( !strcmp(cd, "/join") )        handle_join(in);
  else if( !strcmp(cd, "/w") )           handle_whisper(in);
  else if( !strcmp(cd, "/exit") )        handle_exit();
  else                                  handle_unknown();
}

void handle_send_msg(char * input){
  send_message(input);
}

void handle_server_list(){
  SERVER_LIST_RESPONSE servers;
  if( request_server_list(&servers) == -1)
    printf("No servers up.\n");
  else{
    printf("Active servers: %d\n", servers.active_servers);
    REP(i, servers.active_servers)
      printf("Server %d: %d users online\n", servers.servers[i], servers.clients_on_servers[i]);
  }
}

void handle_whisper(char * input){
  char target[2000];
  sscanf(target, "%*s %s", target);
  printf("whole: %s\n", input);
  memmove(input, input+strlen(target)+1, strlen(input));
  printf("target: '%s' , content: '%s' \n", target, input);
}

void handle_global_client_list(){
  GLOBAL_CLIENT_LIST_RESPONSE users;
  request_all_users(&users);
  printf("All active users: %d\n", users.active_clients);
  REP(i, users.active_clients){
    printf("user '%s'\n", users.names[i]);
  }
}


void handle_room_client_list(){
  ROOM_CLIENT_LIST_RESPONSE users;
  request_users_here(&users);
  printf("Active users on this channel: %d\n", users.active_clients);
  REP(i, users.active_clients){
    printf("user '%s'\n", users.names[i]);
  }
}


void handle_room_list(){
  ROOM_LIST_RESPONSE rooms;
  if( request_room_list(&rooms) == -1)
    printf("Your server is dead bro...\n");
  else{
    printf("Active rooms: %d\n", rooms.active_rooms);
    REP(i, rooms.active_rooms){
      printf("  room: [%s] - %d users\n", rooms.rooms[i].name, rooms.rooms[i].clients);
    }
  }
}


void handle_join(char * input){
  char channel[2000];
  sscanf(input, "%s", channel);

  STATUS_RESPONSE status;
  request_join_room(&status, channel);
  if( status.status == 202)
    printf("Welcome to the '%s' channel!\n", channel);
}


void handle_logout(){
  if( request_logout() == -1)
    printf("You are not signed in you fuckard\n");
  else
    printf("Goodbye, %s ;(\n", my_login);
}


void handle_login(char * input){
  char login[200];
  int server;
  sscanf(input, "%s %d", login, &server);

  STATUS_RESPONSE status;
  if( request_login(&status, login, server) == -1)
    printf("Im sorry, there is no server with id %d\n", server);
  else{
    if(status.status == 503)
      printf("Im sorry, server is full\n");
    if(status.status == 409)
      printf("Im sorry, user with username '%s', already exists\n", login);
    if(status.status == 201){
      printf("Successfully logged in as '%s'\n", login);
      strcpy(my_login, login);
      my_server = server;
    }
  }
}



void handle_unknown(){
  printf("Unknown command, try /help to see all commands\n");
}

void handle_help(){
  printf("Commands list:\n");
  printf("   /help  - show help\n");
  printf("   /server_list  - show server list\n");
  printf("   /client_list  - show client list\n");
  printf("   /login <LOGIN> <SERVER> - do login\n");
  printf("   /logout  - do logout\n");
  printf("   /exit  - exit chat\n");
}

void handle_exit(){
  printf("Exiting...\n");
  exit(1);
}
