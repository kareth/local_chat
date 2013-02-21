#include "libraries.h"

int rid, rsem, lsem;
int mc, ms;
REPO *repo;

time_t start_time, server_start_time;

int my_server, my_client;

char user_name[SERVER_CAPACITY][MAX_NAME_SIZE];
int user_msg[SERVER_CAPACITY];
int user_timeout[SERVER_CAPACITY];
int user_count = 0;

void add_local_user(char * name, int id){
  strcpy(user_name[user_count], name);
  user_msg[user_count] = id;
  user_timeout[user_count] = 0;
  user_count++;
}

void remove_server_with_server_id(int id, int zomb);

void remove_local_user(char * name){
  REP(i, user_count)
    if( !strcmp(user_name[i], name)){
      if( i != user_count - 1){
        strcpy(user_name[i], user_name[user_count-1]);
        user_msg[i] = user_msg[user_count-1];
        user_timeout[i] = user_timeout[user_count-1];
      }

      user_count--;
      break;
    }
}

int get_user_id(char * name){
  REP(i, user_count)
    if( !strcmp(user_name[i], name)){
      return user_msg[i];
    }
  return -1;
}

void reset_timeout(char * name){
  REP(i, user_count)
    if( !strcmp(user_name[i], name))
      user_timeout[i] = 0;
}

// ----------
//
//

int server_count;
int server_waiting_count[1000];
int server_ids[1000];
int server_times[1000][1000];

void remove_row(int index){
  server_count--;
  if( index == server_count) return;
  server_ids[index] = server_ids[server_count];
  server_waiting_count[index] = server_waiting_count[server_count];
  REP(i, server_waiting_count[index])
    server_times[index][i] = server_times[server_count][i];
}

void server_responded(int id){
  printf("*** responded: %d\n", id);
  REP(i, server_count)
    if( server_ids[i] == id){
      server_waiting_count[i]--;
      if( server_waiting_count[i] == 0)
        remove_row(i);
      else{
        REP(j, server_waiting_count[i])
          server_times[i][j] = server_times[i][j+1];
      }
    }
}
void increase_server_times(){
  REP(i, server_count){
    REP(j, server_waiting_count[i])
      server_times[i][j]++;
  }
}

void kill_zombie(int id){
  printf("*** KILL DAT NIAB HAHAHAHAHA : %d\n", id);
  remove_server_with_server_id(id, 1);
}

void kill_dead_servers(){
  REP(i, server_count){
    if(server_times[i][0] > TIMEOUT){
      kill_zombie(server_ids[i]);
      remove_row(i);
    }
  }
}

void start_waiting_for_server(int id){
  printf("*** start waiting: %d\n",id);
  REP(i, server_count)
    if( server_ids[i] == id){
      server_times[i][server_waiting_count[i]] = 0;
      server_waiting_count[i]++;
      return;
    }
  server_waiting_count[server_count]=1;
  server_ids[server_count] = id;
  server_times[server_count][0] = 0;
  server_count++;
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

  FILE *log = fopen(LOG_FILE, "at");
  if (!log) log = fopen(LOG_FILE, "wt");

  va_start(args, format);
  vfprintf(log, format, args);
  va_end(args);

  fclose(log);
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

  my_client = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
  my_server = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
  printf("MY INFO: C%d S:%d\n",my_client, my_server);
  msgget(SERVER_LIST_MSG_KEY, 0666 | IPC_CREAT);

  repo->servers[repo->active_servers].client_msgid = my_client;
  repo->servers[repo->active_servers].server_msgid = my_server;
  repo->servers[repo->active_servers].clients = 0;

  repo->active_servers++;

  qsort(repo->servers, MAX_SERVER_NUM, sizeof(SERVER), comp_s);
  repo_off();
  logger("ALIVE: %d\n", my_client);
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
    int list_id = msgget(SERVER_LIST_MSG_KEY, 0666);
    int repo_id = shmget(ID_REPO, sizeof(REPO), 0666);

    msgctl(my_client, IPC_RMID, 0);
    msgctl(my_server, IPC_RMID, 0);

    msgctl(list_id, IPC_RMID, 0);
    semctl(rsem, IPC_RMID, 0);
    semctl(lsem, IPC_RMID, 0);
    shmctl(repo_id, IPC_RMID, 0);
    logger("DOWN: %d\n", my_client);
  } else {
    repo_off();

    remove_server_with_server_id(my_server, 0);

    repo_on();
      shmdt(repo);
    repo_off();
  }
}

void close_server(){
  close_repo();
  exit(1);
}




// SERVER_LIST
void server_list_request() {
  SERVER_LIST_REQUEST req;
  int list_id = msgget(SERVER_LIST_MSG_KEY, 0666);
  int result = msgrcv(list_id, &req, sizeof(SERVER_LIST_REQUEST)-sizeof(long), SERVER_LIST, IPC_NOWAIT);
  if(result == -1) return;
  printf("request: SERVER_LIST\n");

  SERVER_LIST_RESPONSE res;
  res.type = SERVER_LIST;
  res.active_servers = repo->active_servers;

  REP(i, repo->active_servers) {
    res.servers[i] = repo->servers[i].client_msgid;
    res.clients_on_servers[i] = repo->servers[i].clients;
  }
  msgsnd(req.client_msgid, &res, sizeof(SERVER_LIST_RESPONSE)-sizeof(long), 0);
}

// LOGIN
int validate_user(char * login){
  if(strlen(login) >= MAX_NAME_SIZE) return 400;

  REP(k, strlen(login))
    if(! isprint(login[k])) return 400;

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
  int result = msgrcv(my_client, &req, sizeof(CLIENT_REQUEST)-sizeof(long), LOGIN, IPC_NOWAIT);
  if(result == -1) return;

  printf("request: LOGIN (%s)\n", req.client_name);

  STATUS_RESPONSE res;
  res.type = STATUS;

  repo_on();
  res.status = validate_user(req.client_name);

  if( res.status == 201){
    logger("LOGGED_IN@%d: %s\n", my_client, req.client_name);
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
  msgsnd(req.client_msgid, &res, sizeof(STATUS_RESPONSE)-sizeof(long), 0);
  return;
}



// LOGOUT

void logout_user(char * name){
  printf("dologout %s\n", name);
  logger("LOGGED_OUT@%d: %s\n", my_client, name);
  repo_on();
  REP(i, repo->active_clients)
    if( !strcmp( repo->clients[i].name, name))
      strcpy( repo->clients[i].name, NONAME);

  repo->active_clients--;
  qsort(repo->clients, MAX_CLIENTS, sizeof(CLIENT), comp_c);
  remove_local_user(name);
  repo_off();
  return;
}

void logout_request() {
  CLIENT_REQUEST req;
  int result = msgrcv(my_client, &req, sizeof(CLIENT_REQUEST)-sizeof(long), LOGOUT, IPC_NOWAIT);
  if(result == -1) return;
  if( get_user_id(req.client_name) == -1) return; // Uwierzytelnienie
  printf("request: LOGOUT (%s)\n", req.client_name);
  logout_user(req.client_name);
  return;
}

// JOIN ROOM
void join_room_request() {
  CHANGE_ROOM_REQUEST req;
  int result = msgrcv(my_client, &req, sizeof(CHANGE_ROOM_REQUEST)-sizeof(long), CHANGE_ROOM, IPC_NOWAIT);
  if(result == -1) return;
  if( get_user_id(req.client_name) == -1) return; // Uwierzytelnienie

  printf("request: CHANGE_ROOM (%s) -> (%s)\n", req.client_name, req.room_name);

  repo_on();


  int status = 202;
  if(strlen(req.client_name) >= MAX_NAME_SIZE) status = 400;
  else {
    REP(q, strlen(req.client_name))
      if(! isprint(req.client_name[q]))
        status = 400;
  }

  if( status == 202){
    char old_room[MAX_NAME_SIZE];
    REP(i, repo->active_clients)
      if( !strcmp( repo->clients[i].name, req.client_name)){
        strcpy( old_room, repo->clients[i].room);
        strcpy( repo->clients[i].room, req.room_name);
      }

    REP(j, repo->active_rooms)
      if( !strcmp(repo->rooms[j].name, old_room)){
        repo->rooms[j].clients--;
        if( repo->rooms[j].clients == 0){
          strcpy(repo->rooms[j].name, NONAME);
          repo->active_rooms--;
        }
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
  }

  repo_off();

  STATUS_RESPONSE res;
  res.type = STATUS;
  res.status = status;

  msgsnd(req.client_msgid, &res, sizeof(STATUS_RESPONSE)-sizeof(long), 0);

  return;
}


// ROOM LIST

void room_list_request() {
  CLIENT_REQUEST req;
  int result = msgrcv(my_client, &req, sizeof(CLIENT_REQUEST)-sizeof(long), ROOM_LIST, IPC_NOWAIT);
  if(result == -1) return;
  if( get_user_id(req.client_name) == -1) return; // Uwierzytelnienie

  printf("request: ROOM_LIST\n");

  repo_on();
  ROOM_LIST_RESPONSE res;
  res.type = ROOM_LIST;
  res.active_rooms = repo->active_rooms;

  REP(i, repo->active_rooms){
    strcpy(res.rooms[i].name, repo->rooms[i].name);
    res.rooms[i].clients = repo->rooms[i].clients;
  }

  repo_off();

  msgsnd(req.client_msgid, &res, sizeof(ROOM_LIST_RESPONSE)-sizeof(long), 0);
}

// GLOBAL USERS LIST
//
void all_users_request(){
  CLIENT_REQUEST req;
  int result = msgrcv(my_client, &req, sizeof(CLIENT_REQUEST)-sizeof(long), GLOBAL_CLIENT_LIST, IPC_NOWAIT);
  if(result == -1) return;
  if( get_user_id(req.client_name) == -1) return; // Uwierzytelnienie

  printf("request: GLOBAL_USERS_LIST\n");

  repo_on();

  GLOBAL_CLIENT_LIST_RESPONSE res;
  res.type = GLOBAL_CLIENT_LIST;
  res.active_clients = repo->active_clients;

  REP(i, repo->active_clients)
    strcpy(res.names[i], repo->clients[i].name);

  repo_off();

  msgsnd(req.client_msgid, &res, sizeof(GLOBAL_CLIENT_LIST_RESPONSE)-sizeof(long), 0);
}

// ROOM USERS LIST

void users_here_request(){
  CLIENT_REQUEST req;
  int result = msgrcv(my_client, &req, sizeof(CLIENT_REQUEST)-sizeof(long), ROOM_CLIENT_LIST, IPC_NOWAIT);
  if(result == -1) return;
  if( get_user_id(req.client_name) == -1) return; // Uwierzytelnienie

  printf("request: ROOM_USERS_LIST\n");

  repo_on();

  ROOM_CLIENT_LIST_RESPONSE res;
  res.type = ROOM_CLIENT_LIST;

  char user_room[MAX_NAME_SIZE];
  REP(i, repo->active_clients)
    if(!strcmp(repo->clients[i].name, req.client_name)){
      strcpy(user_room, repo->clients[i].room);
      break;
    }

  printf("----user_room: '%s'\n",user_room);

  int count = 0;

  REP(j, repo->active_clients)
    if( !strcmp(user_room, repo->clients[j].room)){
      strcpy(res.names[i], repo->clients[j].name);
      printf("----user: '%s'\n",res.names[i]);
      count++;
    }

  res.active_clients = count;

  repo_off();

  msgsnd(req.client_msgid, &res, sizeof(ROOM_CLIENT_LIST_RESPONSE)-sizeof(long), 0);
}

// MESSAGE
void message_request(){
  TEXT_MESSAGE msg;
  int result = msgrcv(my_client, &msg, sizeof(TEXT_MESSAGE)-sizeof(long), PUBLIC, IPC_NOWAIT);
  if(result == -1) return;

  printf("request: PUBLIC MSG\n");
  msg.from_id = my_server;

  int server_msgids[MAX_SERVER_NUM], client_msgids[MAX_SERVER_NUM], servers_count;
  repo_on();
    servers_count = repo->active_servers;
    REP(i, repo->active_servers){
      server_msgids[i] = repo->servers[i].server_msgid;
      client_msgids[i] = repo->servers[i].client_msgid;
    }
  repo_off();

  REP(j, servers_count){
    start_waiting_for_server(client_msgids[j]);
    msgsnd(server_msgids[j], &msg, sizeof(TEXT_MESSAGE)-sizeof(long), 0);
  }
  return;
}

// BROADCAST MESSAGE
void broadcast_message(){
  TEXT_MESSAGE msg;
  int result = msgrcv(my_server, &msg, sizeof(TEXT_MESSAGE)-sizeof(long), PUBLIC, IPC_NOWAIT);
  if(result == -1) return;

  int server_to_respond = msg.from_id;

  msg.from_id = 0;
  msg.type = PUBLIC;

  repo_on();

  char room[MAX_NAME_SIZE];
  REP(i, repo->active_clients)
    if( !strcmp(repo->clients[i].name, msg.from_name))
      strcpy(room, repo->clients[i].room);

  REP(j, repo->active_clients)
    if( repo->clients[j].server_id == my_client && !strcmp(repo-> clients[j].room, room))
      msgsnd(get_user_id(repo->clients[j].name), &msg, sizeof(TEXT_MESSAGE)-sizeof(long), 0);

  repo_off();

  STATUS_RESPONSE res;
  res.type = STATUS;
  res.status = my_client;

  msgsnd(server_to_respond, &res, sizeof(STATUS_RESPONSE)-sizeof(long), 0);
}

// WHISPER
void look_for_user(TEXT_MESSAGE *msg){
  if( get_user_id(msg->to) != -1){
    msg->from_id = 0;
    msgsnd(get_user_id(msg->to), msg, sizeof(TEXT_MESSAGE)-sizeof(long), 0);
  }
  else{
    msg->from_id = my_server;
    int rec_server, rec_client;
    repo_on();
      REP(i, repo->active_clients)
        if(!strcmp(repo->clients[i].name, msg->to))
          rec_client = repo->clients[i].server_id;

      REP(j, repo->active_servers)
        if( repo->servers[j].client_msgid == rec_client)
          rec_server = repo->servers[j].server_msgid;

    repo_off();

    start_waiting_for_server(rec_client);
    msgsnd(rec_server, msg, sizeof(TEXT_MESSAGE)-sizeof(long), 0);
  }
}

void whisper_request(){
  TEXT_MESSAGE msg;
  int result = msgrcv(my_client, &msg, sizeof(TEXT_MESSAGE)-sizeof(long), PRIVATE, IPC_NOWAIT);
  if(result == -1) return;

  printf("request: PRIVATE MSG\n");

  look_for_user(&msg);
  return;
}

// BROADCAST WHISPER
void broadcast_whisper(){
  TEXT_MESSAGE msg;
  int result = msgrcv(my_server, &msg, sizeof(TEXT_MESSAGE)-sizeof(long), PRIVATE, IPC_NOWAIT);
  if(result == -1) return;

  int server_to_respond = msg.from_id;
  look_for_user(&msg);

  STATUS_RESPONSE res;
  res.type = STATUS;
  res.status = my_client;

  printf("I SENT :( %d -> %d\n", server_to_respond, res.status);

  msgsnd(server_to_respond, &res, sizeof(STATUS_RESPONSE)-sizeof(long), 0);
}

void increase_timeouts(){
  REP(i, user_count) user_timeout[i]++;
}

void kill_dead(){
  REP(i, user_count)
    if( user_timeout[i] >= TIMEOUT){
      logger("LOGGED_OUT@%d: %s\n", my_client, user_name[i]);
      logout_user(user_name[i]);
    }
}

void heartbeat(){
  if( time(0) - start_time > HEARTBEAT_TIME){

    increase_timeouts();
    kill_dead();// BWAHHAHAHAHAHAHAHAHHAHAHA noobs.

    start_time = time(0);
    STATUS_RESPONSE req;
    req.status = my_client;
    req.type = HEARTBEAT;

    REP(i, user_count)
      msgsnd(user_msg[i], &req, sizeof(STATUS_RESPONSE)-sizeof(long), 0);
  }
}

void heartbeat_response(){
  CLIENT_REQUEST hb;
  int result = msgrcv(my_client, &hb, sizeof(CLIENT_REQUEST)-sizeof(long), HEARTBEAT, IPC_NOWAIT);
  if(result == -1) return;
  reset_timeout(hb.client_name);
}

void server_responses(){
  STATUS_RESPONSE res;
  int result = msgrcv(my_server, &res, sizeof(STATUS_RESPONSE)-sizeof(long), STATUS, IPC_NOWAIT);
  if(result == -1) return;
  server_responded(res.status);
}

void server_timer(){
  if( time(0) - server_start_time > HEARTBEAT_TIME){
    server_start_time = time(0);
    increase_server_times();
    kill_dead_servers();// BWAHHAHAHAHAHAHAHAHHAHAHA noobs.
  }
}

// MAIN
int main() {
  repo_init();
  server_start_time = start_time = time(0);

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
    heartbeat();
    heartbeat_response();
    server_responses();
    server_timer();
  }

  close_repo();
  return 0;
}




void remove_server_with_server_id(int id, int zomb){
    repo_on();
    repo->active_servers--;

    REP(i, MAX_SERVER_NUM){
      if(repo->servers[i].server_msgid == id)  {
        msgctl(repo->servers[i].client_msgid, IPC_RMID, 0);
        msgctl(id, IPC_RMID, 0);

        if(zomb == 1)
          logger("KILLED_ZOMBIE: %d\n", repo->servers[i].client_msgid);
        logger("DOWN: %d\n", repo->servers[i].client_msgid);


        REP(j, repo->active_clients)
          if( repo->clients[i].server_id == repo->servers[i].client_msgid){
            logout_user(repo->clients[i].name);
            j = -1;
          }

        repo->servers[i].client_msgid = INF;
        break;
      }
    }

    qsort(repo->servers, MAX_SERVER_NUM, sizeof(SERVER), comp_s);
    repo_off();
}
