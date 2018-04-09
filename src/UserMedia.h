//
// Created by Michał Łaszczewski on 02/02/18.
//

#ifndef PJWEBRTC_USERMEDIA_H
#define PJWEBRTC_USERMEDIA_H

#include <memory>
#include "global.h"

namespace webrtc {

  class PeerConnection;

  struct UserMediaConstraints {

  };

  class UserMedia {
  public:
    friend class PeerConnection;

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
