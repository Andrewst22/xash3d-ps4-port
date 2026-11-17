set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Ruta a tu SDK de PS4 (ajustada a donde lo tienes en Descargas)
set(OO_PS4_TOOLCHAIN "$ENV{HOME}/Descargas/OpenOrbis/PS4Toolchain" CACHE PATH "OpenOrbis Toolchain")

# Configurar los compiladores
set(CMAKE_C_COMPILER ${OO_PS4_TOOLCHAIN}/bin/clang)
set(CMAKE_CXX_COMPILER ${OO_PS4_TOOLCHAIN}/bin/clang++)
set(CMAKE_LINKER ${OO_PS4_TOOLCHAIN}/bin/ld.lld)

# Flags del compilador para PS4 (FreeBSD 12)
set(CMAKE_C_FLAGS_INIT "--target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -isysroot ${OO_PS4_TOOLCHAIN} -isystem ${OO_PS4_TOOLCHAIN}/include")
set(CMAKE_CXX_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} -isystem ${OO_PS4_TOOLCHAIN}/include/c++/v1")

# Flags del Linker
set(CMAKE_EXE_LINKER_FLAGS_INIT "-m elf_x86_64 -pie --script ${OO_PS4_TOOLCHAIN}/link.x --eh-frame-hdr -L${OO_PS4_TOOLCHAIN}/lib")

# Buscar librerías en el Toolchain
set(CMAKE_FIND_ROOT_PATH ${OO_PS4_TOOLCHAIN})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
