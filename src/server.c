#include "libraries.h"

int rid, rsem, lsem;
int mc, ms;
REPO *repo;

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
  repo_on();
  msgget(getpid(), 0666 | IPC_CREAT);
  msgget(SERVER_LIST_MSG_KEY, 0666 | IPC_CREAT);

  SERVER clean;
  mc = getpid();
  ms = rand()%2000000009;
  clean.client_msgid = getpid();
  clean.server_msgid = ms;
  clean.clients = 0;

  repo->servers[repo->active_servers] = clean;
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
  clean.active_clients = clean.active_rooms = clean.active_servers = 0;
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
  int client_id = msgget(req.client_msgid, 0666);
  msgsnd(client_id, &res, sizeof(res), 0);
}



// LOGIN
int validate_user(char * login){
  REP(i, repo->active_servers)
    if( repo->servers[i].client_msgid == mc){
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
  int my_id = msgget(getpid(), 0666 | IPC_CREAT);
  int result = msgrcv(my_id, &req, sizeof(req), LOGIN, IPC_NOWAIT);
  if(result == -1) return;

  printf("request: LOGIN (%s)\n", req.client_name);

  STATUS_RESPONSE res;
  res.type = STATUS;

  repo_on();
  res.status = validate_user(req.client_name);

  if( res.status == 201){
    CLIENT client;
    strcpy(client.name, req.client_name);
    client.server_id = getpid();
    // TODO this should increase value in room
    strcpy(client.room, "");

    repo->clients[repo->active_clients] = client;
    repo->active_clients++;
    qsort(repo->clients, MAX_CLIENTS, sizeof(CLIENT), comp_c);
  }
  repo_off();

  int client_id = msgget(req.client_msgid, 0666);
  msgsnd(client_id, &res, sizeof(res), 0);
  return;
}



// LOGOUT
void logout_request() {
  CLIENT_REQUEST req;
  int my_id = msgget(getpid(), 0666 | IPC_CREAT);
  int result = msgrcv(my_id, &req, sizeof(req), LOGOUT, IPC_NOWAIT);
  if(result == -1) return;

  printf("request: LOGOUT (%s)\n", req.client_name);

  repo_on();

  REP(i, repo->active_clients)
    if( !strcmp( repo->clients[i].name, req.client_name))
      strcpy( repo->clients[i].name, NONAME);

  repo->active_clients--;

  qsort(repo->clients, MAX_CLIENTS, sizeof(CLIENT), comp_c);

  repo_off();
  return;
}

// JOIN ROOM
void join_room_request() {
  CHANGE_ROOM_REQUEST req;
  int my_id = msgget(getpid(), 0666 | IPC_CREAT);
  int result = msgrcv(my_id, &req, sizeof(req), CHANGE_ROOM, IPC_NOWAIT);
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
    if( !strcmp(repo->rooms[i].name, old_room)){
      repo->rooms[i].clients--;
      if( repo->rooms[i].clients == 0)
        strcpy(repo->rooms[i].name, NONAME);
    }

  int found = 0;
  REP(k, repo->active_rooms)
    if( !strcmp(repo->rooms[i].name, req.room_name)){
      found = 1;
      repo->rooms[i].clients++;
    }

  if( found == 0){
    ROOM room;
    room.clients = 1;
    strcpy(room.name, req.room_name);
    repo->rooms[repo->active_rooms] = room;
    repo->active_rooms++;
  }

  qsort(repo->rooms, MAX_CLIENTS, sizeof(ROOM), comp_r);

  repo_off();

  STATUS_RESPONSE res;
  res.type = STATUS;
  res.status = 202;

  int client_id = msgget(req.client_msgid, 0666);
  msgsnd(client_id, &res, sizeof(res), 0);

  return;
}


// ROOM LIST

void room_list_request() {
  CLIENT_REQUEST req;
  int my_id = msgget(getpid(), 0666 | IPC_CREAT);
  int result = msgrcv(my_id, &req, sizeof(req), ROOM_LIST, IPC_NOWAIT);
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
    printf("room: %s %d\n", room.name, room.clients);
  }
  printf("%d\n",repo->active_rooms);

  repo_off();

  int client_id = msgget(req.client_msgid, 0666);
  if( msgsnd(client_id, &res, sizeof(res), 0) == -1) perror("error");
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
  }

  close_repo();
  return 0;
}
