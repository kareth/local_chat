#include "libraries.h"

int my_server;
char my_login[2000];
int pids[200], pc, my_queue;

// REQUESTS


//SERVER LIST
int request_server_list(SERVER_LIST_RESPONSE *servers){
  int list_id = msgget(SERVER_LIST_MSG_KEY, 0666);
  if( list_id == -1) return -1;

  SERVER_LIST_REQUEST req;
  req.type = SERVER_LIST;
  req.client_msgid = my_queue;

  msgsnd(list_id, &req, sizeof(req), 0);

  SERVER_LIST_RESPONSE res;
  msgrcv(my_queue, &res, sizeof(res), SERVER_LIST, 0);

  *servers = res;
  return 0;
}

// LOGIN
int request_login(STATUS_RESPONSE *status, char * login, int server){
  CLIENT_REQUEST req;
  req.type = LOGIN;
  req.client_msgid = my_queue;
  strcpy(req.client_name, login);

  msgsnd(server, &req, sizeof(req), 0);

  STATUS_RESPONSE res;
  printf("wait_on: %d\n", my_queue);
  if ( msgrcv(my_queue, &res, sizeof(res), STATUS, 0) == -1) perror("error");

  *status = res;
  return 0;
}

// LOGOUT
int request_logout(){

  CLIENT_REQUEST req;
  req.type = LOGOUT;
  req.client_msgid = my_queue;
  strcpy(req.client_name, my_login);

  msgsnd(my_server, &req, sizeof(req), 0);

  my_server = INF;
  return 0;
}

// JOIN ROOM
int request_join_room(STATUS_RESPONSE *status, char * channel){

  CHANGE_ROOM_REQUEST req;
  req.type = CHANGE_ROOM;
  req.client_msgid = my_queue;
  strcpy(req.room_name, channel);
  strcpy(req.client_name, my_login);

  msgsnd(my_server, &req, sizeof(req), 0);

  STATUS_RESPONSE res;
  msgrcv(my_queue, &res, sizeof(res), STATUS, 0);

  *status = res;
  return 0;
}


// ROOM LIST
int request_room_list(ROOM_LIST_RESPONSE *list){
  CLIENT_REQUEST req;
  req.type = ROOM_LIST;
  req.client_msgid = my_queue;
  strcpy(req.client_name, my_login);

  msgsnd(my_server, &req, sizeof(req), 0);

  ROOM_LIST_RESPONSE res;
  msgrcv(my_queue, &res, sizeof(res), ROOM_LIST, 0);

  *list = res;
  return 0;
}


// USERS LIST
int request_users_here(ROOM_CLIENT_LIST_RESPONSE *users){
  CLIENT_REQUEST req;
  req.type = ROOM_CLIENT_LIST;
  req.client_msgid = my_queue;
  strcpy(req.client_name, my_login);

  msgsnd(my_server, &req, sizeof(req), 0);

  ROOM_CLIENT_LIST_RESPONSE res;
  msgrcv(my_queue, &res, sizeof(res), ROOM_CLIENT_LIST, 0);

  *users = res;
  return 0;
}

int request_all_users(GLOBAL_CLIENT_LIST_RESPONSE *users){
  CLIENT_REQUEST req;
  req.type = GLOBAL_CLIENT_LIST;
  req.client_msgid = my_queue;
  strcpy(req.client_name, my_login);

  msgsnd(my_server, &req, sizeof(req), 0);

  GLOBAL_CLIENT_LIST_RESPONSE res;
  msgrcv(my_queue, &res, sizeof(res), GLOBAL_CLIENT_LIST, 0);

  *users = res;
  return 0;
}



// SEND MESSAGE

int send_message(char * text){
  TEXT_MESSAGE req;
  req.type = PUBLIC;
  req.from_id = my_queue;
  req.time = time(0);
  strcpy(req.from_name, my_login);
  strcpy(req.text, text);

  msgsnd(my_server, &req, sizeof(req), 0);
  return 0;
}

int send_whisper(char * user, char * text){
  TEXT_MESSAGE req;
  req.type = PRIVATE;
  req.from_id = my_queue;
  req.time = time(0);
  strcpy(req.from_name, my_login);
  strcpy(req.to, user);
  strcpy(req.text, text);

  msgsnd(my_server, &req, sizeof(req), 0);
  return 0;
}



// Others

void close_client(){
  request_logout();
  REP(i, pc)
    if(pids[i] != 0)
      kill(pids[i], SIGKILL);
  msgctl(my_queue, IPC_RMID, 0);
}

void wait_for_public(){
  TEXT_MESSAGE msg;
  while (1) {
    int res = msgrcv(my_queue, &msg, sizeof(msg), PUBLIC, 0);
    if(res == -1) break; // prevent loop on ctrlC
    printf("\"%s\"@%s:%s\n", msg.from_name, ctime(&msg.time), msg.text);
  }
}

void wait_for_private(){
  TEXT_MESSAGE msg;
  while (1) {
    int res = msgrcv(my_queue, &msg, sizeof(msg), PRIVATE, 0);
    if(res == -1) break; // prevent loop on ctrlC
    printf("WHISP:\"%s\"@%s:%s\n", msg.from_name, ctime(&msg.time), msg.text);
  }
}

void handle_command(char * cd, char * in);
void handle_send_msg(char * in);

int handle_server_list();
void handle_help();
void handle_login(char * input);
void handle_logout();
void handle_join();
void handle_whisper();
void handle_exit();
void handle_unknown();
void handle_room_list();
void handle_room_client_list();
void handle_global_client_list();


void heartbeat(){}

// MAIN

int main() {
  srand(time(0));
  signal(SIGINT, close_client);
  signal(SIGTERM, close_client);

  my_server = INF;
  my_queue = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
  if( handle_server_list() == -1) exit(1);
  pc = 0;

  if((pids[pc++] = fork()) == 0)
    wait_for_public();
  else if ((pids[pc++] = fork()) == 0)
    wait_for_private();
  else if ((pids[pc++] = fork()) == 0)
    heartbeat();
  else{
    char input[3000];
    char command[3000];

    while(1){
      //printf("> ");
      scanf("%s",command);
      gets(input);
      if( command[0] == '/') handle_command(command, input);
      else handle_send_msg( strcat(command, input));
    }
    close_client();
  }
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

int handle_server_list(){
  SERVER_LIST_RESPONSE servers;
  if( request_server_list(&servers) == -1){
    printf("No servers up.\n");
    return -1;
  }
  else{
    printf("Active servers: %d\n", servers.active_servers);
    REP(i, servers.active_servers)
      printf("Server %d: %d users online\n", servers.servers[i], servers.clients_on_servers[i]);
  }
  return 1;
}

void handle_whisper(char * input){
  char target[2000];
  sscanf(input, "%s", target);
  memmove(input, input+strlen(target)+2, strlen(input));

  send_whisper(target, input);
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
  close_client();
  exit(1);
}
