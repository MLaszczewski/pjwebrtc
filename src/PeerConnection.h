//
// Created by Michał Łaszczewski on 02/02/18.
//

#ifndef PJWEBRTC_PEERCONNECTION_H
#define PJWEBRTC_PEERCONNECTION_H

#include <vector>
#include "UserMedia.h"
#include "global.h"
#include "Promise.h"
#include <json.hpp>

namespace webrtc {

  struct PeerConnectionConfiguration {
    nlohmann::json iceServers;
  };

  struct MediaTransport {
    pjmedia_transport* ice;
    pjmedia_transport* srtp;
    //pjmedia_transport* mux;
  };

  struct MediaStream {
    pjmedia_stream* stream;
    pjmedia_port* mediaPort;
    pjmedia_snd_port* soundPort;
  };

  class PeerConnection {
  private:
    pjmedia_endpt *mediaEndpoint;
    std::vector<MediaTransport> mediaTransport; /* Media stream transport	*/
    int mediaTransportsIceInitializedCount;
    int mediaTransportsDtlsInitializedCount;
    pj_ice_strans_cfg iceTransportConfiguration;
    pj_pool_t* pool;

    std::vector<MediaStream> mediaStreams;

    pjmedia_srtp_setting srtpSetting;

    std::vector<std::shared_ptr<UserMedia>> inputStreams;

    std::shared_ptr<promise::Promise<bool>> iceCompletePromise;
    std::shared_ptr<promise::Promise<bool>> dtlsCompletePromise;

    nlohmann::json doCreateOffer();
    nlohmann::json doCreateAnswer(nlohmann::json offer);

    nlohmann::json localDescription;
    nlohmann::json remoteDescription;

    std::string localWithIce;
    std::string remoteWithIce;

    pjmedia_sdp_session *localSdp;
    pjmedia_sdp_session *remoteSdp;

    std::vector<nlohmann::json> remoteCandidates;
    bool remoteCandidatesGathered;
    std::shared_ptr<promise::Promise<bool>> remoteIceCompletePromise;

    bool sdpGenerated;
    bool transportStarted;

    void startTransportIfPossible();
    void startMedia();

    pj_timer_entry statTimerEntry;

    void readStats();
    void scheduleReadStats(int secs, int msecs);

    friend void  statTimerCb(pj_timer_heap_t *ht, pj_timer_entry *e);

    void addIceServer(std::string& url, std::string username, std::string password);

    void handleDisconnect();
    bool closed;

    unsigned int lastRtpTs;

  public:
    pj_ioqueue_t* ioqueue;
    pj_timer_heap_t* timerHeap;


    std::string iceGatheringState;
    std::function<void(std::string)> onIceGatheringStateChange;
    std::string iceConnectionState;
    std::function<void(std::string)> onIceConnectionStateChange;
    std::string connectionState;
    std::function<void(std::string)> onConnectionStateChange;
    std::string signalingState;
    std::function<void(std::string)> onSignalingStateChange;


    std::vector<nlohmann::json> localCandidates;

    PeerConnectionConfiguration configuration;

    PeerConnection();
    ~PeerConnection();

    void init(PeerConnectionConfiguration& configurationp);

    void addStream(std::shared_ptr<UserMedia> userMedia);
    std::shared_ptr<promise::Promise<bool>> gatherIceCandidates(int streamsCount);


    std::shared_ptr<promise::Promise<nlohmann::json>> createOffer();
    std::shared_ptr<promise::Promise<nlohmann::json>> createAnswer();
    void setLocalDescription(nlohmann::json sdp);
    void setRemoteDescription(nlohmann::json sdp);

    void addIceCandidate(nlohmann::json candidate);

    void close();

   /// callbacks:
    void handleIceTransportComplete(pjmedia_transport *pTransport);
    void handleDtlsTransportComplete(pjmedia_transport *pTransport);
  };

}


#endif //PJWEBRTC_PEERCONNECTION_H
