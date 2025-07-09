3D game/engine with networking.
[Video clip 4](https://www.youtube.com/watch?v=VxGE_yR9gus)
[Video clip 3](https://www.youtube.com/watch?v=QV_7TaHOlao)
[Video clip 2](https://www.youtube.com/watch?v=tm6N5Fy2Jxs)
[Video clip 1](https://www.youtube.com/watch?v=567seD6WJco)
[Video clip 0](https://www.youtube.com/watch?v=v2UXzL0xuF4)


# Cloning the project
To download the repository and its submodules (SDL), run
```bash
git clone git@github.com:fda0/Treasure.git
git submodule update --init --recursive
```


# Building
Building the game and its dependency libraries (SDL .dll)
```
jai compile.jai - all
```

Generating asset file (data.pie)
```
cd build
Baker.exe
```


# Educational resources
Things I found useful while working on this.

## Collision
The game uses the SAT algorithm for collision detection.
Two resources I found especially useful when researching its implementation:
- [Collision Detection with SAT (Math for Game Developers) by pikuma](https://www.youtube.com/watch?v=-EsWKT7Doww)
- [N Tutorial A – Collision Detection and Response](https://www.metanetsoftware.com/2016/n-tutorial-a-collision-detection-and-response)

## Networking
[Gaffer on Games](https://gafferongames.com/categories/game-networking/) articles are great to get an overview of various networking topics.

## Math
I used the [Handmade Math](https://github.com/HandmadeMath/HandmadeMath) library as a reference for matrix and quaternion math implementations.

Resources on quaternions:
- [Quaternions, Double-cover, and the Rest Pose Neighborhood (2006) by Casey Muratori](https://caseymuratori.com/blog_0002) + [video](https://www.youtube.com/watch?v=vmAY5kP-tpU)
- [Understanding Slerp, Then Not Using It by Jonathan Blow](http://number-none.com/product/Understanding%20Slerp,%20Then%20Not%20Using%20It/)

## Using .gltf files - models and animations
- https://github.com/jkuhlmann/cgltf - small portable library that parses .gltf files and gives you structs and arrays of data.
- [Skeletal animation in glTF - lisyarus blog](https://lisyarus.github.io/blog/posts/gltf-animation.html) - the best article on the topic of using animations from .gltf files that I found.

## Shadow mapping
- [Shadow maps, shadow volumes, deep shadow maps by Justin Solomon](https://www.youtube.com/watch?v=QCIKgyL3ePo) - MIT lecture - a good overview of different shadow rendering techniques.
- [Shadow Mapping learnopengl.com](https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping) - good step by step tutorial for getting shadow maps on the screen. Explains common issues.
