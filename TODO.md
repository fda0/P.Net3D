- animated models (bone animation, skinned meshes)
- debug ui interface
- multithreading (+ put input polling on client thread)
- support more shapes in the collision system (add circles!)
- pathfinding
- gameplay gameplay gameplay

# Network
I think networking is in a good enough state for local (single PC) development. I would like to make more progress on all other parts of the engine before making decisions about how to optimize network transfers. Two things I'll want to do eventually:
- send less data - bitpacking, data compression; don't send zeroes, etc.
- client-server acknowledgments - compress payloads against data that was confirmed as received by the other side
