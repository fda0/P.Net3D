- Interpolate client state based on render DT instead of tick DT. The goal is to make both cases look good: [270fps monitor + 16 tick rate] and [30fps monitor + 128 tick rate].
- Explore client side input prediction improvements.
- Debug ui interface.
- Multithreading (+ put input polling on client thread)/
- Support more shapes in the collision system (add circles!).
- Pathfinding.
- gameplay gameplay gameplay

- Eventually optimize network transfer - bitpacking, data compression; don't send zeroes, etc.
- Client-server acknowledgments - compress payloads against data that was confirmed as received by the other side.
