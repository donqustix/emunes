# emunes
Noexcept quality NES emulator written in C++17.\*

Passes almost all Blargg's tests: vbl/nmi & instruction timing; dummy reads; sprite behaviour.

Runs a large amount of games with mappers: 0, 2, 3, 7.

## Building
You need a compiler that supports C++17.  
You need an SDL2 library to be installed in your system.

**Enter the following commands to build for Linux**
```
apt install libsdl2-dev libsdl2-net-dev
cd project_root_directory
mkdir bin
make
```

Now, you are ready to use the emulator. Put 'emunes --help' in your terminal.

Use 'emunes --server rom port' to create a server, where 'rom' is a file path.  
Use 'emunes --client ip port' to connect to a server.  
Use 'emunes --help' to print the information above.

\*Source code contains **noexcept** keyword.

## Screenshots
![alt tag](/res/images/battletoads/screenshot1.png)
![alt tag](/res/images/battletoads/screenshot2.png)
![alt tag](/res/images/battletoads/screenshot3.png)
![alt tag](/res/images/battletoads/screenshot4.png)

![alt tag](/res/images/castlevania/screenshot0.png)
![alt tag](/res/images/castlevania/screenshot1.png)

![alt tag](/res/images/marble_madness/screenshot0.png)
![alt tag](/res/images/marble_madness/screenshot1.png)
![alt tag](/res/images/marble_madness/screenshot2.png)
![alt tag](/res/images/marble_madness/screenshot3.png)
![alt tag](/res/images/marble_madness/screenshot4.png)

![alt tag](/res/images/wizards/screenshot0.png)
![alt tag](/res/images/wizards/screenshot1.png)
