#include <pjmedia.h>
#include <pjlib-util.h>
#include <pjlib.h>
#include <stdio.h>
#include <stdlib.h>

#include "src/global.h"
#include "src/UserMedia.h"
#include "src/PeerConnection.h"
#include <WebSocket.h>
#include <json.hpp>
#include <random>

std::string generateRandomId(size_t length = 0)
{
  static const std::string::value_type allowed_chars[] {"123456789BCDFGHJKLMNPQRSTVWXZbcdfghjklmnpqrstvwxz"};

  static thread_local std::default_random_engine randomEngine(std::random_device{}());
  static thread_local std::uniform_int_distribution<int> randomDistribution(0, sizeof(allowed_chars) - 1);

  std::string id(length ? length : 32, '\0');

  for (std::string::value_type& c : id) {
    c = allowed_chars[randomDistribution(randomEngine)];
  }

  return id;
}

int main(int argc, const char** argv) {
  webrtc::init();

  bool offerer = std::string(argv[1]) == "call";

  webrtc::UserMediaConstraints constraints;
  std::shared_ptr<webrtc::UserMedia> userMedia = webrtc::UserMedia::getUserMedia(constraints);

  webrtc::PeerConnectionConfiguration pcConfig;
  nlohmann::json iceServers =  {
      {
          { "urls", "turn:turn.xaos.ninja:4433" },
          { "username", "test" },
          { "credential", "12345" }
      }
  };
  pcConfig.iceServers = iceServers;
  std::shared_ptr<webrtc::PeerConnection> peerConnection = std::make_shared<webrtc::PeerConnection>(pcConfig);

  std::string uuid = generateRandomId(10);

  pj_thread_t* wsThread = nullptr;
  pj_thread_desc wsThreadDesc;
  pj_thread_t* wsReaderThread = nullptr;
  pj_thread_desc wsReaderThreadDesc;
  bzero(wsThreadDesc, sizeof(pj_thread_desc));

  std::shared_ptr<wsxx::WebSocket> webSocket = std::make_shared<wsxx::WebSocket>(
      "ws://localhost:8338/",
      [&webSocket, peerConnection, userMedia, uuid, &wsThreadDesc, &wsThread, offerer]() { // on open
        if(!pj_thread_is_registered()) pj_thread_register("websocket", wsThreadDesc, &wsThread);
        peerConnection->addStream(userMedia);
        if(offerer) {
          peerConnection->createOffer()->onResolved([=](nlohmann::json offer) {
            peerConnection->setLocalDescription(offer);
            nlohmann::json msg = {{"sdp",  offer},
                                  {"uuid", uuid}};
            webSocket->send(msg.dump(2), wsxx::WebSocket::PacketType::Text);
            for (auto &candidate : peerConnection->localCandidates) {
              nlohmann::json msg = {{"ice",  candidate},
                                    {"uuid", uuid}};
              webSocket->send(msg.dump(2), wsxx::WebSocket::PacketType::Text);
            }
/*            {
              nlohmann::json msg = {{"ice",  nullptr},
                                    {"uuid", uuid}};
              webSocket->send(msg.dump(2), wsxx::WebSocket::PacketType::Text);
            }*/
          });
        }
      },
      [&webSocket, &uuid, &peerConnection, &wsReaderThreadDesc, &wsReaderThread, offerer]
          (std::string data, wsxx::WebSocket::PacketType type) {
        if(!pj_thread_is_registered()) pj_thread_register("websocket_reader", wsReaderThreadDesc, &wsReaderThread);
        auto msg = nlohmann::json::parse(data);
        if(msg["uuid"] == uuid) return;
        printf("WSMSG %s\n", data.c_str());
        auto sdp = msg.find("sdp");
        auto ice = msg.find("ice");
        if(sdp != msg.end()) { // Handle SDP message
          printf("SDP MSD\n");
          auto sdpString = (*sdp)["sdp"];
          peerConnection->setRemoteDescription(*sdp);
          if((*sdp)["type"] == "offer" && !offerer) {
            peerConnection->createAnswer()->onResolved([=](nlohmann::json answer) {
              peerConnection->setLocalDescription(answer);
              nlohmann::json msg = {{"sdp", answer},
                                    {"uuid", uuid}};
              webSocket->send(msg.dump(2), wsxx::WebSocket::PacketType::Text);
              for (auto &candidate : peerConnection->localCandidates) {
                nlohmann::json msg = {{"ice",  candidate},
                                      {"uuid", uuid}};
                webSocket->send(msg.dump(2), wsxx::WebSocket::PacketType::Text);
              }
           /*   {
                nlohmann::json msg = {{"ice",  nullptr},
                                      {"uuid", uuid}};
                webSocket->send(msg.dump(2), wsxx::WebSocket::PacketType::Text);
              }*/
            });
          }
        } else if(ice != msg.end()) {
          printf("ICE MSD\n");
          peerConnection->addIceCandidate(*ice);
        }
      },
      [&webSocket](int code, std::string reason, bool wasClean) {
      }
  );

  while (true) {
    const pj_time_val delay = {0, 10};
    pj_timer_heap_poll(peerConnection->timerHeap, nullptr);
    pj_ioqueue_poll(peerConnection->ioqueue, &delay);
  }

  webrtc::destroy();

  /* Done. */
  return 0;
}
