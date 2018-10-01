//
// Created by Michał Łaszczewski on 02/02/18.
//

#include "UserMedia.h"

namespace webrtc {

  size_t Track::addStateListener(std::function<void(bool)> stateListener) {
    std::lock_guard<std::mutex> lock(stateListenersMutex);
    lastId++;
    stateListeners.push_back(std::make_pair(lastId,stateListener));
  }

  void Track::removeStateListener(size_t id) {
    std::lock_guard<std::mutex> lock(stateListenersMutex);
    for(int i = 0; i < stateListeners.size(); i++) {
      if(stateListeners[i].first == id) {
        stateListeners.erase(stateListeners.begin()+i);
        break;
      }
    }
  }

  bool Track::isEnabled() {
    return enabled;
  }

  void Track::setEnabled(bool enabledp) {
    std::lock_guard<std::mutex> lock(stateListenersMutex);
    enabled = enabledp;
    for(auto& listener : stateListeners) {
      listener.second(enabled);
    }
  }

  UserMedia::UserMedia(UserMediaConstraints& constraintsp) : constraints(constraintsp) {
    if(constraints.audio) tracks.push_back(std::make_shared<Track>());
    if(constraints.video) tracks.push_back(std::make_shared<Track>());
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