lexxos_add_app(pacman
        SOURCES pacman.cpp game.cpp
        MANIFEST manifest.json
        MEMORY 65536
        STACK 4096
        CAPABILITIES console,keyboard,audio)
