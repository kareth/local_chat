#include "libraries.h"

#define TIMED(X)  time_t start = time(0);int flag = 0;while(1){ if(time(0)-start >= TIMEOUT){ server_timeout(); flag = -1; break;}int result=X;if( result == -1) continue; break; }
#define TIMEX(X)  time_t start = time(0);int flag = 0;while(1){ if(time(0)-start >= TIMEOUT){ everys_timeout(); flag = -1; break;}int result=X;if( result == -1) continue; break; }


int my_server;
char my_login[MAX_NAME_SIZE];
int pids[200], pc, my_queue;
int client_repo, sem;
CLIENT *my_info;

char color_red[30], color_green[30], color_blue[30],color_yellow[30], color_white[30], color_crystal[30], color_purple[30];


void semP(int id){
  struct sembuf buf;
  buf.sem_flg = 0; buf.sem_num = 0; buf.sem_op = -1;
  semop(id, &buf, 1);
}

void semV(int id){
  struct sembuf buf;
  buf.sem_flg = 0; buf.sem_num = 0; buf.sem_op = 1;
  semop(id, &buf, 1);
}

void info_on(){ semP(sem); }
void info_off(){semV(sem); }

void server_timeout(){
  printf("%s\n> timeout!\n", color_red);

  printf("%s> Your server is dead, login again.\n", color_yellow);
  printf("\n%s", color_white);

  info_on();
    my_server = INF;
    strcpy(my_login, NONAME);
    my_info->server_id = INF;
    strcpy(my_info->name, NONAME);
  info_off();
}

void everys_timeout(){
  printf("\ntimeout!\n");
  printf("\n%s", color_white);
}


//SERVER LIST
int request_server_list(SERVER_LIST_RESPONSE *servers){
  int list_id = msgget(SERVER_LIST_MSG_KEY, 0666);
  if( list_id == -1) return -1;

  SERVER_LIST_REQUEST req;
  req.type = SERVER_LIST;
  req.client_msgid = my_queue;

  msgsnd(list_id, &req, sizeof(req)-sizeof(long), 0);

  TIMEX(msgrcv(my_queue, servers, sizeof(SERVER_LIST_RESPONSE)-sizeof(long), SERVER_LIST, 0));
  return 0;
}

// LOGIN
int request_login(STATUS_RESPONSE *status, char * login, int server){
  CLIENT_REQUEST req;
  req.type = LOGIN;
  req.client_msgid = my_queue;
  strcpy(req.client_name, login);

  msgsnd(server, &req, sizeof(CLIENT_REQUEST)-sizeof(long), 0);

  msgrcv(my_queue, status, sizeof(STATUS_RESPONSE)-sizeof(long), STATUS, 0);

  if(status->status == 201){
  info_on();
    strcpy(my_login, login);
    my_server = server;

    strcpy(my_info->name, my_login);
    my_info->server_id = my_server;
  info_off();
  }
  return 0;
}

// LOGOUT
int request_logout(){
  if(my_server == INF) return -1;
  CLIENT_REQUEST req;
  req.type = LOGOUT;
  req.client_msgid = my_queue;
  strcpy(req.client_name, my_login);

  msgsnd(my_server, &req, sizeof(CLIENT_REQUEST)-sizeof(long), 0);

  info_on();
    my_server = INF;
    strcpy(my_login, NONAME);
    my_info->server_id = INF;
    strcpy(my_info->name, NONAME);
  info_off();
  return 0;
}

// JOIN ROOM
int request_join_room(STATUS_RESPONSE *status, char * channel){
  if(my_server == INF) return -1;
  CHANGE_ROOM_REQUEST req;
  req.type = CHANGE_ROOM;
  req.client_msgid = my_queue;
  strcpy(req.room_name, channel);
  strcpy(req.client_name, my_login);

  msgsnd(my_server, &req, sizeof(CHANGE_ROOM_REQUEST)-sizeof(long), 0);
  TIMED( msgrcv(my_queue, status, sizeof(STATUS_RESPONSE)-sizeof(long), CHANGE_ROOM, IPC_NOWAIT) );
  return flag;
}

// ROOM LIST
int request_room_list(ROOM_LIST_RESPONSE *list){
  if(my_server == INF) return -1;
  CLIENT_REQUEST req;
  req.type = ROOM_LIST;
  req.client_msgid = my_queue;
  strcpy(req.client_name, my_login);

  msgsnd(my_server, &req, sizeof(CLIENT_REQUEST)-sizeof(long), 0);
  TIMED( msgrcv(my_queue, list, sizeof(ROOM_LIST_RESPONSE)-sizeof(long), ROOM_LIST, 0));
  return flag;
}

// ROOM USERS LIST
int request_users_here(ROOM_CLIENT_LIST_RESPONSE *users){
  if(my_server == INF) return -1;
  CLIENT_REQUEST req;
  req.type = ROOM_CLIENT_LIST;
  req.client_msgid = my_queue;
  strcpy(req.client_name, my_login);

  msgsnd(my_server, &req, sizeof(CLIENT_REQUEST)-sizeof(long), 0);
  TIMED(msgrcv(my_queue, users, sizeof(ROOM_CLIENT_LIST_RESPONSE)-sizeof(long), ROOM_CLIENT_LIST, 0));
  return flag;
}

// GLOBAL USERS LIST
int request_all_users(GLOBAL_CLIENT_LIST_RESPONSE *users){
  if(my_server == INF) return -1;
  CLIENT_REQUEST req;
  req.type = GLOBAL_CLIENT_LIST;
  req.client_msgid = my_queue;
  strcpy(req.client_name, my_login);

  msgsnd(my_server, &req, sizeof(CLIENT_REQUEST)-sizeof(long), 0);
  TIMED(msgrcv(my_queue, users, sizeof(GLOBAL_CLIENT_LIST_RESPONSE)-sizeof(long), GLOBAL_CLIENT_LIST, 0));
  return flag;
}

// SEND MESSAGE
int send_message(char * text){
  if(my_server == INF) return -1;
  TEXT_MESSAGE req;
  req.type = PUBLIC;
  req.from_id = my_queue;
  req.time = time(0);
  strcpy(req.from_name, my_login);
  strcpy(req.text, text);

  msgsnd(my_server, &req, sizeof(TEXT_MESSAGE)-sizeof(long), 0);
  return 0;
}

// WHISPER
int send_whisper(char * user, char * text){
  if(my_server == INF) return -1;
  TEXT_MESSAGE req;
  req.type = PRIVATE;
  req.from_id = my_queue;
  req.time = time(0);
  strcpy(req.from_name, my_login);
  strcpy(req.to, user);
  strcpy(req.text, text);

  msgsnd(my_server, &req, sizeof(TEXT_MESSAGE)-sizeof(long), 0);
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
    int res = msgrcv(my_queue, &msg, sizeof(TEXT_MESSAGE)-sizeof(long), PUBLIC, 0);
    if(res == -1) break; // prevent loop on ctrlC
    char *s = ctime(&msg.time);
    *(s+24) = 0;
    printf("%s\n> [@%s] [%s] > '%s'\n", color_crystal, s, msg.from_name, msg.text);
  }
  printf("%s\n", color_white);
}

void wait_for_private(){
  TEXT_MESSAGE msg;
  while (1) {
    int res = msgrcv(my_queue, &msg, sizeof(TEXT_MESSAGE)-sizeof(long), PRIVATE, 0);
    if(res == -1) break; // prevent loop on ctrlC
    char *s = ctime(&msg.time);
    *(s+24) = 0;
    printf("%s\n> [@%s] [%s] > '%s'\n", color_purple, s, msg.from_name, msg.text);
  }
  printf("%s\n", color_white);
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


void heartbeat(){
  STATUS_RESPONSE hb;

  CLIENT_REQUEST req;
  req.type = HEARTBEAT;
  req.client_msgid = my_queue;

  while (1) {
    int res = msgrcv(my_queue, &hb, sizeof(STATUS_RESPONSE)-sizeof(long), HEARTBEAT, 0);
    if(res == -1) break; // prevent loop on ctrlC - not sure if necessary here

    info_on();
      if( my_info->server_id != hb.status) continue;
      strcpy(req.client_name, my_info->name);
    info_off();

    msgsnd(hb.status, &req, sizeof(CLIENT_REQUEST)-sizeof(long), 0);
  }
}

// ---------------------------------------------- MAIN --------------------------------

void init_client_memo(){
  pc = 0;
  my_server = INF;
  my_queue = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);

  sem = semget(IPC_PRIVATE, 1, 0666 | IPC_CREAT);
  semctl(sem, 0, SETVAL, 0);

  client_repo = shmget(IPC_PRIVATE, sizeof(REPO), 0666 | IPC_CREAT);
  CLIENT *mem = shmat(client_repo, NULL, 0);
  CLIENT c;
  *mem = c;
  shmdt(mem);

  my_info = shmat(client_repo, NULL, 0);
  info_off();
}

void init_colors(){
  strcpy(color_red, "\033[22;31m");
  strcpy(color_green, "\033[22;32m");
  strcpy(color_blue, "\033[22;36m");
  strcpy(color_yellow, "\033[01;33m");
  strcpy(color_white, "\033[22;37m");
  strcpy(color_crystal, "\033[01;37m");
  strcpy(color_purple, "\033[22;35m");
}

int main() {
  init_colors();
  srand(time(0));
  signal(SIGINT, close_client);
  signal(SIGTERM, close_client);
  init_client_memo();
  if( handle_server_list() == -1) exit(1);

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
  if(my_server == INF)
    printf("%s \n> Sign in to send message!\n\n%s", color_red, color_white);
  else
    send_message(input);

  printf("\n%s", color_white);
}

int handle_server_list(){
  SERVER_LIST_RESPONSE servers;
  if( request_server_list(&servers) == -1){
    printf("%s \n> No servers up.\n\n", color_red);
    return -1;
  }
  else{
    printf("%s \n> Active servers: %d\n", color_blue, servers.active_servers);
    REP(i, servers.active_servers)
      printf("%s> - Server %d: %d users online\n ", color_blue, servers.servers[i], servers.clients_on_servers[i]);
    printf("\n%s", color_white);
  }
  return 1;
}

void handle_whisper(char * input){
  if(my_server == INF){
    printf("%s \n> Sign in to send message!\n\n%s", color_red, color_white);
    return;
  }
  char target[2000];
  sscanf(input, "%s", target);
  memmove(input, input+strlen(target)+2, strlen(input));

  send_whisper(target, input);
  printf("\n%s", color_white);
}

void handle_global_client_list(){
  GLOBAL_CLIENT_LIST_RESPONSE users;
  if(request_all_users(&users) == -1)
    printf("%s \n> Cannot fetch users list\n", color_red);
  else{
    printf("%s \n> All active users: %d\n", color_blue, users.active_clients);
    REP(i, users.active_clients){
      printf("%s> - user: '%s'\n", color_blue, users.names[i]);
    }
  }
  printf("\n%s", color_white);
}


void handle_room_client_list(){
  ROOM_CLIENT_LIST_RESPONSE users;
  if(request_users_here(&users) == -1)
    printf("%s \n> Cannot fetch users list\n", color_red);
  else{
    printf("%s \n> Active users on this channel: %d\n", color_blue, users.active_clients);
    REP(i, users.active_clients){
      printf("%s> - user '%s'\n", color_blue, users.names[i]);
    }
  }
  printf("\n%s", color_white);
}


void handle_room_list(){
  ROOM_LIST_RESPONSE rooms;
  if( request_room_list(&rooms) == -1)
    printf("%s \n> Cannot fetch room list\n", color_red);
  else{
    printf("%s \n> Active rooms: %d\n",color_blue, rooms.active_rooms);
    REP(i, rooms.active_rooms){
      printf("%s> - room: [%s] - %d users\n", color_blue, rooms.rooms[i].name, rooms.rooms[i].clients);
    }
  }
  printf("\n%s", color_white);
}


void handle_join(char * input){
  char channel[2000];
  sscanf(input, "%s", channel);

  STATUS_RESPONSE status;
  request_join_room(&status, channel);
  if( status.status == 202)
    printf("%s\n> Welcome to the '%s' channel!\n",color_green, channel);
  else
    printf("%s\n> something went wrong!\n",color_red);
  printf("\n%s", color_white);
}


void handle_logout(){
  if( request_logout() == -1)
    printf("%s\n> You are not signed in!\n\n", color_red);
  else
    printf("%s\n> Goodbye ;(\n", color_green);
  printf("\n%s", color_white);
}


void handle_login(char * input){
  char login[2000];
  int server;
  sscanf(input, "%s %d", login, &server);

  STATUS_RESPONSE status;
  if( request_login(&status, login, server) == -1)
    printf("%s\n> Im sorry, there is no server with id %d\n", color_red, server);
  else{
    if(status.status == 503)
      printf("%s\n> Im sorry, server is full\n", color_red);
    if(status.status == 409)
      printf("%s\n> Im sorry, user with username '%s', already exists\n", color_red, login);
    if(status.status == 400)
      printf("%s\n> Im sorry, invalid username\n", color_red);
    if(status.status == 201){
      printf("%s\n> Successfully logged in as '%s'\n", color_green, login);
    }
  }
  printf("\n%s", color_white);
}

void handle_unknown(){
  printf("%s\n> Unknown command, try /help to see all commands\n", color_yellow);
  printf("\n%s", color_white);
}

void handle_help(){
  printf("%s\n> Commands list:\n", color_blue);
  printf("%s>   /help  - show help\n", color_blue);
  printf("%s>   /server_list  - show server list\n", color_blue);
  printf("%s>   /users_here  - show clients on this channel\n", color_blue);
  printf("%s>   /all_users  - show all users - on any channel\n", color_blue);
  printf("%s>   /login <LOGIN> <SERVER> - do login\n", color_blue);
  printf("%s>   /logout  - do logout\n", color_blue);
  printf("%s>   /exit  - exit chat\n", color_blue);
  printf("\n%s", color_white);
}

void handle_exit(){
  printf("%s\n> Exiting...\n", color_green);
  printf("\n%s", color_white);
  close_client();
  exit(1);
}
