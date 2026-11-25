lexxos_add_app(sdk-demo
        SOURCES main.cpp
        MANIFEST manifest.json
        MEMORY 65536
        STACK 8192
        CAPABILITIES console,keyboard,storage-read,storage-write,graphics,clock,audio,devices)
