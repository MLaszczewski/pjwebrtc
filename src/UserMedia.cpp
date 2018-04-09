//
// Created by Michał Łaszczewski on 02/02/18.
//

#include "UserMedia.h"

namespace webrtc {

  UserMedia::UserMedia(UserMediaConstraints& constraintsp) : constraints(constraintsp) {

  }

  UserMedia::~UserMedia() {

  }

  void UserMedia::init() {

  }

  int UserMedia::getTransportsCount() {
    return 1; // 1 for audio, 2 for audio/video
  }

  std::shared_ptr<UserMedia> UserMedia::getUserMedia(UserMediaConstraints& constraintsp) {
    auto um = std::make_shared<UserMedia>(constraintsp);
    um->init();
    return um;
  }

}