/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include "mongoose.h"

static sig_atomic_t s_signal_received = 0;
static struct mg_serve_http_opts s_http_server_opts;
static char *s_http_port, *server_pwd;
static struct mg_connection *node = NULL;

static void signal_handler(int sig_num) {
  signal(sig_num, signal_handler);  // Reinstantiate signal handler
  s_signal_received = sig_num;
}

//Display Message in console/terminal
static void display(struct mg_connection *nc, char type[25]){
  char addr[32];
  mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr), MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
  printf("%s\t%s\n", addr, type);
}

//Broadcast incoming message (from nc to all)
static void broadcast(struct mg_connection *nc, const struct mg_str msg) {
  char addr[32];
  mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr), MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
  printf("%s\tBroadcast\t[%d]\n", addr, (int)msg.len);
  struct mg_connection *c;
  for (c = mg_next(nc->mgr, NULL); c != NULL; c = mg_next(nc->mgr, c)) {
    if (c == nc) continue; /* Don't send to the sender. */
    mg_send_websocket_frame(c, WEBSOCKET_OP_TEXT,  msg.p, msg.len);
  }
}

//Unicast message (to nc)
static void unicast(struct mg_connection *nc, const struct mg_str msg) {
  char addr[32];
  mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr), MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
  printf("%s\tUnicast\t[%d]\n", addr, (int)msg.len);
  if(nc != NULL)
    mg_send_websocket_frame(nc, WEBSOCKET_OP_TEXT, msg.p, msg.len);
}

//Forward message (from nc to supernode)
static void unicast_forward(struct mg_connection *nc, const struct mg_str msg) {
  char addr[32];
  mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr), MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
  printf("%s\tForward\t[%d]\n", addr, (int)msg.len);
  if(node != NULL)
    mg_send_websocket_frame(node, WEBSOCKET_OP_TEXT, msg.p, msg.len);
  else
    printf("webApp_node is offline!\n");
}

//Request message (from nc to supernode)
static void unicast_request(struct mg_connection *nc, const struct mg_str msg) {
  if(node == NULL){
    printf("webApp_node is offline!\n");
    return;
  }
  char addr[32], buf[500];
  mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr), MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
  printf("%s\tRequest\t[%d]\n", addr, (int)msg.len-1);
  snprintf(buf, sizeof(buf), "?%s %.*s", addr, (int) msg.len-1, msg.p+1);
  mg_send_websocket_frame(node, WEBSOCKET_OP_TEXT, buf, strlen(buf));
}

//Reply message (from node)
static void unicast_reply(const struct mg_str msg) {
  if(node == NULL){
    printf("webApp_node is offline!\n");
    return;
  }
  //Get receiver address from msg
  char receiverAddr[32];
  int index = (int)(strchr(msg.p, ' ') - msg.p) + 1;
  snprintf(receiverAddr, sizeof(receiverAddr), "%.*s", index - 1, msg.p);
  printf("%s\tReply\t[%d]\n", receiverAddr, (int)msg.len - index);
  //send msg to receiver
  struct mg_connection *c;
  for (c = mg_next(node->mgr, NULL); c != NULL; c = mg_next(node->mgr, c)) {
    char addr[32];
    mg_sock_addr_to_str(&c->sa, addr, sizeof(addr), MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
    if (!strcmp(receiverAddr,addr)) 
      mg_send_websocket_frame(c, WEBSOCKET_OP_TEXT,  msg.p + index, msg.len - index);
  }
}

static void ev_handler(struct mg_connection *nc, int ev, void *ev_data) {
  switch (ev) {
    case MG_EV_WEBSOCKET_HANDSHAKE_DONE: {
      /*New websocket connection*/
      display(nc, "+Connected+");
      if(node!=NULL)
        unicast(nc, mg_mk_str("$+"));
      else
        unicast(nc, mg_mk_str("$-"));
      break;
    }
    case MG_EV_WEBSOCKET_FRAME: {
      struct websocket_message *wm = (struct websocket_message *) ev_data;
      /* New websocket message*/
      struct mg_str d = {(char *) wm->data, wm->size};
      if (d.p[0] == '$'){
        char pass[100];
        snprintf(pass, sizeof(pass), "%.*s",(int)d.len-1, &d.p[1]);
        if(!strcmp(pass,server_pwd)){
          if(node!=NULL)
            unicast(node,mg_mk_str("$Another login is encountered! Please close/refresh this window"));
          else
            broadcast(nc, mg_mk_str("$+"));
          node = nc;
          unicast(node,mg_mk_str("$Access Granted!"));
          display(nc, "*Became webApp_node*");
        }else
          unicast(nc,mg_mk_str("$Access Denied!"));
      }
      else if (d.p[0] == '?')
        unicast_request(nc,d);
      else if (nc == node)
        unicast_reply(d);
      else
        unicast_forward(nc,d);
      break;
    }
    case MG_EV_HTTP_REQUEST: {
      mg_serve_http(nc, (struct http_message *) ev_data, s_http_server_opts);
      break;
    }
    case MG_EV_CLOSE: {
      /* Disconnect websocket*/
      if(nc == node){
        node = NULL;
        display(nc,"!webApp_node Disconnected!");
        broadcast(nc, mg_mk_str("$-"));
      }else
        display(nc, "-Disconnected-");
      break;
    }
  }
}

int main(int argc, char** argv) {

  s_http_port = argv[1];
  server_pwd = argv[2];

  struct mg_mgr mgr;
  struct mg_connection *nc;

  signal(SIGTERM, signal_handler);
  signal(SIGINT, signal_handler);
  setvbuf(stdout, NULL, _IOLBF, 0);
  setvbuf(stderr, NULL, _IOLBF, 0);

  mg_mgr_init(&mgr, NULL);

  nc = mg_bind(&mgr, s_http_port, ev_handler);
  mg_set_protocol_http_websocket(nc);
  s_http_server_opts.document_root = "app/";  // Serve current directory
  s_http_server_opts.enable_directory_listing = "no";

  printf("Started on port %s\n", s_http_port);
  while (s_signal_received == 0) {
    mg_mgr_poll(&mgr, 200);
  }
  mg_mgr_free(&mgr);

  return 0;
}
