- networking does a lot of copying between heap and stack to get around alignment requirements
- this causes stack overflow errors currently - quick workaround was to bump stack size of the game to 32MB on msvc. @todo modify stack size on all targets or reduce copying in the networking code

- networking
- support more shapes in collision system
- debug ui interface
