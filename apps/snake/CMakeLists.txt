lexxos_add_app(snake
        SOURCES snake.cpp game.cpp runtime.cpp
        MANIFEST manifest.json
        MEMORY 65536
        STACK 4096
        CAPABILITIES console,keyboard,audio)
