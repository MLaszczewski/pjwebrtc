//
// Created by Michał Łaszczewski on 02/02/18.
//

#ifndef PJWEBRTC_USERMEDIA_H
#define PJWEBRTC_USERMEDIA_H

#include <memory>
#include <vector>
#include <mutex>
#include "global.h"
#include <functional>

namespace webrtc {

  class PeerConnection;

  struct UserMediaConstraints {
    bool audio;
    bool video;
  };

  class Track {
  private:
    bool enabled;
    std::vector<std::pair<size_t, std::function<void(bool)>>> stateListeners;
    std::mutex stateListenersMutex;
    size_t lastId;

  public:
    Track() : lastId(0), enabled(true) {}

    size_t addStateListener(std::function<void(bool)> stateListener);
    void removeStateListener(size_t id);

    bool isEnabled();
    void setEnabled(bool enabled);
  };

  class UserMedia {
  public:
    friend class PeerConnection;

  public:

    std::vector<std::shared_ptr<Track>> tracks;

    UserMediaConstraints constraints;

    UserMedia(UserMediaConstraints& constraintsp);
    ~UserMedia();

    void init();

  public:

    static std::shared_ptr<UserMedia> getUserMedia(UserMediaConstraints& constraintsp);
    int getTransportsCount();
  };

}

#endif //PJWEBRTC_USERMEDIA_H
