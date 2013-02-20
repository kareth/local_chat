#include "libraries.h"

int my_server;
char my_login[2000];

// REQUESTS


//SERVER LIST
int request_server_list(SERVER_LIST_RESPONSE *servers){
  int list_id = msgget(SERVER_LIST_MSG_KEY, 0666);
  if( list_id == -1) return -1;
  int my_id = msgget(getpid(), 0666 | IPC_CREAT);

  SERVER_LIST_REQUEST req;
  req.type = SERVER_LIST;
  req.client_msgid = getpid();

  msgsnd(list_id, &req, sizeof(req), 0);
  my_id = msgget(getpid(), 0666 | IPC_CREAT);

  SERVER_LIST_RESPONSE res;
  msgrcv(my_id, &res, sizeof(res), SERVER_LIST, 0);

  *servers = res;
  return 1;
}

// LOGIN
int request_login(STATUS_RESPONSE *status, char * login, int server){
  int server_id = msgget(server, 0666);
  if( server_id == -1) return -1;
  int my_id = msgget(getpid(), 0666 | IPC_CREAT);

  CLIENT_REQUEST req;
  req.type = LOGIN;
  req.client_msgid = getpid();
  strcpy(req.client_name, login);

  msgsnd(server_id, &req, sizeof(req), 0);

  STATUS_RESPONSE res;
  msgrcv(my_id, &res, sizeof(res), STATUS, 0);

  *status = res;
  return 1;
}

// LOGOUT
int request_logout(){
  int server_id = msgget(my_server, 0666);
  if( server_id == -1) return -1;

  CLIENT_REQUEST req;
  req.type = LOGOUT;
  req.client_msgid = getpid();
  strcpy(req.client_name, my_login);

  msgsnd(server_id, &req, sizeof(req), 0);

  my_server = INF;
  return 1;
}

// JOIN ROOM
int request_join_room(STATUS_RESPONSE *status, char * channel){
  int server_id = msgget(my_server, 0666);
  if( server_id == -1) return -1;

  int my_id = msgget(getpid(), 0666 | IPC_CREAT);

  CHANGE_ROOM_REQUEST req;
  req.type = CHANGE_ROOM;
  req.client_msgid = getpid();
  strcpy(req.room_name, channel);
  strcpy(req.client_name, my_login);

  msgsnd(server_id, &req, sizeof(req), 0);

  STATUS_RESPONSE res;
  msgrcv(my_id, &res, sizeof(res), STATUS, 0);

  *status = res;
  return 1;
}

// ROOM LIST
int request_room_list(ROOM_LIST_RESPONSE *list){
  int server_id = msgget(my_server, 0666);
  if( server_id == -1) return -1;

  int my_id = msgget(getpid(), 0666 | IPC_CREAT);

  CLIENT_REQUEST req;
  req.type = ROOM_LIST;
  req.client_msgid = getpid();
  strcpy(req.client_name, my_login);

  msgsnd(server_id, &req, sizeof(req), 0);

  ROOM_LIST_RESPONSE res;
  if( msgrcv(my_id, &res, sizeof(res), ROOM_LIST, 0) == -1) perror("error");

  *list = res;
  return 1;
}


// Others

void close_client(){
  int id = msgget(getpid(), 0666);
  msgctl(id, IPC_RMID, 0);
}

void handle_command(char * cd, char * in);
void handle_send_msg(char * in);

void handle_help();
void handle_server_list();
void handle_client_list();
void handle_login(char * input);
void handle_logout();
void handle_status();
void handle_join();
void handle_whisper();
void handle_exit();
void handle_unknown();
void handle_room_list();




// MAIN

int main() {
  signal(SIGINT, close_client);
  signal(SIGTERM, close_client);

  my_server = INF;

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

  char input[3000];
  char command[3000];

  while(1){
    printf("> ");
    scanf("%s",command);
    gets(input);
    if( command[0] == '/') handle_command(command, input);
    else handle_send_msg( strcat(command, input));
  }

  close_client();
  return 0;
}





// Commands handlers

void handle_command(char * cd, char * in){
       if( !strcmp(cd, "/help") )        handle_help();
  else if( !strcmp(cd, "/server_list") ) handle_server_list();
  else if( !strcmp(cd, "/client_list") ) handle_client_list();
  else if( !strcmp(cd, "/login") )       handle_login(in);
  else if( !strcmp(cd, "/room_list") )   handle_room_list();
  else if( !strcmp(cd, "/logout") )      handle_logout();
  else if( !strcmp(cd, "/status") )      handle_status();
  else if( !strcmp(cd, "/join") )        handle_join(in);
  else if( !strcmp(cd, "/w") )           handle_whisper(in);
  else if( !strcmp(cd, "/exit") )        handle_exit();
  else                                  handle_unknown();
}

void handle_send_msg(char * input){

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

void handle_client_list(){}
void handle_status(){}
void handle_whisper(){}


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
