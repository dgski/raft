#include <iostream>
#include <variant>
#include <optional>
#include <chrono>
#include <functional>

namespace utils {

auto getRandomTimeout() {
  return std::chrono::milliseconds(150 + rand() % 150);
}

} // namespace utils

namespace raft {

constexpr size_t SEND_TO_ALL = 0;

struct RequestVote {
  size_t term;
  size_t nodeId;
};

struct ResponseVote {
  size_t term;
  size_t nodeId;
  bool success;
};

struct Heartbeat {
  size_t term;
  size_t nodeId;
};

using Message = std::variant<RequestVote, ResponseVote, Heartbeat>;

using Uninitialized = std::monostate;

struct OtherNodeIsLeader {
  size_t nodeId;
  std::chrono::time_point<std::chrono::system_clock> _lastHeartbeat;
};

struct ThisNodeIsLeader {
};

struct RunningForLeader {
  size_t votes;
};

struct VotedFor {
  size_t nodeId;
};

using State = std::variant<Uninitialized, OtherNodeIsLeader, ThisNodeIsLeader, RunningForLeader, VotedFor>;

class Node {
  size_t _id;
  const std::chrono::milliseconds _timeoutInterval;
  std::function<void(size_t, Message)> _send;
  size_t _currentTerm = 0;
  State _state = Uninitialized();
// ---------------------------
  bool leaderIsMissing() {
    if (std::holds_alternative<Uninitialized>(_state)) {
      return true;
    }
    if (auto otherNodeIsLeader = std::get_if<OtherNodeIsLeader>(&_state); otherNodeIsLeader) {
      return std::chrono::system_clock::now() - otherNodeIsLeader->_lastHeartbeat > 2 * _timeoutInterval;
    }
  }
  bool isLeader() {
    return std::holds_alternative<ThisNodeIsLeader>(_state);
  }
  void startElection() {
    _state = RunningForLeader{1};
    ++_currentTerm;
    _send(SEND_TO_ALL, RequestVote{_currentTerm, _id});
  }
public:
  Node(size_t id)
    : _id(id),
    _timeoutInterval(utils::getRandomTimeout())
  {}
  void setSend(std::function<void(size_t, Message)> send) {
    _send = send;
  }
  void onTimer() {
    if (leaderIsMissing()) {
      startElection();
    }
    if (isLeader()) {
      _send(SEND_TO_ALL, Heartbeat{_currentTerm, _id});
    }
  }
  void onMessage(const Message& message) {
    std::visit(*this, message);
  }
  void operator()(const RequestVote& request) {
    if (request.term > _currentTerm) {
      _currentTerm = request.term;
      _send(request.nodeId, ResponseVote{_currentTerm, _id, true});
      _state = VotedFor{request.nodeId};
    } else if (request.term == _currentTerm) {
      auto votedFor = std::get_if<VotedFor>(&_state);
      if (!votedFor || (votedFor->nodeId == request.nodeId)) {
        _send(request.nodeId, ResponseVote{_currentTerm, _id, true});
        _state = VotedFor{request.nodeId};
      } else {
        _send(request.nodeId, ResponseVote{_currentTerm, _id, false});
      }
    } else {
      _send(request.nodeId, ResponseVote{_currentTerm, _id, false});
    }
  }
  void operator()(const ResponseVote& response) {
    if (response.term > _currentTerm) {
      _currentTerm = response.term;
      _state = Uninitialized();
    }
    if (auto runningForLeader = std::get_if<RunningForLeader>(&_state); runningForLeader) {
      if (response.success) {
        if (++runningForLeader->votes > 2) {
          _state = ThisNodeIsLeader();
        }
      }
    }
  }
  void operator()(const Heartbeat& heartbeat) {
    if (heartbeat.term <= _currentTerm) {
      return;
    }
    _state = OtherNodeIsLeader{heartbeat.nodeId, std::chrono::system_clock::now()};
  }
};

} // namespace raft

int main() {
  srand(time(nullptr));
  return 0;
}