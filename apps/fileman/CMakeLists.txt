lexxos_add_app(fileman
        SOURCES fileman.cpp file_operations.cpp input_reader.cpp runtime.cpp
        MANIFEST manifest.json
        MEMORY 65536
        STACK 4096
        CAPABILITIES console,keyboard,storage-read,storage-write)
