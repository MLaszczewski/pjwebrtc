var localAudio;
var localStream;
var remoteAudio;
var peerConnection;
var uuid;
var serverConnection;

var peerConnectionConfig = {
  iceServers: [
    {"urls": "turn:turn.xaos.ninja:4433", username:"test", credential: "12345"}
  ]
};

function pageReady() {
  uuid = createUUID();

  localAudio = document.getElementById('localAudio');
  remoteAudio = document.getElementById('remoteAudio');

  serverConnection = new WebSocket('ws://' + window.location.hostname + ':8338');
  serverConnection.onmessage = gotMessageFromServer;

  var constraints = {
    video: false,
    audio: true,
  };

  if(navigator.mediaDevices.getUserMedia) {
    navigator.mediaDevices.getUserMedia(constraints).then(getUserMediaSuccess).catch(errorHandler);
  } else {
    alert('Your browser does not support getUserMedia API');
  }
}

function getUserMediaSuccess(stream) {
  localStream = stream;
  localAudio.srcObject = stream;
}

function start(isCaller) {
  peerConnection = new RTCPeerConnection(peerConnectionConfig);
  peerConnection.onicecandidate = gotIceCandidate;
  peerConnection.ontrack = gotRemoteStream;
  peerConnection.addStream(localStream);

  peerConnection.oniceconnectionstatechange = () => {
    console.log("ICE CONNECTION STATE:",peerConnection.iceConnectionState);
  }
  peerConnection.onconnectionstatechange = () => {
    console.log("CONNECTION STATE:",peerConnection.connectionState);
  }
  peerConnection.onsignalingstatechange = () => {
    console.log("SIGNALING STATE:",peerConnection.signalingState);
  }
  peerConnection.onidentityresult = (ev) => {
    console.log("IDENTITY RESULT:",ev);
  }
  peerConnection.onidpassertionerror = (ev) => {
    console.log("IDP ASSERTION ERROR:",ev);
  }
  peerConnection.onidpvalidationerror = (ev) => {
    console.log("IDP VALIDATION ERROR:",ev);
  }
  peerConnection.onnegotiationneeded = (ev) => {
    console.log("NEGOTIATION NEEDED:",ev);
  }
  peerConnection.onpeeridentity = (ev) => {
    console.log("PEER IDENTITY", ev);
  }


  if(isCaller) {
    peerConnection.createOffer().then(createdDescription).catch(errorHandler);
  }
}

function gotMessageFromServer(message) {
  if(!peerConnection) start(false);

  var signal = JSON.parse(message.data);

  // Ignore messages from ourself
  if(signal.uuid == uuid) return;

  if(signal.sdp) {
    peerConnection.setRemoteDescription(new RTCSessionDescription(signal.sdp)).then(function() {
      // Only create answers in response to offers
      if(signal.sdp.type == 'offer') {
        console.log("SDP ANSWER")
        peerConnection.createAnswer().then(createdDescription).catch(errorHandler);
      }
    }).catch(errorHandler);
  } else if(signal.ice) {
    peerConnection.addIceCandidate(new RTCIceCandidate(signal.ice)).catch(errorHandler);
  }
}

function gotIceCandidate(event) {
  //if(event.candidate != null) {
    serverConnection.send(JSON.stringify({'ice': event.candidate, 'uuid': uuid}));
  //}
}

function createdDescription(description) {
  console.log('got description');

  peerConnection.setLocalDescription(description).then(function() {
    serverConnection.send(JSON.stringify({'sdp': peerConnection.localDescription, 'uuid': uuid}));
  }).catch(errorHandler);
}

function gotRemoteStream(event) {
  console.log('got remote stream');
  remoteAudio.srcObject = event.streams[0];
  //remoteAudio.volume = 0.0;
}

function errorHandler(error) {
  console.log(error);
}

// Taken from http://stackoverflow.com/a/105074/515584
// Strictly speaking, it's not a real UUID, but it gets the job done here
function createUUID() {
  function s4() {
    return Math.floor((1 + Math.random()) * 0x10000).toString(16).substring(1);
  }

  return s4() + s4() + '-' + s4() + '-' + s4() + '-' + s4() + '-' + s4() + s4() + s4();
}
