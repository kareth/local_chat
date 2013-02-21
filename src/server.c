#include "libraries.h"

int rid, rsem, lsem;
int mc, ms;
REPO *repo;

int my_server, my_client;

char user_name[50][50];
int user_msg[50];
int user_count = 0;

void add_local_user(char * name, int id){
  strcpy(user_name[user_count], name);
  user_msg[user_count] = id;
  user_count++;
  printf("Adding user: %s %d\n", name, id);
}

void remove_local_user(char * name){
  REP(i, user_count)
    if( !strcmp(user_name[i], name)){
      if( i != user_count - 1){
        strcpy(user_name[i], user_name[user_count-1]);
        user_msg[i] = user_msg[user_count-1];
      }

      user_count--;
      break;
    }
}

int get_user_id(char * name){
  printf("checking user: %s\n",name);
  REP(i, user_count)
    if( !strcmp(user_name[i], name)){
      return user_msg[i];
    }
  return -1;
}


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

void repo_on(){ semP(rsem); }
void repo_off(){semV(rsem); }

// Logger

void logger(const char * format, ... ){
  semP(lsem);
  va_list args;

  //int fd = open("/tmp/czat.log", O_WRONLY|O_CREAT|O_APPEND);
  va_start(args, format);
  vprintf(format, args);
  va_end(args);

  //close(fd);
  semV(lsem);
}

int comp_s(const void *a, const void *b){
  if(((SERVER*)a)->client_msgid > ((SERVER*)b)->client_msgid) return 1;
  if(((SERVER*)b)->client_msgid > ((SERVER*)a)->client_msgid) return -1;
  return 0;
}

int comp_c(const void *a, const void *b){
  return strcmp( ((CLIENT*)a)->name, ((CLIENT*)b)->name);
}

int comp_r(const void *a, const void *b){
  return strcmp( ((ROOM*)a)->name, ((ROOM*)b)->name);
}

// Servers

void register_server(){
  srand(time(0));
  repo_on();
  my_client = rand()%2000000009;
  my_server = rand()%2000000009;
  printf("server_Q: %d\n", my_server);

  my_client = msgget(my_client, 0666 | IPC_CREAT);
  my_server = msgget(my_server, 0666 | IPC_CREAT);
  msgget(SERVER_LIST_MSG_KEY, 0666 | IPC_CREAT);

  repo->servers[repo->active_servers].client_msgid = my_client;
  repo->servers[repo->active_servers].server_msgid = my_server;
  repo->servers[repo->active_servers].clients = 0;

  repo->active_servers++;
  printf("active_servers: %d\n", repo->active_servers);

  qsort(repo->servers, MAX_SERVER_NUM, sizeof(SERVER), comp_s);
  repo_off();
}


// Repository

void lsem_init() {
  lsem = semget(SEM_LOG, 1, 0666 | IPC_CREAT | IPC_EXCL);
  if(lsem == -1) lsem = semget(SEM_LOG, 1, 0666);
  else semctl(lsem, 0, SETVAL, 1);
}

void rsem_init() {
  rsem = semget(SEM_REPO, 1, 0666 | IPC_CREAT | IPC_EXCL);
  if(rsem == -1) rsem = semget(SEM_REPO, 1, 0666);
  else semctl(rsem, 0, SETVAL, 0);
}

void rmem_init() {
  rid = shmget(ID_REPO, sizeof(REPO), 0666 | IPC_CREAT | IPC_EXCL);
  if( rid == -1){
    rid = shmget(ID_REPO, 1, 0666);
    return;
  }

  REPO *mem = shmat(rid, NULL, 0);

  REPO clean;
  clean.active_clients = 0;
  clean.active_rooms = 0;
  clean.active_servers = 0;
  REP(i, MAX_SERVER_NUM) clean.servers[i].client_msgid = INF;
  REP(j, MAX_CLIENTS) strcpy( clean.clients[j].name, NONAME );
  REP(k, MAX_CLIENTS){
    strcpy( clean.rooms[k].name, NONAME );
    clean.rooms[k].clients = 0;
  }

  *mem = clean;
  shmdt(mem);
  semV(rsem);
}

void repo_init(){
  lsem_init();
  rsem_init();
  rmem_init();
}

void fetch_repo(){
  repo_on();
  repo = shmat(rid, NULL, 0);
  repo_off();
}

void close_repo(){
  repo_on();
  logger("DOWN: %d\n", getpid());

  int my_id = msgget(getpid(), 0666);
  msgctl(my_id, IPC_RMID, 0);

  if(repo->active_servers == 1) {
    printf("should be no\n");
    int list_id = msgget(SERVER_LIST_MSG_KEY, 0666);
    int repo_id = shmget(ID_REPO, sizeof(REPO), 0666);

    msgctl(list_id, IPC_RMID, 0);
    semctl(rsem, IPC_RMID, 0);
    semctl(lsem, IPC_RMID, 0);
    shmctl(repo_id, IPC_RMID, 0);
  } else {
    repo->active_servers--;
    printf("active_servers: %d\n", repo->active_servers);

    REP(i, MAX_SERVER_NUM){
      if(repo->servers[i].client_msgid == getpid())  {
        repo->servers[i].client_msgid = INF;
        break;
      }
    }

    qsort(repo->servers, MAX_SERVER_NUM, sizeof(SERVER), comp_s);
    shmdt(repo);
    repo_off();
  }
}

void close_server(){
  close_repo();
  msgctl(my_client, IPC_RMID, 0);
  msgctl(my_server, IPC_RMID, 0);
  exit(1);
}

// Requests


// SERVER_LIST
void server_list_request() {
  SERVER_LIST_REQUEST req;
  int list_id = msgget(SERVER_LIST_MSG_KEY, 0666);
  int result = msgrcv(list_id, &req, sizeof(req), SERVER_LIST, IPC_NOWAIT);
  if(result == -1) return;
  printf("request: SERVER_LIST\n");

  SERVER_LIST_RESPONSE res;
  res.type = SERVER_LIST;
  res.active_servers = repo->active_servers;

  REP(i, repo->active_servers) {
    res.servers[i] = repo->servers[i].client_msgid;
    res.clients_on_servers[i] = repo->servers[i].clients;
  }
  msgsnd(req.client_msgid, &res, sizeof(res), 0);
}



// LOGIN
int validate_user(char * login){
  REP(i, repo->active_servers)
    if( repo->servers[i].client_msgid == my_client){
      if( repo->servers[i].clients == SERVER_CAPACITY) return 503;
      else break;
    }

  REP(j, repo->active_clients)
    if( !strcmp( repo->clients[j].name, login))
      return 409;

  return 201;
}

void login_request() {
  CLIENT_REQUEST req;
  int result = msgrcv(my_client, &req, sizeof(req), LOGIN, IPC_NOWAIT);
  if(result == -1) return;

  printf("request: LOGIN (%s)\n", req.client_name);

  STATUS_RESPONSE res;
  res.type = STATUS;

  repo_on();
  res.status = validate_user(req.client_name);

  if( res.status == 201){
    int found = 0;
    REP(i, repo->active_rooms)
      if( !strcmp(repo->rooms[i].name, "") ){
        found = 1;
        repo->rooms[i].clients++;
      }

    if(found == 0){
      strcpy(repo->rooms[repo->active_rooms].name, "");
      repo->rooms[repo->active_rooms].clients = 1;
      repo->active_rooms++;
    }

    strcpy(repo->clients[repo->active_clients].name, req.client_name);
    repo->clients[repo->active_clients].server_id = my_client;
    strcpy(repo->clients[repo->active_clients].room, "");

    repo->active_clients++;
    qsort(repo->clients, MAX_CLIENTS, sizeof(CLIENT), comp_c);

    add_local_user(req.client_name, req.client_msgid);
  }
  repo_off();

  printf("USERS:\n");
  REP(i, repo->active_clients){
    printf("user: '%s', '%d', '%s' \n",repo->clients[i].name, repo->clients[i].server_id, repo->clients[i].room);
  }

  printf("answer_to: %d\n", req.client_msgid);
  msgsnd(req.client_msgid, &res, sizeof(res), 0);
  return;
}



// LOGOUT
void logout_request() {
  CLIENT_REQUEST req;
  int result = msgrcv(my_client, &req, sizeof(req), LOGOUT, IPC_NOWAIT);
  if(result == -1) return;

  printf("request: LOGOUT (%s)\n", req.client_name);

  repo_on();

  REP(i, repo->active_clients)
    if( !strcmp( repo->clients[i].name, req.client_name))
      strcpy( repo->clients[i].name, NONAME);

  repo->active_clients--;

  qsort(repo->clients, MAX_CLIENTS, sizeof(CLIENT), comp_c);

  remove_local_user(req.client_name);

  repo_off();
  return;
}

// JOIN ROOM
void join_room_request() {
  CHANGE_ROOM_REQUEST req;
  int result = msgrcv(my_client, &req, sizeof(req), CHANGE_ROOM, IPC_NOWAIT);
  if(result == -1) return;

  printf("request: CHANGE_ROOM (%s) -> (%s)\n", req.client_name, req.room_name);

  repo_on();

  char old_room[2000];
  REP(i, repo->active_clients)
    if( !strcmp( repo->clients[i].name, req.client_name)){
      strcpy( old_room, repo->clients[i].room);
      strcpy( repo->clients[i].room, req.room_name);
    }

  REP(j, repo->active_rooms)
    if( !strcmp(repo->rooms[j].name, old_room)){
      repo->rooms[j].clients--;
      if( repo->rooms[j].clients == 0)
        strcpy(repo->rooms[j].name, NONAME);
    }

  int found = 0;
  REP(k, repo->active_rooms)
    if( !strcmp(repo->rooms[k].name, req.room_name)){
      found = 1;
      repo->rooms[k].clients++;
    }

  if( found == 0){
    strcpy(repo->rooms[repo->active_rooms].name, req.room_name);
    repo->rooms[repo->active_rooms].clients = 1;
    repo->active_rooms++;
  }

  qsort(repo->rooms, MAX_CLIENTS, sizeof(ROOM), comp_r);

  repo_off();

  STATUS_RESPONSE res;
  res.type = STATUS;
  res.status = 202;

  msgsnd(req.client_msgid, &res, sizeof(res), 0);

  return;
}


// ROOM LIST

void room_list_request() {
  CLIENT_REQUEST req;
  int result = msgrcv(my_client, &req, sizeof(req), ROOM_LIST, IPC_NOWAIT);
  if(result == -1) return;

  printf("request: ROOM_LIST\n");

  repo_on();
  ROOM_LIST_RESPONSE res;
  res.type = ROOM_LIST;
  res.active_rooms = repo->active_rooms;

  REP(i, MAX_CLIENTS){
    ROOM room;
    strcpy(room.name, repo->rooms[i].name);
    room.clients = repo->rooms[i].clients;
    res.rooms[i] = room;
  }

  repo_off();

  msgsnd(req.client_msgid, &res, sizeof(res), 0);
}

// GLOBAL USERS LIST
//
void all_users_request(){
  CLIENT_REQUEST req;
  int result = msgrcv(my_client, &req, sizeof(req), GLOBAL_CLIENT_LIST, IPC_NOWAIT);
  if(result == -1) return;

  printf("request: GLOBAL_USERS_LIST\n");

  repo_on();

  GLOBAL_CLIENT_LIST_RESPONSE res;
  res.type = GLOBAL_CLIENT_LIST;
  res.active_clients = repo->active_clients;

  REP(i, repo->active_clients)
    strcpy(res.names[i], repo->clients[i].name);

  repo_off();

  msgsnd(req.client_msgid, &res, sizeof(res), 0);
}

// ROOM USERS LIST

void users_here_request(){
  CLIENT_REQUEST req;
  int result = msgrcv(my_client, &req, sizeof(req), ROOM_CLIENT_LIST, IPC_NOWAIT);
  if(result == -1) return;

  printf("request: ROOM_USERS_LIST\n");

  repo_on();

  ROOM_CLIENT_LIST_RESPONSE res;
  res.type = ROOM_CLIENT_LIST;

  char user_room[2000];
  REP(i, repo->active_clients)
    if(!strcmp(repo->clients[i].name, req.client_name)){
      strcpy(user_room, repo->clients[i].room);
      break;
    }

  int count = 0;

  REP(j, repo->active_clients)
    if( !strcmp(user_room, repo->clients[j].room)){
      strcpy(res.names[i], repo->clients[j].name);
      count++;
    }

  res.active_clients = count;

  repo_off();

  msgsnd(req.client_msgid, &res, sizeof(res), 0);
}

// MESSAGE

void message_request(){
  TEXT_MESSAGE msg;
  int result = msgrcv(my_client, &msg, sizeof(msg), PUBLIC, IPC_NOWAIT);
  if(result == -1) return;

  printf("request: PUBLIC MSG\n");
  msg.from_id = my_server;

  int servers[30], servers_count;
  repo_on();
    servers_count = repo->active_servers;
    REP(i, repo->active_servers)
      servers[i] = repo->servers[i].server_msgid;
  repo_off();

  REP(j, servers_count){
    if( fork() != 0){
      printf("Sending msg '%s' to server %d\n",msg.text, servers[j]);
      msgsnd(servers[j], &msg, sizeof(msg), 0);
    }
    else{
      STATUS_RESPONSE res;
      msgrcv(my_server, &res, sizeof(res), STATUS, 0);
      printf("STATUS RECEIVED\n");
      exit(1);
    }
  }
  return;
}

void broadcast_message(){
  TEXT_MESSAGE msg;
  int result = msgrcv(my_server, &msg, sizeof(msg), PUBLIC, IPC_NOWAIT);
  if(result == -1) return;

  printf("BROADCAST: \n");

  int server_to_respond = msg.from_id;

  msg.from_id = 0;
  msg.type = PUBLIC;

  repo_on();

  char room[2000];
  REP(i, repo->active_clients)
    if( !strcmp(repo->clients[i].name, msg.from_name))
      strcpy(room, repo->clients[i].room);

  printf("room: '%s'\n",room);
  REP(j, repo->active_clients){
    printf("client servered to: %s %d %s\n",repo->clients[j].name, repo->clients[j].server_id, repo->clients[j].room);
    if( repo->clients[j].server_id == my_client && !strcmp(repo-> clients[j].room, room)){
      printf("sending_to %d - %s\n",get_user_id(repo->clients[j].name), repo->clients[j].name );
      int id = get_user_id(repo->clients[j].name);
      printf("id: %d\n",id);
      msgsnd(id, &msg, sizeof(msg), 0);
    }
  }

  repo_off();

  STATUS_RESPONSE res;
  res.type = STATUS;
  res.status = my_server;

  msgsnd(server_to_respond, &res, sizeof(res), 0);
}

// WHISPER

void look_for_user(TEXT_MESSAGE msg){
  if( get_user_id(msg.to) != -1){
    msg.from_id = 0;
    msgsnd(get_user_id(msg.to), &msg, sizeof(msg), 0);
  }
  else{
    msg.from_id = my_server;
    int rec_server;
    repo_on();
      REP(i, repo->active_clients)
        if(!strcmp(repo->clients[i].name, msg.to))
          rec_server = repo->clients[i].server_id;
    repo_off();

    if( fork() != 0){
      msgsnd(rec_server, &msg, sizeof(msg), 0);
    }
    else{
      STATUS_RESPONSE res;
      msgrcv(my_server, &res, sizeof(res), STATUS, 0);
      exit(1);
    }
  }
}

void whisper_request(){
  TEXT_MESSAGE msg;
  int result = msgrcv(my_client, &msg, sizeof(msg), PRIVATE, IPC_NOWAIT);
  if(result == -1) return;

  printf("request: PRIVATE MSG\n");

  look_for_user(msg);
  return;
}

void broadcast_whisper(){
  TEXT_MESSAGE msg;
  int result = msgrcv(my_server, &msg, sizeof(msg), PUBLIC, IPC_NOWAIT);
  if(result == -1) return;

  int server_to_respond = msg.from_id;
  look_for_user(msg);

  STATUS_RESPONSE res;
  res.type = STATUS;
  res.status = my_server;

  msgsnd(server_to_respond, &res, sizeof(res), 0);
}

// MAIN

int main() {
  repo_init();

  logger("UP: %d, repo sem: %d log sem: %d repo_id: %d\n", getpid(), rsem, lsem, rid);

  repo_off();
  fetch_repo();
  register_server();

  signal(SIGINT, close_server);
  signal(SIGTERM, close_server);

  logger("TURNED ON: %d\n", getpid());

  while(1){
    server_list_request();
    login_request();
    logout_request();
    join_room_request();
    room_list_request();
    all_users_request();
    users_here_request();
    message_request();
    broadcast_message();
    whisper_request();
    broadcast_whisper();
  }

  close_repo();
  return 0;
}
