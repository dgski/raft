# Raft

Simple Consensus / Leader Election Protocol implemented in C++.

- Messages:
  - Election Request: (term, node_id)
  - Election Response: (term, node_id, success)
  - Leader Heartbeat: (term, node_id)
  - Discovery Heartbeat: (node_id)

- Node:
  - If it has not received a 'Leader Heartbeat' message for the current term for more than a randomly generated interval, the node sends an election request to all nodes in the network; nominating itself as the potential next leader.
  - If a node receives an 'Election Request' message, it will check whether the incoming term is greater than the current term. If yes, it will vote if it has not yet voted for this term. If no, it will ignore the message.
  - If a node receives an 'Election Response' message as a response to it's 'Election Request' message, it will add that to the tally of votes it has received. If the number of votes matches or exceeds quorum, it will assume it is the new leader and start broadcasting a heartbeat.