//
// Created by Michał Łaszczewski on 02/02/18.
//


#include "PeerConnection.h"

#include <algorithm>
#include <cmath>

namespace webrtc {

  void onIceComplete(pjmedia_transport *tp, pj_ice_strans_op op, pj_status_t status) {
    PeerConnection *pc = (PeerConnection *) tp->user_data;
    if (status != PJ_SUCCESS) {
      pc->handleIceTransportFailed(tp);
      return;
    }
    pc->handleIceTransportComplete(tp);
  }

  void onIceComplete2(pjmedia_transport *tp, pj_ice_strans_op op, pj_status_t status, void *user_data){
    assert(status == PJ_SUCCESS);
    PeerConnection* pc = (PeerConnection*)tp->user_data;
    pc->handleIceTransportComplete(tp);
  }

  pjmedia_ice_cb iceCallbacks = {
      .on_ice_complete = onIceComplete,
      .on_ice_complete2 = onIceComplete2
  };

  void onSrtpComplete(pjmedia_transport *tp, pj_status_t status) {
    assert(status == PJ_SUCCESS);
    PeerConnection* pc = (PeerConnection*)tp->user_data;
    pc->handleDtlsTransportComplete(tp);
  }

  pjmedia_srtp_cb srtpCallbacks = {
      .on_srtp_nego_complete = onSrtpComplete
  };

  void PeerConnection::addIceServer(std::string& url, std::string username, std::string password) {
    std::string protocol = url.substr(0, 4);
    std::string hostPart = url.substr(5, url.size()-5);
    std::string host;
    unsigned short port;
    auto portSep = hostPart.find(':');
    if(portSep == std::string::npos) {
      host = hostPart;
      port = 3478;
    } else {
      host = hostPart.substr(0, portSep);
      port = atoi(hostPart.substr(portSep+1, hostPart.size()-(portSep+1)).c_str());
    }
    auto & cfg = iceTransportConfiguration;

    auto& stun = cfg.stun_tp[cfg.stun_tp_cnt++];
    pj_ice_strans_stun_cfg_default(&stun);
    stun.server = pj_strdup3(pool, host.c_str());
    stun.port = port;
    stun.af = pj_AF_INET();
/*
    auto& stun2 = cfg.stun_tp[cfg.stun_tp_cnt++];
    stun2 = stun;
    stun2.af = pj_AF_INET6();*/

    printf("ICE PROTO = %s HOST = %s PORT = %d UNAME = %s PASS = %s\n",
           protocol.c_str(), host.c_str(), port, username.c_str(), password.c_str());

    if(protocol == "turn") {
      auto& turn = cfg.turn_tp[cfg.turn_tp_cnt++];
      pj_ice_strans_turn_cfg_default(&turn);
      turn.server =  pj_strdup3(pool, host.c_str());
      turn.port = port;
      turn.af = pj_AF_INET();
      turn.auth_cred.type = PJ_STUN_AUTH_CRED_STATIC;
      turn.auth_cred.data.static_cred.username = pj_strdup3(pool, username.c_str());
      turn.auth_cred.data.static_cred.data_type = PJ_STUN_PASSWD_PLAIN;
      turn.auth_cred.data.static_cred.data = pj_strdup3(pool, password.c_str());
/*
      auto& turn2 = cfg.turn_tp[cfg.turn_tp_cnt++];
      turn2 = turn;
      turn2.af = pj_AF_INET6();*/
    }

  }

  PeerConnection::PeerConnection() {
    iceGatheringState = "new";
    iceConnectionState = "new";
    connectionState = "new";
    signalingState = "stable";

    remoteCandidatesGathered = false;
    sdpGenerated = false;
    transportStarted = false;
    localDescription = nullptr;
    remoteDescription = nullptr;

    iceCompletePromise = nullptr;
    dtlsCompletePromise = nullptr;

    bundle = true;

    remoteIceCompletePromise = std::make_shared<promise::Promise<bool>>();
  }

  void PeerConnection::init(PeerConnectionConfiguration& configurationp) {
    configuration = configurationp;

    pj_status_t status;
    pool = pj_pool_create(&cachingPool.factory,"PeerConnection.pool", 4096, 4096, NULL);

    mediaTransportsIceInitializedCount = 0;
    mediaTransportsDtlsInitializedCount = 0;

    pj_ice_strans_cfg_default(&iceTransportConfiguration);
    auto & cfg = iceTransportConfiguration;
    pj_stun_config_init(&cfg.stun_cfg, &cachingPool.factory, 0, ioqueue, timerHeap);

    cfg.turn.conn_type = PJ_TURN_TP_UDP;

    for(auto& iceServer : configuration.iceServers) {
      std::string uname("");
      std::string cred("");
      auto unamei = iceServer.find("username");
      auto credi = iceServer.find("credential");
      if(unamei != iceServer.end()) uname = *unamei;
      if(credi != iceServer.end()) cred = *credi;
      auto urli = iceServer.find("url");
      auto urlsi = iceServer.find("urls");
      if(urlsi != iceServer.end()) {
        if (iceServer["urls"].is_array()) {
          for (std::string url : iceServer["urls"]) {
            addIceServer(url, uname, cred);
          }
        } else {
          std::string url = iceServer["urls"];
          addIceServer(url, uname, cred);
        }
      } else if(urli != iceServer.end()) {
        std::string url = iceServer["url"];
        addIceServer(url, uname, cred);
      }
    }

    pjmedia_srtp_setting_default(&srtpSetting);
    srtpSetting.use = PJMEDIA_SRTP_MANDATORY;
    srtpSetting.close_member_tp = PJ_TRUE;
    srtpSetting.keying_count = 1;
    srtpSetting.keying[0] = PJMEDIA_SRTP_KEYING_DTLS_SRTP;
    srtpSetting.keying[1] = PJMEDIA_SRTP_KEYING_SDES;
    srtpSetting.user_data = (void*)this;
    srtpSetting.cb = srtpCallbacks;
  }

  void PeerConnection::addStream(std::shared_ptr<UserMedia> userMedia) {
    for(int i = 0; i < userMedia->tracks.size(); i++) {
      TrackId track = { .userMedia = userMedia, .trackId = i};
      inputTracks.push_back(track);
    }
  }

  std::shared_ptr<promise::Promise<bool>> PeerConnection::gatherIceCandidates(int streamsCount) {
    /// TODO: Create promise here
    if(!iceCompletePromise || iceCompletePromise->state == promise::Promise<bool>::PromiseState::Resolved)
      iceCompletePromise = std::make_shared<promise::Promise<bool>>();

    iceGatheringState = "gathering";
    if(onIceGatheringStateChange) onIceGatheringStateChange(iceGatheringState);

    pj_status_t status;
    for (int i = 0; i < streamsCount; i++) {
      mediaTransport.push_back(MediaTransport{nullptr, nullptr}); // make place for new transport
      auto& transport = mediaTransport[mediaTransport.size()-1];
      status = pjmedia_ice_create3(mediaEndpoint, NULL, 1, &iceTransportConfiguration, &iceCallbacks,
                                   PJMEDIA_ICE_RTCP_MUX, (void*)this, &transport.ice);
      if(status != PJ_SUCCESS) {
        iceGatheringState = "complete";
        if(onIceGatheringStateChange) onIceGatheringStateChange(iceGatheringState);
        iceConnectionState = "failed";
        if(onIceConnectionStateChange) onIceConnectionStateChange(iceConnectionState);
        connectionState = "failed";
        if(onConnectionStateChange) onConnectionStateChange(connectionState);
        //iceCompletePromise->reject(std::make_exception_ptr(std::logic_error("ice failed")));
        iceCompletePromise->resolve(false); // rejection fails with sigabrt TODO: repair promises
        return iceCompletePromise;
      }

      /* status = pjmedia_transport_mux_create(mediaEndpoint, transport.ice, &transport.mux);
       assert(status == PJ_SUCCESS);*/

      status = pjmedia_transport_srtp_create(mediaEndpoint, transport.ice, &srtpSetting, &transport.srtp);

      assert(status == PJ_SUCCESS);

    }

    return iceCompletePromise;
  }

  void PeerConnection::handleIceTransportComplete(pjmedia_transport *pTransport) {
    printf("ICE COMPLETE?!\n");
    mediaTransportsIceInitializedCount++;
    if(mediaTransportsIceInitializedCount == mediaTransport.size()) {
      printf("ICE COMPLETE!!\n");
      iceGatheringState = "complete";
      if(onIceGatheringStateChange) onIceGatheringStateChange(iceGatheringState);
      iceCompletePromise->resolve(true);
    }
  }

  void PeerConnection::handleDtlsTransportComplete(pjmedia_transport *pTransport) {
    printf("DTLS COMPLETE?!\n");
    mediaTransportsDtlsInitializedCount++;
    if(mediaTransportsDtlsInitializedCount == mediaTransport.size()) {
      printf("DTLS COMPLETE!!\n");
      dtlsCompletePromise->resolve(true);
    }
  }

  std::shared_ptr<promise::Promise<nlohmann::json>> PeerConnection::createOffer() {
    printf("CREATE OFFER?!");
    if(!iceCompletePromise) {
      if(inputTracks.size() > 1) bundle = true;
      gatherIceCandidates(1);
    }
    if(mediaTransport.size() == 0) throw "error";
    return iceCompletePromise->then<nlohmann::json>([this](bool& v) {
      startTransportIfPossible();
      return promise::Promise<nlohmann::json>::resolved(doCreateOffer());
    });
  }

  std::shared_ptr<promise::Promise<nlohmann::json>> PeerConnection::createAnswer() {
    PeerConnection* pc = this;
    if(!iceCompletePromise) {
      if(inputTracks.size() > 1) bundle = true;
      gatherIceCandidates(1);
    }
    if(mediaTransport.size() == 0) throw "error";
    return pc->iceCompletePromise->then<nlohmann::json>([this, pc](bool& v) {
      return pc->remoteIceCompletePromise->then<nlohmann::json>([this, pc](bool& v) {
        pc->startTransportIfPossible();
        return promise::Promise<nlohmann::json>::resolved(pc->doCreateAnswer(this->remoteDescription));
      });
    });
  }

  std::string replace(const std::string& data, const std::string& substr, const std::string& replacement)
  {
    std::string res;
    std::string::const_iterator b = data.cbegin();
    std::string::const_iterator e = data.cend();

    std::string::const_iterator pos = search(b, e, substr.cbegin(), substr.cend());
    while (pos != e)
    {
      std::copy(b, pos, back_inserter(res));
      std::copy(begin(replacement), end(replacement), back_inserter(res));

      b = pos + substr.size();
      pos = search(b, e, substr.cbegin(), substr.cend());
    }
    std::copy(b, e, std::back_inserter(res));

    return res;
  }

  nlohmann::json PeerConnection::doCreateOffer() {
    printf("CREATE SDP!\n");

    pj_status_t status;
    pj_sockaddr origin;
    pj_str_t originString = pj_strdup3(pool, "localhost");
    pj_sockaddr_parse(pj_AF_INET(), 0, &originString, &origin);
    pjmedia_sdp_session *sdp;
    status = pjmedia_endpt_create_base_sdp(mediaEndpoint, pool, NULL, &origin, &sdp);
    assert(status == PJ_SUCCESS);

    for(int i = 0; i < mediaTransport.size(); i++) {
      auto& transport = mediaTransport[i];
      pjmedia_transport_media_create(transport.srtp, pool, 0, nullptr, i);
    }

    pjmedia_transport_info transportInfo;

    pjmedia_transport_info_init(&transportInfo);
    pjmedia_transport_get_info(mediaTransport[0].ice, &transportInfo);

    pjmedia_sdp_media* sdpMedia;

    pj_sockaddr zero;
    pj_str_t zeroString = pj_strdup3(pool, "0.0.0.0:9");
    pj_sockaddr_parse(pj_AF_INET(), 0, &zeroString, &zero);

    transportInfo.sock_info.rtp_addr_name = zero;
    transportInfo.sock_info.rtcp_addr_name = zero;

    status = pjmedia_endpt_create_audio_sdp(mediaEndpoint, pool, &transportInfo.sock_info, 0, &sdpMedia);
    assert(status == PJ_SUCCESS);
    sdp->media[sdp->media_count++] = sdpMedia;

    for(int i = 0; i < mediaTransport.size(); i++) {
      auto& transport = mediaTransport[i];
      status = pjmedia_transport_encode_sdp(transport.srtp, pool, sdp, nullptr, i);
      assert(status == PJ_SUCCESS);
    }

    char buf[10240];
    size_t offerSize = pjmedia_sdp_print(sdp, buf, 10240);
    std::string rawSdpString(buf, offerSize);
    //printf("\nRAW SDP:\n%s\n", rawSdpString.c_str());

    std::istringstream iss(rawSdpString);
    std::ostringstream oss;
    std::string line;
    std::string iceUfrag;
    while(std::getline(iss, line, '\n')) {
      printf("SDP LINE: %s\n", line.c_str());
      if(line.substr(0, 12) == "a=candidate:") {
        nlohmann::json candidate = {
            {"candidate", line.substr(2, line.size()-3)/* + " generation 0"
                          + " ufrag " + iceUfrag + " network-id 1"*/
            },
            {"sdpMLineIndex", 0},
            {"sdpMid", "audio"},
            {"usernameFragment", iceUfrag}
        };
        localCandidates.push_back(candidate);
      }/* else if(line.substr(0, 7) == "m=audio") {
        oss << replace(line, "SAVP", "SAVPF") << '\n';
      }*/ else {
        oss << line << '\n';
        if(line.substr(0, 12) == "a=ice-ufrag:") {
          iceUfrag = line.substr(12, line.size()-12-1);
        }
        if(line.substr(0, 10) == "a=ice-pwd:") {
          oss << "a=ice-options:trickle\r\n";
        }
        if(line.substr(0, 7) == "m=audio") {
          oss << "a=mid:audio\r\n";
        }
      }
    }

    nlohmann::json sdpJson = { {"type", "offer"}, {"sdp", oss.str() } };

    sdpGenerated = true;

    return sdpJson;
  }

  nlohmann::json PeerConnection::doCreateAnswer(nlohmann::json offer) {
    pj_status_t status;

    printf("CREATE ANSWER FOR OFFER %s\n", offer.dump(2).c_str());

    std::ostringstream oss;
    oss << offer["sdp"].get<std::string>();
    for(auto& candidate : remoteCandidates) {
      oss << "a=" << candidate["candidate"].get<std::string>() << "\r\n";
    }
    std::string sdpString = oss.str();

    printf("REMOTE SDP WITH ICE:\n%s\n", sdpString.c_str());

    pjmedia_sdp_session* offerSdp;
    status = pjmedia_sdp_parse(pool, (char*)sdpString.data(), sdpString.size(), &offerSdp);
    assert(status == PJ_SUCCESS);

    pj_sockaddr origin;
    pj_str_t originString = pj_strdup3(pool, "localhost");
    pj_sockaddr_parse(pj_AF_INET(), 0, &originString, &origin);
    pjmedia_sdp_session *sdp;
    status = pjmedia_endpt_create_base_sdp(mediaEndpoint, pool, nullptr, &origin, &sdp);
    assert(status == PJ_SUCCESS);

    for(int i = 0; i < mediaTransport.size(); i++) {
      auto& transport = mediaTransport[i];
      status = pjmedia_transport_media_create(transport.srtp, pool, 0, offerSdp, i);
      assert(status == PJ_SUCCESS);
    }

    pjmedia_transport_info transportInfo;

    pjmedia_transport_info_init(&transportInfo);
    pjmedia_transport_get_info(mediaTransport[0].srtp, &transportInfo);

    pjmedia_sdp_media* sdpMedia;

    pj_sockaddr zero;
    pj_str_t zeroString = pj_strdup3(pool, "0.0.0.0:9");
    pj_sockaddr_parse(pj_AF_INET(), 0, &zeroString, &zero);

    transportInfo.sock_info.rtp_addr_name = zero;
    transportInfo.sock_info.rtcp_addr_name = zero;

    status = pjmedia_endpt_create_audio_sdp(mediaEndpoint, pool, &transportInfo.sock_info, 0, &sdpMedia);
    assert(status == PJ_SUCCESS);
    sdp->media[sdp->media_count++] = sdpMedia;

    for(int i = 0; i < mediaTransport.size(); i++) {
      auto& transport = mediaTransport[i];
      status = pjmedia_transport_encode_sdp(transport.srtp, pool, sdp, offerSdp, i);
      assert(status == PJ_SUCCESS);
    }

    char buf[10240];
    size_t offerSize = pjmedia_sdp_print(sdp, buf, 10240);
    std::string rawSdpString(buf, offerSize);
    //printf("\nRAW SDP:\n%s\n", rawSdpString.c_str());

    std::istringstream iss(rawSdpString);
    std::string line;
    bool foundAudio = false;
    bool foundVideo = false;
    while(std::getline(iss, line, '\n')) {
      if(line.substr(0, 7) == "m=audio") {
        foundAudio = true;
      }
      if(line.substr(0, 7) == "m=video") {
        foundVideo = true;
      }
    }


    iss.str(rawSdpString);
    iss.clear();
    oss.str("");
    std::string iceUfrag;
    bool mediaFound = false;
    while(std::getline(iss, line, '\n')) {
      printf("SDP LINE: %s\n", line.c_str());
      if(line.substr(0, 12) == "a=candidate:") {
        nlohmann::json candidate = {
            {"candidate", line.substr(2, line.size()-3)/* + " generation 0"
                          + " ufrag " + iceUfrag + " network-id 1"*/
            },
            {"sdpMLineIndex", 0},
            {"sdpMid", "audio"},
            {"usernameFragment", iceUfrag}
        };
        localCandidates.push_back(candidate);
      } else {
        oss << line << '\n';
        if(line.substr(0, 12) == "a=ice-ufrag:") {
          iceUfrag = line.substr(12, line.size()-12-1);
        }
        if(line.substr(0, 10) == "a=ice-pwd:") {
          oss << "a=ice-options:trickle\r\n";
        }
        if(line.substr(0, 7) == "m=audio") {
          if(!mediaFound) {
            mediaFound = true;
            oss << "a=group:BUNDLE";
            if(foundAudio) oss << " audio";
            if(foundVideo) oss << " video";
          }
          oss << "a=mid:audio\r\n";
        }
        if(line.substr(0, 7) == "m=video") {
          if(!mediaFound) {
            mediaFound = true;
            oss << "a=group:BUNDLE";
            if(foundAudio) oss << " audio";
            if(foundVideo) oss << " video";
          }
          oss << "a=mid:video\r\n";
        }
      }
    }

    nlohmann::json sdpJson = { { "type", "answer" }, { "sdp", oss.str() } };

    sdpGenerated = true;

    return sdpJson;
  }

  void PeerConnection::setLocalDescription(nlohmann::json sdp) {
    localDescription = sdp;
    startTransportIfPossible();
  }
  void PeerConnection::setRemoteDescription(nlohmann::json sdp) {
    remoteDescription = sdp;
    std::istringstream iss(remoteDescription["sdp"].get<std::string>());
    std::string line;
    std::string iceUfrag;
    while(std::getline(iss, line, '\n')) {
      printf("SDP LINE: %s\n", line.c_str());
      if(line.substr(0, 14) == "a=group:BUNDLE") {
        bundle = true;
      }
    }
    startTransportIfPossible();
  }

  void checkIceTimerCb(pj_timer_heap_t *ht, pj_timer_entry *e) {
    PeerConnection* pc = (PeerConnection*)e->user_data;
    pc->checkIce();
  }
  void PeerConnection::checkIce() {
    bool foundRelay = false;
    for(auto& candidate : remoteCandidates) {
      std::string text = candidate["candidate"].get<std::string>();
      int pos = text.find("relay");
      if(pos != std::string::npos) {
        foundRelay = true;
        break;
      }
    }
    if(foundRelay) {
      remoteCandidatesGathered = true;
      remoteIceCompletePromise->resolve(true);
      startTransportIfPossible();
    }
    pj_time_val delay;
    pj_status_t status;
    delay.sec = 1;
    delay.msec = 0;
    pj_timer_entry_init(&statTimerEntry, 0, (void*)this, checkIceTimerCb);
    status = pj_timer_heap_schedule(timerHeap, &statTimerEntry, &delay);
    assert(status == PJ_SUCCESS);
  }
  void PeerConnection::addIceCandidate(nlohmann::json candidate) {
    if(remoteCandidatesGathered) return;
    if(remoteCandidates.size() == 0) {
      checkIce();
    }
    if(candidate == nullptr) {
      remoteCandidatesGathered = true;
      remoteIceCompletePromise->resolve(true);
    } else {
      remoteCandidates.push_back(candidate);
    }
    startTransportIfPossible();
  }

  void PeerConnection::startTransportIfPossible() {
    if(!dtlsCompletePromise || dtlsCompletePromise->state == promise::Promise<bool>::PromiseState::Resolved)
      dtlsCompletePromise = std::make_shared<promise::Promise<bool>>();

    pj_status_t status;
    printf("START TRANSPORT? %d %d %d\n", remoteCandidatesGathered, localDescription != nullptr, remoteDescription != nullptr);
    if(!(remoteCandidatesGathered && localDescription != nullptr && remoteDescription != nullptr
         && sdpGenerated && !transportStarted)) return;

    transportStarted = true;

    printf("START TRANSPORT!\n");

    iceConnectionState = "checking";
    if(onIceConnectionStateChange) onIceConnectionStateChange(iceConnectionState);

    connectionState = "connecting";
    if(onConnectionStateChange) onConnectionStateChange(connectionState);

    std::ostringstream oss;
    oss << localDescription["sdp"].get<std::string>();
    for(auto& candidate : localCandidates) {
      oss << "a=" << candidate["candidate"].get<std::string>() << "\r\n";
    }
    localWithIce = oss.str();
    oss.str("");
    oss << remoteDescription["sdp"].get<std::string>();
    for(auto& candidate : remoteCandidates) {
      oss << "a=" << candidate["candidate"].get<std::string>() << "\r\n";
    }
    remoteWithIce = oss.str();

    printf("PREPARED LOCAL SDP: \n%s\n", localWithIce.c_str());
    printf("PREPARED REMOTE SDP: \n%s\n", remoteWithIce.c_str());

    status = pjmedia_sdp_parse(pool, (char*)localWithIce.data(), localWithIce.size(), &localSdp);
    assert(status == PJ_SUCCESS);
    status = pjmedia_sdp_parse(pool, (char*)remoteWithIce.data(), remoteWithIce.size(), &remoteSdp);
    assert(status == PJ_SUCCESS);

    printf("LOCAL AND REMOTE SDP PARSED!\n");

    for(int i = 0; i < mediaTransport.size(); i++) {
      auto transport = mediaTransport[i].srtp;
      status = pjmedia_transport_media_start(transport, pool, localSdp, remoteSdp, i);
      assert(status == PJ_SUCCESS);
    }
    printf("MEDIA TRANSPORTS STARTED\n");

    dtlsCompletePromise->onResolved([this](bool ok){
      iceConnectionState = "completed";
      if(onIceConnectionStateChange) onIceConnectionStateChange(iceConnectionState);
      startMedia();
    });
  }

  void PeerConnection::startMedia() {
    printf("START MEDIA!!!\n");

    int streamsCount = remoteSdp->media_count;
    if(streamsCount > inputTracks.size()) streamsCount = inputTracks.size();
    if(!bundle && streamsCount > mediaTransport.size()) streamsCount = mediaTransport.size();

    mediaStreams.resize(streamsCount);

    if(bundle) {
      pjmedia_transport_bundle_create(mediaEndpoint, mediaTransport[0].srtp, &mediaTransport[0].bundle);
    }

    for(int i = 0; i < mediaStreams.size(); i++) {
      pj_status_t status;

      pjmedia_stream_info stream_info;
      auto& stream = mediaStreams[i];

      pjmedia_transport_info transportInfo;

      pjmedia_transport_get_info(mediaTransport[i].ice, &transportInfo);
      /*remoteSdp->media[i]->conn->addr = transportInfo.sock_info.*/

      pjmedia_sdp_media* sdpMedia;

      status = pjmedia_endpt_create_audio_sdp(mediaEndpoint, pool, &transportInfo.sock_info, 0, &sdpMedia);
      assert(status == PJ_SUCCESS);
      localSdp->media[i]->conn = sdpMedia->conn;
      localSdp->media[i]->desc.media = sdpMedia->desc.media;

      remoteSdp->media[i]->conn->addr = pj_strdup3(pool, "1.2.3.4");

      pj_str_t localAddr = localSdp->media[i]->conn->addr;
      pj_str_t remoteAddr = remoteSdp->media[i]->conn->addr;
      std::string localAddrStr(localAddr.ptr, localAddr.slen), remoteAddrStr(remoteAddr.ptr, remoteAddr.slen);
      printf ("LOCAL ADDR %s    REMOTE ADDR %s\n", localAddrStr.c_str(), remoteAddrStr.c_str());

      status = pjmedia_stream_info_from_sdp(&stream_info, pool, mediaEndpoint, localSdp, remoteSdp, i);
      assert(status == PJ_SUCCESS);

      stream_info.param->setting.vad = 0;

      if(!bundle) {
        stream.transport = mediaTransport[i].srtp;
      } else {
        pjmedia_transport_bundle_endpoint_create(mediaTransport[0].bundle, stream_info.ssrc, stream_info.rem_ssrc,
            &stream.transport);
      }

      fprintf(stderr, "WTF\n?");
      pjmedia_stream_create(mediaEndpoint, pool, &stream_info, stream.transport, (void*)this, &stream.stream);
      assert(status == PJ_SUCCESS);

      printf("STREAM ENCODING = %d \n", stream_info.dir & PJMEDIA_DIR_ENCODING);
      printf("STREAM DECODING = %d \n", stream_info.dir & PJMEDIA_DIR_DECODING);

      pjmedia_stream_start(stream.stream);
      assert(status == PJ_SUCCESS);

      status = pjmedia_stream_get_port(stream.stream, &stream.mediaPort);
      assert(status == PJ_SUCCESS);

      stream.inputListenerId = inputTracks[i].get()->addStateListener([this, i](bool enabled) {
        if(!enabled) pjmedia_stream_pause(mediaStreams[i].stream, PJMEDIA_DIR_ENCODING);
        if(enabled) pjmedia_stream_resume(mediaStreams[i].stream, PJMEDIA_DIR_ENCODING);
      });

      if(!inputTracks[i].get()->isEnabled()) pjmedia_stream_pause(stream.stream, PJMEDIA_DIR_ENCODING);

      printf("CREATING MEDIA PORT!\n");
      pjmedia_snd_port_create(pool, PJMEDIA_AUD_DEFAULT_CAPTURE_DEV, PJMEDIA_AUD_DEFAULT_PLAYBACK_DEV,// 1, 0,
                              PJMEDIA_PIA_SRATE(&stream.mediaPort->info), /* clock rate */
                              PJMEDIA_PIA_CCNT(&stream.mediaPort->info), /* channel count */
                              PJMEDIA_PIA_SPF(&stream.mediaPort->info), /* samples per frame*/
                              PJMEDIA_PIA_BITS(&stream.mediaPort->info), /* bits per sample */
                              0, &stream.soundPort);
      assert(status == PJ_SUCCESS);

      status = pjmedia_snd_port_connect(stream.soundPort, stream.mediaPort);
      assert(status == PJ_SUCCESS);

      printf("CONNECTED!\n");
      connectionState = "connected";
      if(onConnectionStateChange) onConnectionStateChange(connectionState);
      iceConnectionState = "connected";
      if(onIceConnectionStateChange) onIceConnectionStateChange(iceConnectionState);

      if(onAddStream) onAddStream();

      //pjmedia_transport_simulate_lost(mediaTransport[i].mux, PJMEDIA_DIR_ENCODING_DECODING, 20);

    }
    scheduleReadStats(5, 0);
  }

  static const char *good_number(char *buf, pj_int32_t val)
  {
    if (val < 1000) {
      pj_ansi_sprintf(buf, "%d", val);
    } else if (val < 1000000) {
      pj_ansi_sprintf(buf, "%d.%dK",
                      val / 1000,
                      (val % 1000) / 100);
    } else {
      pj_ansi_sprintf(buf, "%d.%02dM",
                      val / 1000000,
                      (val % 1000000) / 10000);
    }

    return buf;
  }

  void PeerConnection::readStats() {
    char duration[80], last_update[80];
    char bps[16], ipbps[16], packets[16], bytes[16], ipbytes[16];

    if(closed) return;

    printf("READ STREAM STATS(%zd)!\n", mediaStreams.size());

    for(int i = 0; i < mediaStreams.size(); i++) {
      pjmedia_rtcp_stat stat;
      pjmedia_port *port;
      pj_time_val now;
      pj_gettimeofday(&now);
      pjmedia_stream_get_stat(mediaStreams[i].stream, &stat);
      pjmedia_stream_get_port(mediaStreams[i].stream, &port);

      printf("Stream #%d statistics:\n", i);

      /* Print duration */
      PJ_TIME_VAL_SUB(now, stat.start);
      sprintf(duration, " Duration: %02ld:%02ld:%02ld.%03ld",
              now.sec / 3600,
              (now.sec % 3600) / 60,
              (now.sec % 60),
              now.msec);

      if (stat.rx.update_cnt == 0)
        strcpy(last_update, "never");
      else {
        pj_gettimeofday(&now);
        PJ_TIME_VAL_SUB(now, stat.rx.update);
        sprintf(last_update, "%02ldh:%02ldm:%02ld.%03lds ago",
                now.sec / 3600,
                (now.sec % 3600) / 60,
                now.sec % 60,
                now.msec);
      }

      printf(" RX stat last update: %s\n"
             "    total %s packets %sB received (%sB +IP hdr)%s\n"
             "    pkt loss=%d (%3.1f%%), dup=%d (%3.1f%%), reorder=%d (%3.1f%%)%s\n"
             "          (msec)    min     avg     max     last    dev\n"
             "    loss period: %7.3f %7.3f %7.3f %7.3f %7.3f%s\n"
             "    jitter     : %7.3f %7.3f %7.3f %7.3f %7.3f%s\n",
             last_update,
             good_number(packets, stat.rx.pkt),
             good_number(bytes, stat.rx.bytes),
             good_number(ipbytes, stat.rx.bytes + stat.rx.pkt * 32),
             "",
             stat.rx.loss,
             stat.rx.loss * 100.0 / (stat.rx.pkt + stat.rx.loss),
             stat.rx.dup,
             stat.rx.dup * 100.0 / (stat.rx.pkt + stat.rx.loss),
             stat.rx.reorder,
             stat.rx.reorder * 100.0 / (stat.rx.pkt + stat.rx.loss),
             "",
             stat.rx.loss_period.min / 1000.0,
             stat.rx.loss_period.mean / 1000.0,
             stat.rx.loss_period.max / 1000.0,
             stat.rx.loss_period.last / 1000.0,
             pj_math_stat_get_stddev(&stat.rx.loss_period) / 1000.0,
             "",
             stat.rx.jitter.min / 1000.0,
             stat.rx.jitter.mean / 1000.0,
             stat.rx.jitter.max / 1000.0,
             stat.rx.jitter.last / 1000.0,
             pj_math_stat_get_stddev(&stat.rx.jitter) / 1000.0,
             ""
      );

      if (stat.tx.update_cnt == 0)
        strcpy(last_update, "never");
      else {
        pj_gettimeofday(&now);
        PJ_TIME_VAL_SUB(now, stat.tx.update);
        sprintf(last_update, "%02ldh:%02ldm:%02ld.%03lds ago",
                now.sec / 3600,
                (now.sec % 3600) / 60,
                now.sec % 60,
                now.msec);
      }

      printf(" TX stat last update: %s\n"
             "    total %s packets %sB sent (%sB +IP hdr)%s\n"
             "    pkt loss=%d (%3.1f%%), dup=%d (%3.1f%%), reorder=%d (%3.1f%%)%s\n"
             "          (msec)    min     avg     max     last    dev\n"
             "    loss period: %7.3f %7.3f %7.3f %7.3f %7.3f%s\n"
             "    jitter     : %7.3f %7.3f %7.3f %7.3f %7.3f%s\n",
             last_update,
             good_number(packets, stat.tx.pkt),
             good_number(bytes, stat.tx.bytes),
             good_number(ipbytes, stat.tx.bytes + stat.tx.pkt * 32),
             "",
             stat.tx.loss,
             stat.tx.loss * 100.0 / (stat.tx.pkt + stat.tx.loss),
             stat.tx.dup,
             stat.tx.dup * 100.0 / (stat.tx.pkt + stat.tx.loss),
             stat.tx.reorder,
             stat.tx.reorder * 100.0 / (stat.tx.pkt + stat.tx.loss),
             "",
             stat.tx.loss_period.min / 1000.0,
             stat.tx.loss_period.mean / 1000.0,
             stat.tx.loss_period.max / 1000.0,
             stat.tx.loss_period.last / 1000.0,
             pj_math_stat_get_stddev(&stat.tx.loss_period) / 1000.0,
             "",
             stat.tx.jitter.min / 1000.0,
             stat.tx.jitter.mean / 1000.0,
             stat.tx.jitter.max / 1000.0,
             stat.tx.jitter.last / 1000.0,
             pj_math_stat_get_stddev(&stat.tx.jitter) / 1000.0,
             ""
      );


      printf(" RTT delay     : %7.3f %7.3f %7.3f %7.3f %7.3f%s\n",
             stat.rtt.min / 1000.0,
             stat.rtt.mean / 1000.0,
             stat.rtt.max / 1000.0,
             stat.rtt.last / 1000.0,
             pj_math_stat_get_stddev(&stat.rtt) / 1000.0,
             ""
      );

      /*if(now.sec > 10) {
        connectionState = "failed";
        if(onConnectionStateChange) onConnectionStateChange(connectionState);
        iceConnectionState = "failed";
        if(onIceConnectionStateChange) onIceConnectionStateChange(iceConnectionState);
        handleDisconnect();
      }*/

      pjmedia_stream_rtp_sess_info rtp_info;
      pjmedia_stream_get_rtp_session_info(mediaStreams[i].stream, &rtp_info);
      unsigned int rtpTs = rtp_info.rtcp->rtp_last_ts;
      if(rtpTs == lastRtpTs) {
        printf("CLOSING BECAUSE POOR NETWORK CONDITIONS!\n");
        return handleDisconnect();
      }
      lastRtpTs = rtpTs;

    }

    scheduleReadStats(1, 0);
  }

  void statTimerCb(pj_timer_heap_t *ht, pj_timer_entry *e) {
    PeerConnection* pc = (PeerConnection*)e->user_data;
    pc->readStats();
  }
  void PeerConnection::scheduleReadStats(int secs, int msecs) {
    pj_status_t status;
    pj_time_val delay;
    delay.sec = secs;
    delay.msec = msecs;
    pj_timer_entry_init(&statTimerEntry, 0, (void*)this, statTimerCb );
    /*statTimerEntry.user_data = (void*)this;
    statTimerEntry.cb = statTimerCb;*/
    status = pj_timer_heap_schedule(timerHeap, &statTimerEntry, &delay);
    assert(status == PJ_SUCCESS);
  }

  void PeerConnection::handleDisconnect() {
    printf("STOP MEDIA!!!\n");
    pj_status_t status;
    if(closed) return;
    closed = true;
    for(int i = 0; i < mediaStreams.size(); i++) {
      pjmedia_snd_port_disconnect(mediaStreams[i].soundPort);
      pjmedia_stream_destroy(mediaStreams[i].stream);
      pjmedia_port_destroy(mediaStreams[i].mediaPort);
      pjmedia_snd_port_destroy(mediaStreams[i].soundPort);
      inputTracks[i].get()->removeStateListener(mediaStreams[i].inputListenerId);
    }
    for(int i = 0; i < mediaTransport.size(); i++) {
      pjmedia_transport_close(mediaTransport[i].srtp);
    }
    connectionState = "closed";
    if(onConnectionStateChange) onConnectionStateChange(connectionState);
    iceConnectionState = "closed";
    if(onIceConnectionStateChange) onIceConnectionStateChange(iceConnectionState);
  }

  void PeerConnection::close() {
    handleDisconnect();
  }

  PeerConnection::~PeerConnection() {
    if(!closed) handleDisconnect();

    pj_pool_release(pool);
  }

  void PeerConnection::handleIceTransportFailed(pjmedia_transport *pTransport) {
    printf("ICE TRANSPORT FAILED!!!\n");
    iceConnectionState = "failed";
    if(onIceConnectionStateChange) onIceConnectionStateChange(iceConnectionState);
    connectionState = "failed";
    if(onConnectionStateChange) onConnectionStateChange(connectionState);
  }


}
