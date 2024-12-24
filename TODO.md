- networking does a lot of copying between heap and stack to get around alignment requirements
  - this causes stack overflow errors currently - quick workaround was to bump stack size of the game to 32MB on msvc. @todo modify stack size on all targets or reduce copying in the networking code

- networking sort of works now but I made a big mess while fixing it.
1. refactor the code that is here
2. the idea of dividing big UDP message into smaller "packets" that all need to arrive at the destination is _problematic_. Its essentially an emulation of TCP. Maybe its not AS BAD because we still allow you to drop a portion of udp packets and go forward. But thats what is problematic. If a message is made out of 64 packets and for every message we have a drop of 1 packet the client will not read anything.
3. move to a model where each udp packet has independent portion of info that can be read independetly by the client.
4. udp message size could be dynamic
5. we send an update for X objects with Y latest ticks
6. client sends ACK
7. we reduce client spam for stuff that was ACKed, otherwise we resend (most recent data) - we prioritize things that werent ACKed

^ above system requires us to be able to playback / sync objecets separately, we cant assume that the whole gamestate is updated at once

- support more shapes in collision system
- debug ui interface
