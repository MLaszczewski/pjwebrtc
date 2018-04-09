//
// Created by Michał Łaszczewski on 02/02/18.
//


#include <pjnath/stun_auth.h>
#include <pjmedia.h>
#include "PeerConnection.h"

namespace webrtc {

  void onIceComplete(pjmedia_transport *tp, pj_ice_strans_op op, pj_status_t status){
    assert(status == PJ_SUCCESS);
    PeerConnection* pc = (PeerConnection*)tp->user_data;
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


  PeerConnection::PeerConnection(PeerConnectionConfiguration& configurationp) : configuration(configurationp) {
    remoteCandidatesGathered = false;
    sdpGenerated = false;
    transportStarted = false;
    localDescription = nullptr;
    remoteDescription = nullptr;

    iceCompletePromise = nullptr;
    dtlsCompletePromise = nullptr;

    remoteIceCompletePromise = std::make_shared<promise::Promise<bool>>();

    pj_status_t status;
    pool = pj_pool_create(&cachingPool.factory,"PeerConnection.pool", 4096, 4096, NULL);
    assert( pj_timer_heap_create(pool, 100, &timerHeap) == PJ_SUCCESS );

    /* Create the endpoint: */
    status = pjmedia_endpt_create(&cachingPool.factory, NULL, 1, &mediaEndpoint);

    //pj_bool_t telephony = false;
    //pjmedia_endpt_set_flag(mediaEndpoint, PJMEDIA_ENDPT_HAS_TELEPHONE_EVENT_FLAG, &telephony);

    assert(status == PJ_SUCCESS);
    status = pjmedia_codec_g711_init(mediaEndpoint);
    assert(status == PJ_SUCCESS);
    status = pjmedia_codec_g722_init(mediaEndpoint);
    assert(status == PJ_SUCCESS);
    status = pjmedia_codec_ilbc_init(mediaEndpoint, 30);
    assert(status == PJ_SUCCESS);
    status = pjmedia_codec_opus_init(mediaEndpoint);
    assert(status == PJ_SUCCESS);

    mediaTransportsIceInitializedCount = 0;
    mediaTransportsDtlsInitializedCount = 0;

    pj_ioqueue_create(pool, 16, &ioqueue);

    pj_ice_strans_cfg_default(&iceTransportConfiguration);
    auto & cfg = iceTransportConfiguration;
    pj_stun_config_init(&cfg.stun_cfg, &cachingPool.factory, 0, ioqueue, timerHeap);

    cfg.turn.conn_type = PJ_TURN_TP_UDP;

    /// TODO: move turn configuration elsewere
    auto& stun1 = cfg.stun_tp[cfg.stun_tp_cnt++];
    pj_ice_strans_stun_cfg_default(&stun1);
    stun1.server = pj_strdup3(pool, "turn.xaos.ninja");
    stun1.port = 4433;
    stun1.af = pj_AF_INET();

    auto& turn1 = cfg.turn_tp[cfg.turn_tp_cnt++];
    pj_ice_strans_turn_cfg_default(&turn1);
    turn1.server =  pj_strdup3(pool, "turn.xaos.ninja");
    turn1.port = 4433;
    turn1.af = pj_AF_INET();
    turn1.auth_cred.type = PJ_STUN_AUTH_CRED_STATIC;
    turn1.auth_cred.data.static_cred.username = pj_strdup3(pool, "test");
    turn1.auth_cred.data.static_cred.data_type = PJ_STUN_PASSWD_PLAIN;
    turn1.auth_cred.data.static_cred.data = pj_strdup3(pool, "12345");
/*
    auto& turn2 = cfg.turn_tp[cfg.turn_tp_cnt++];
    turn2 = turn1;
    turn2.af = pj_AF_INET6();

    auto& stun2 = cfg.stun_tp[cfg.stun_tp_cnt++];
    stun2 = stun1;
    stun2.af = pj_AF_INET6();*/

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
    inputStreams.push_back(userMedia);
    if(inputStreams.size() > mediaTransport.size()) gatherIceCandidates(inputStreams.size() - mediaTransport.size());
  }

  std::shared_ptr<promise::Promise<bool>> PeerConnection::gatherIceCandidates(int streamsCount) {
    /// TODO: Create promise here
    if(!iceCompletePromise || iceCompletePromise->state == promise::Promise<bool>::PromiseState::Resolved)
      iceCompletePromise = std::make_shared<promise::Promise<bool>>();

    pj_status_t status;
    for (int i = 0; i < streamsCount; i++) {
      mediaTransport.push_back(MediaTransport{nullptr, nullptr}); // make place for new transport
      auto& transport = mediaTransport[mediaTransport.size()-1];
      status = pjmedia_ice_create3(mediaEndpoint, NULL, 1, &iceTransportConfiguration, &iceCallbacks, 0,
                                   (void*)this, &transport.ice);
      assert(status == PJ_SUCCESS);

      status = pjmedia_transport_srtp_create(mediaEndpoint, transport.ice, &srtpSetting, &transport.srtp);
      assert(status == PJ_SUCCESS);

      status = pjmedia_transport_mux_create(mediaEndpoint, transport.srtp, &transport.mux);
      assert(status == PJ_SUCCESS);
    }

    return iceCompletePromise;
  }

  void PeerConnection::handleIceTransportComplete(pjmedia_transport *pTransport) {
    printf("ICE COMPLETE?!\n");
    mediaTransportsIceInitializedCount++;
    if(mediaTransportsIceInitializedCount == mediaTransport.size()) {
      printf("ICE COMPLETE!!\n");
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
    if(mediaTransport.size() == 0) throw "zrob tu errora";
    return iceCompletePromise->then<nlohmann::json>([this](bool& v) {
      startTransportIfPossible();
      return promise::Promise<nlohmann::json>::resolved(doCreateOffer());
    });
  }

  std::shared_ptr<promise::Promise<nlohmann::json>> PeerConnection::createAnswer(nlohmann::json offer) {
    if(mediaTransport.size() == 0) throw "zrob tu errora";
    return iceCompletePromise->then<nlohmann::json>([this, offer](bool& v) {
      return remoteIceCompletePromise->then<nlohmann::json>([this, offer](bool& v) {
        startTransportIfPossible();
        return promise::Promise<nlohmann::json>::resolved(doCreateAnswer(offer));
      });
    });
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
      pjmedia_transport_media_create(transport.mux, pool, 0, nullptr, i);
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
      status = pjmedia_transport_encode_sdp(transport.mux, pool, sdp, nullptr, i);
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
      } else {
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

    nlohmann::json sdpJson = { {"type", "offer"}, {"sdp", oss.str() }};

    sdpGenerated = true;

    return sdpJson;
  }

  nlohmann::json PeerConnection::doCreateAnswer(nlohmann::json offer) {
    pj_status_t status;

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
      status = pjmedia_transport_media_create(transport.mux, pool, 0, offerSdp, i);
      assert(status == PJ_SUCCESS);
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
      status = pjmedia_transport_encode_sdp(transport.mux, pool, sdp, offerSdp, i);
      assert(status == PJ_SUCCESS);
    }

    char buf[10240];
    size_t offerSize = pjmedia_sdp_print(sdp, buf, 10240);
    std::string rawSdpString(buf, offerSize);
    //printf("\nRAW SDP:\n%s\n", rawSdpString.c_str());

    std::istringstream iss(rawSdpString);
    oss.str("");
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
      } else {
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

    nlohmann::json sdpJson = { {"type", "answer"}, {"sdp", oss.str() }};

    sdpGenerated = true;

    return sdpJson;
  }

  void PeerConnection::setLocalDescription(nlohmann::json sdp) {
    localDescription = sdp;
    startTransportIfPossible();
  }
  void PeerConnection::setRemoteDescription(nlohmann::json sdp) {
    remoteDescription = sdp;
    startTransportIfPossible();
  }
  void PeerConnection::addIceCandidate(nlohmann::json candidate) {
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

    mediaStreams.resize(mediaTransport.size());

    for(int i = 0; i < mediaTransport.size(); i++) {
      auto &transport = mediaTransport[i].mux;
      status = pjmedia_transport_media_start(transport, pool, localSdp, remoteSdp, i);
      assert(status == PJ_SUCCESS);
    }
    printf("MEDIA TRANSPORTS STARTED\n");

    dtlsCompletePromise->onResolved([this](bool ok){
      startMedia();
    });
  }

  void PeerConnection::startMedia() {

    printf("START MEDIA!!!\n");
    for(int i = 0; i < mediaTransport.size(); i++) {
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

      pjmedia_stream_create(mediaEndpoint, pool, &stream_info, mediaTransport[i].mux, (void*)this, &stream.stream);
      assert(status == PJ_SUCCESS);

      printf("STREAM ENCODING = %d \n", stream_info.dir & PJMEDIA_DIR_ENCODING);
      printf("STREAM DECODING = %d \n", stream_info.dir & PJMEDIA_DIR_DECODING);

      pjmedia_stream_start(stream.stream);
      assert(status == PJ_SUCCESS);

      status = pjmedia_stream_get_port(stream.stream, &stream.mediaPort);
      assert(status == PJ_SUCCESS);

      pjmedia_snd_port_create(pool, /*PJMEDIA_AUD_DEFAULT_CAPTURE_DEV, PJMEDIA_AUD_DEFAULT_PLAYBACK_DEV,*/ 1, 0,
                              PJMEDIA_PIA_SRATE(&stream.mediaPort->info), /* clock rate */
                              PJMEDIA_PIA_CCNT(&stream.mediaPort->info), /* channel count */
                              PJMEDIA_PIA_SPF(&stream.mediaPort->info), /* samples per frame*/
                              PJMEDIA_PIA_BITS(&stream.mediaPort->info), /* bits per sample */
                              0, &stream.soundPort);
      assert(status == PJ_SUCCESS);

      status = pjmedia_snd_port_connect(stream.soundPort, stream.mediaPort);
      assert(status == PJ_SUCCESS);

    }
  }

}
