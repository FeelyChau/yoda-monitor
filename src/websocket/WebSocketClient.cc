//
// Created by 周飞宇 on 2018/11/26.
//

#include "WebSocketClient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include "../common.h"

#define RX_BUFFER_BYTES (50000)


void WebSocketClient::sendMsg(shared_ptr<Caps> &msg, SendCallback cb, void *cbData) {
  int32_t len = msg->serialize(nullptr, 0, 0x80);
  if (len > 0) {
    if (msgList.size() > maxBufferSize) {
      if (msgList.front().cb != nullptr)
        msgList.front().cb(SendResult::Removed,
                           msgList.front().userdata);
      msgList.pop_front();
      YODA_SIXSIX_SERROR("websocket buffer is full\n");
    }

    msgList.push_back({vector<unsigned char>(len + LWS_SEND_BUFFER_PRE_PADDING), cbData, cb});
    auto rst = msg->serialize(msgList.back().data.data() + LWS_SEND_BUFFER_PRE_PADDING,
                              (uint32_t) msgList.back().data.size(), 0x80);
    if (rst != len)
      YODA_SIXSIX_SERROR("send msg error\n");
  } else
    YODA_SIXSIX_SERROR("send msg error\n");
  if (web_socket)
    lws_callback_on_writable(web_socket);
}

void WebSocketClient::sendMsg(vector<shared_ptr<Caps>> &msgs, SendCallback cb, void *cbData) {
  for (auto &m : msgs) {
    int32_t len = m->serialize(nullptr, 0, 0x80);
    if (len > 0) {
      if (msgList.size() > maxBufferSize) {
        if (msgList.front().cb != nullptr)
          msgList.front().cb(SendResult::Removed,
                             msgList.front().userdata);
        msgList.pop_front();
        YODA_SIXSIX_SERROR("websocket buffer is full\n");
      }
      msgList.push_back({vector<unsigned char>(len + LWS_SEND_BUFFER_PRE_PADDING), cbData, cb});
      auto rst = m->serialize(msgList.back().data.data() + LWS_SEND_BUFFER_PRE_PADDING,
                              (uint32_t) msgList.back().data.size(), 0x80);
      if (rst != len)
        YODA_SIXSIX_SERROR("send msg error\n");
    } else
      YODA_SIXSIX_SERROR("send msg error\n");
  }
  if (web_socket)
    lws_callback_on_writable(web_socket);
}

WebSocketClient::WebSocketClient(uv_loop_t *uv, uint32_t maxBufferSize) : uv(uv), maxBufferSize(maxBufferSize) {
  if (foreign_loops == nullptr)
    foreign_loops = new void *[1];
  foreign_loops[0] = uv;


//  uv_timer_init(uv, &timerHandle);
  timerHandle.data = this;
}

bool WebSocketClient::start(const char *address, int port, const char *path) {

  static const struct lws_protocols protocols[] =
          {
                  {"performance", callback_ws, 0, RX_BUFFER_BYTES},
                  {nullptr,       nullptr,     0, 0} /* terminator */
          };


  this->address = address;
  this->path = path;
  this->port = port;
  memset(&info, 0, sizeof(info));
  info.port = CONTEXT_PORT_NO_LISTEN;
  info.protocols = protocols;
  info.gid = -1;
  info.uid = -1;
  info.options |= LWS_SERVER_OPTION_LIBUV;
  info.foreign_loops = foreign_loops;

  context = lws_create_context(&info);


  cinfo.context = context;
  cinfo.address = this->address.c_str();
  cinfo.port = port;
  cinfo.path = this->path.c_str();
  cinfo.host = lws_canonical_hostname(context);
  cinfo.origin = this->address.c_str();
  cinfo.protocol = protocols[0].name;
  cinfo.userdata = this;
  cinfo.ssl_connection = LCCSCF_PIPELINE;


  connect();

  return true;
}

int WebSocketClient::callback_ws(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
  auto wsc = reinterpret_cast<WebSocketClient *>(user);
  shared_ptr<Caps> caps;
  int32_t parseResult;
  int write;
  switch (reason) {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
      if (wsc->funcEventCb)
        wsc->funcEventCb(EventCode::Connected);
      lws_callback_on_writable(wsi);
      break;
    case LWS_CALLBACK_CLIENT_RECEIVE:
      /* Handle incomming messages here. */
      parseResult = Caps::parse(in, len, caps);
      if (parseResult == CAPS_SUCCESS) {
        if (wsc->funcRecvCb)
          wsc->funcRecvCb(caps);
      } else
        YODA_SISIX_FERROR("recv msg, caps parse error:%d\n", parseResult);
      break;
    case LWS_CALLBACK_CLIENT_WRITEABLE: {
      if (wsc->msgList.size() > 0) {
        auto d = wsc->msgList.front();
        write = lws_write(wsi, d.data.data() + LWS_SEND_BUFFER_PRE_PADDING, d.data.size() - LWS_SEND_BUFFER_PRE_PADDING,
                          LWS_WRITE_BINARY);
        lws_callback_on_writable(wsi);
        wsc->msgList.pop_front();
        if (d.cb != nullptr)
          d.cb(write == d.data.size() - LWS_SEND_BUFFER_PRE_PADDING ?
               SendResult::SendOut : SendResult::SendFailed,
               d.userdata
          );
        if (write != d.data.size() - LWS_SEND_BUFFER_PRE_PADDING)
          YODA_SISIX_FERROR("write ws error:%d/%ld\n", write, d.data.size() - LWS_SEND_BUFFER_PRE_PADDING);
        else
          YODA_SIXSIX_FLOG("write ws success:%d/%ld\n", write, d.data.size() - LWS_SEND_BUFFER_PRE_PADDING);
      }
      break;
    }
    case LWS_CALLBACK_WSI_CREATE:
      wsc->web_socket = wsi;
      break;
    case LWS_CALLBACK_WSI_DESTROY:
      wsc->web_socket = nullptr;
      if (wsc && wsc->funcEventCb)
        wsc->funcEventCb(EventCode::DisConnected);
      break;
    case LWS_CALLBACK_CLOSED:
      break;
    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
      wsc->reconnect();
      break;
    case LWS_CALLBACK_CLIENT_CLOSED:
      wsc->reconnect();
      break;
    case LWS_CALLBACK_EVENT_WAIT_CANCELLED:
      break;
    default:
      break;
  }

  return 0;
}


void WebSocketClient::stop() {
  lws_context_destroy(context);
}

WebSocketClient::~WebSocketClient() {
  if (foreign_loops)
    delete[] foreign_loops;
}

bool WebSocketClient::connect() {
  lws_client_connect_via_info(&cinfo);
  return true;
}

void WebSocketClient::reconnectTimerCb(uv_timer_t *handle) {
  printf("reconnect timer in\n");
  auto wsc = reinterpret_cast<WebSocketClient *>(handle->data);
  wsc->connect();
  uv_timer_stop(handle);
  printf("reconnect timer out\n");
}

void WebSocketClient::reconnect() {
  printf("reconnect timer start\n");
  uv_timer_init(uv, &timerHandle);
  uv_timer_start(&timerHandle, WebSocketClient::reconnectTimerCb, 5000, 5000);
}

void WebSocketClient::setRecvCallback(const std::function<void(shared_ptr<Caps> &)> &cb) {
  funcRecvCb = cb;
}

void WebSocketClient::setEventCallback(const std::function<void(enum EventCode)> &cb) {
  funcEventCb = cb;
}
