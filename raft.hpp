#pragma once

#include <chrono>
#include <variant>
#include <functional>
#include <string>
#include <format>

#include "utils.hpp"

namespace raft {

// Constants
constexpr size_t SEND_TO_ALL = 0;

// Messages
struct RequestVote { size_t term; size_t nodeId; };
struct ResponseVote { size_t term; size_t nodeId; bool success; };
struct Heartbeat { size_t term; size_t nodeId; };
using Message = std::variant<RequestVote, ResponseVote, Heartbeat>;

// States
struct Uninitialized {};
struct Sleeping { std::chrono::system_clock::time_point wakeupTime; };
struct OtherNodeIsLeader { size_t nodeId; };
struct ThisNodeIsLeader {};
struct RunningForLeader { size_t votes; };
struct VotedFor { size_t nodeId; };
using State = std::variant<Uninitialized, Sleeping, OtherNodeIsLeader, ThisNodeIsLeader, RunningForLeader, VotedFor>;
std::string stateToString(const State& state) {
  if (std::holds_alternative<Uninitialized>(state)) {
    return "Uninitialized";
  } else if (auto otherNodeIsLeader = std::get_if<OtherNodeIsLeader>(&state); otherNodeIsLeader) {
    return "OtherNodeIsLeader{ " + std::to_string(otherNodeIsLeader->nodeId) + " }";
  } else if (std::holds_alternative<ThisNodeIsLeader>(state)) {
    return "ThisNodeIsLeader";
  } else if (auto runningForLeader = std::get_if<RunningForLeader>(&state); runningForLeader) {
    return "RunningForLeader{ " + std::to_string(runningForLeader->votes) + " }";
  } else if (auto votedFor = std::get_if<VotedFor>(&state); votedFor) {
    return "VotedFor{ " + std::to_string(votedFor->nodeId) + " }";
  } else if (std::holds_alternative<Sleeping>(state)) {
    return "Sleeping";
  }
  return "Unknown";
}

class Node {
  size_t _id;
  const std::chrono::milliseconds _timeoutInterval;
  std::function<void(size_t, Message)> _send;
  size_t _currentTerm = 0;
  std::chrono::system_clock::time_point _lastEvent = std::chrono::system_clock::now();
  std::chrono::system_clock::time_point _relinquishleadershipTimepoint;
  State _state = Uninitialized();
// ---------------------------
  bool leaderIsMissing() {
    return _lastEvent + _timeoutInterval < std::chrono::system_clock::now();
  }
  bool isLeader() {
    return std::holds_alternative<ThisNodeIsLeader>(_state);
  }
  void startElection() {
    if (isSleeping()) {
      return;
    }
    if (std::holds_alternative<RunningForLeader>(_state)) {
      return;
    }
    _state = RunningForLeader{1};
    ++_currentTerm;
    _send(SEND_TO_ALL, RequestVote{_currentTerm, _id});
    _lastEvent = std::chrono::system_clock::now();
  }
  bool relinquishLeadership() {
    return _relinquishleadershipTimepoint < std::chrono::system_clock::now();
  }
  bool isSleeping() {
    if (auto sleeping = std::get_if<Sleeping>(&_state); sleeping) {
      if (sleeping->wakeupTime < std::chrono::system_clock::now()) {
        _state = Uninitialized();
      }
      return true;
    }
    return false;
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
      _lastEvent = std::chrono::system_clock::now();
      if (relinquishLeadership()) {
        _state = Sleeping{std::chrono::system_clock::now() + std::chrono::seconds(5)};
      }
    }

    utils::log("Node {} term={} state={}", _id, _currentTerm, stateToString(_state));
  }
  void onMessage(const Message& message) {
    std::visit(*this, message);
  }
  void operator()(const RequestVote& request) {
    if (request.term > _currentTerm) {
      _currentTerm = request.term;
      _send(request.nodeId, ResponseVote{_currentTerm, _id, true});
      _state = VotedFor{request.nodeId};
      _lastEvent = std::chrono::system_clock::now();
    } else if (request.term == _currentTerm) {
      auto runningForLeader = std::get_if<RunningForLeader>(&_state);
      if (runningForLeader) {
        _send(request.nodeId, ResponseVote{_currentTerm, _id, false});
        return;
      }
      auto votedFor = std::get_if<VotedFor>(&_state);
      if (!votedFor || (votedFor->nodeId == request.nodeId)) {
        _send(request.nodeId, ResponseVote{_currentTerm, _id, true});
        _state = VotedFor{request.nodeId};
        _lastEvent = std::chrono::system_clock::now();
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
          _send(SEND_TO_ALL, Heartbeat{_currentTerm, _id});
          _relinquishleadershipTimepoint = std::chrono::system_clock::now() + std::chrono::seconds(5);
        }
      }
    }
  }
  void operator()(const Heartbeat& heartbeat) {
    _state = OtherNodeIsLeader{heartbeat.nodeId};
    _lastEvent = std::chrono::system_clock::now();
    _currentTerm = heartbeat.term;
  }
};

} // namespace raft