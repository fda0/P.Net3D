- animated models (skinned meshes)
- debug ui interface
- support more shapes in collision system (add circles!)
- pathfinding

# Network
I think networking is in good enough state for local network development. Before making further progress on it I would like to make more progress on the rest of the engine. After that two immediate tasks are:
- network compression (send less data, bitpacking, skip data initialized to zero etc)
- network compression 2: client-server ack (don't resend data that got confirmed as received)
