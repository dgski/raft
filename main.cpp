#include "utils.hpp"
#include "raft.hpp"

int main() {
  srand(time(nullptr));
  std::unordered_map<size_t, raft::Node> nodes;
  std::unordered_map<size_t, std::vector<raft::Message>> messages;
  for (size_t i = 0; i < 5; ++i) {
    nodes.emplace(i, i);
    messages.emplace(i, std::vector<raft::Message>());
  }
  for (auto& [id, node] : nodes) {
    node.setSend([&messages, thisId = id](size_t targetId, raft::Message message) {
      if (targetId == raft::SEND_TO_ALL) {
        for (auto& [id, messagesForId] : messages) {
          if (id != thisId) {
            messagesForId.push_back(message);
          }
        }
      } else {
        messages[targetId].push_back(message);
      }
    });
  }

  auto deliverMessages = [&messages, &nodes]() {
    for (auto& [id, messagesForId] : messages){
      for (auto& message : messagesForId) {
        nodes.at(id).onMessage(message);
      }
      messagesForId.clear();
    }
  };

  while (true) {
    for (auto& [id, node] : nodes) {
      node.onTimer();
    }
    deliverMessages();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    utils::log("-------------------------------------------------");
  }
  return 0;
}