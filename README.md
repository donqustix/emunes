# emunes
Noexcept quality NES emulator written in C++17.\*

Passes almost all Blargg's tests: vbl/nmi & instruction timing; dummy reads; sprite behaviour.

Runs a large amount of games with mappers: 0, 1, 2, 3, 7.

## Building
You need a compiler that supports C++17.  
You need an SDL2 library to be installed in your system.

**Enter the following commands to build for Linux**
```
apt install libsdl2-dev
cd project_root_directory
mkdir bin
make
```

Now, you are ready to use the emulator.

Use 'emunes filepath' to load a game.

\*Source code contains **noexcept** keyword.

## Screenshots
![alt tag](/res/images/1.png)
![alt tag](/res/images/2.png)
![alt tag](/res/images/3.png)

