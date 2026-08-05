

# nyukomatic
[![Version Badge](https://img.shields.io/github/v/release/alexanderk23/nyukomatic)](https://github.com/alexanderk23/nyukomatic/releases/latest)
[![Build](https://github.com/alexanderk23/nyukomatic/actions/workflows/cmake.yml/badge.svg?event=push)](https://github.com/alexanderk23/nyukomatic/actions/workflows/cmake.yml)

Una herramienta de programación en vivo para ZX Spectrum inspirada en [Bazematic](https://github.com/gasman/bazematic)
y [Bonzomatic](https://github.com/Gargaj/Bonzomatic).

![scrshot](https://github.com/user-attachments/assets/5acc8853-b786-46a9-94a8-9188b601e03e)

[**Descargar la última versión**](https://github.com/alexanderk23/nyukomatic/releases/latest) •
[Informar de un error](https://github.com/alexanderk23/nyukomatic/issues/new?labels=bug&template=bug_report.md) •
[Solicitar una nueva función](https://github.com/alexanderk23/nyukomatic/issues/new?labels=enhancement&template=feature_request.md)

## Acerca del proyecto
Al igual que otras herramientas de [programación en vivo](https://livecode.demozoo.org),
**nyukomatic** combina un editor de código y un ensamblador Z80, lo que te permite ver los resultados de los cambios en el código en tiempo real. A diferencia de [Bazematic](https://github.com/gasman/bazematic),
que se ejecuta en un navegador y utiliza [RASM](https://github.com/EdouardBERGE/rasm) como compilador, es un ejecutable nativo independiente (Win/Linux/macOS) y utiliza [SJASMPlus](https://github.com/z00m128/sjasmplus),
que es más familiar para muchos programadores Z80 en cuanto a sintaxis.

Continuando con la tradición ya establecida de nombrar las herramientas de programación en vivo, **nyukomatic** lleva el nombre de
[Nyuk](https://github.com/akanyuk), el organizador del demoparty [Multimatograf](https://multimatograf.ru/)
que realizó el primer [evento](https://livecode.demozoo.org/serie/Multimatograf.html#mc) de programación en vivo para ZX Spectrum en Rusia.

## Características principales
- Aplicación nativa multiplataforma (disponible para Windows, Linux y macOS)
- Emulación rápida y precisa a nivel de ciclo del Pentagon 128, impulsada por la [biblioteca z80](https://github.com/kosarev/z80)
- Modos de red de emisor/receptor con retransmisión a través de [BonzomaticServer](https://github.com/alkama/BonzomaticServer)
- Puede recibir datos de transmisión por red disponibles desde el código Z80 leyendo puertos
  (requiere un [servidor Bonzomatic parcheado](https://github.com/alexanderk23/BonzomaticServer))
- Integra [SJASMPlus](https://github.com/z00m128/sjasmplus): el código se compila y ejecuta en tiempo real mientras escribes
- Resaltado de sintaxis para ensamblador Z80

## Compilar nyukomatic en Linux (Debian)

```bash 
sudo apt install -y libgl1-mesa-dev libfontconfig-dev libxmu-dev libxi-dev libgl-dev libxrandr-dev libxinerama-dev xorg-dev libglu1-mesa-dev libssl-dev
git clone https://github.com/alexanderk23/nyukomatic
cd nyukomatic
git submodule update --init --recursive
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

## Uso
**nyukomatic** puede operar en modo **emisor** o **receptor**.

- En el **modo emisor**, nyukomatic transmite las acciones del usuario (cambios de código, movimientos del cursor, etc.)
al servidor Bonzomatic, que a su vez las retransmite a todos los receptores que tengan la misma **sala** y
**nombre de usuario** especificados en su URL del servidor.

- En el **modo receptor**, la entrada se desactiva y **nyukomatic** muestra las acciones de un emisor con
una **sala** y un **nombre de usuario** coincidentes.

De forma predeterminada, el programa se inicia en **modo emisor sin conexión al servidor**.
Puedes experimentar con el código, pero no se transmitirá a ningún lado.

Para conectarte, abre la ventana de configuración, especifica la URL del servidor Bonzomatic en el formato:
```
ws://bonzomatic.example.com/room/nickname/
```
y haz clic en el botón "conectar".

## Transmisión de valores de puertos
**nyukomatic** admite la recepción de valores de puertos arbitrarios a través de la red desde utilidades externas.
Estos valores de puertos transmitidos por la red se pueden leer en código ensamblador Z80 utilizando instrucciones estándar.

Esto permite un comportamiento de código dinámico basado en condiciones externas. Por ejemplo, [enviar datos del analizador de espectro FFT](https://github.com/alexanderk23/nm-fft-example) desde una entrada de micrófono y modificar los parámetros de los efectos en tiempo real según la música (similar a las competiciones de shader showdown).

### Implementación  
1. **Configuración de la utilidad externa**:  
   - Conéctate a una sala del servidor Bonzomatic utilizando un **nombre de usuario vacío**.  
   - Envía un mensaje JSON que contenga un arreglo de pares puerto-valor:  
     - Puerto único: `[port_number, value]`  
     - Arreglo de valores: `[base_port_number, [value_1, value_2, ...]]`  

     ```jsonc
     {
         "ports": [
             [ 4783, 42 ], // Valor de un solo puerto
             [ 1019, [1, 2, 3] ] // Arreglo de valores (mapeados a puertos)
         ]
     }
     ```

2. **Lectura en código Z80**:  
   - **Puerto único**:  

     ```asm
     ld bc, port_number
     in a, (c)     ; Valor almacenado en el registro A
     ```
   - **Arreglo de puertos**:  
     Cada elemento del arreglo se asigna a `base_port_number - (index * 256)`.  
     Utiliza [`inir`](https://www.jnz.dk/z80/inir.html) para una lectura en bloque eficiente:  

     ```asm
     ld bc, base_port_number
     ld hl, dest_addr
     inir          ; Lee los valores secuencialmente en la memoria en HL
     ```

### Ejemplo  
Para esta entrada JSON:  
```jsonc
{
    "ports": [
        [ 4783, 42 ],
        [ 1019, [1, 2, 3] ]
    ]
}
```
**Valores de puertos resultantes** (`4783` = `0x12AF`, `1019` = `0x03FB`):
- `0x12AF` → `42`  
- `0x03FB` → `1`, `0x02FB` → `2`, `0x01FB` → `3`  

**Demostración en ensamblador**:  
```asm
; Leer un solo valor
ld bc, 0x12AF
in a, (c)       ; A = 42

; Leer arreglo de valores
ld bc, 0x03FB
ld hl, 0x4000
inir            ; Escribe 0x01, 0x02, 0x03 en las direcciones 0x4000-0x4002
```

## Agradecimientos
### Proyectos originales
- [Bazematic](https://github.com/gasman/bazematic): un entorno de programación en vivo para ZX Spectrum basado en navegador
- [Bonzomatic](https://github.com/Gargaj/Bonzomatic): una herramienta de programación de shaders en vivo y caballo de batalla de Shader Showdown

### Bibliotecas y otro software incluido
- [Boost Regex](https://github.com/boostorg/regex): proporciona soporte de expresiones regulares para C++
- [cxxopts](https://github.com/jarro2783/cxxopts): una biblioteca ligera de análisis de opciones para C++
- [Dear ImGui](https://github.com/ocornut/imgui): una biblioteca de interfaz gráfica de usuario para C++ sin componentes innecesarios
- [FreeType](https://github.com/freetype/freetype): una biblioteca de software de código abierto para renderizar fuentes
- [GLEW](https://github.com/nigels-com/glew): una biblioteca de código abierto y multiplataforma para cargar extensiones C/C++
- [GLFW](https://github.com/glfw/glfw): una biblioteca de código abierto y multiplataforma para el desarrollo de aplicaciones OpenGL, OpenGL ES y Vulkan
- [ImGuiColorTextEdit](https://github.com/BalazsJako/ImGuiColorTextEdit): un editor de texto con resaltado de sintaxis para ImGui
- [IXWebSocket](https://github.com/machinezone/IXWebSocket): una biblioteca C++ para el desarrollo de clientes y servidores WebSocket
- [json.cpp](https://github.com/jart/json.cpp): una biblioteca barroca de análisis y serialización JSON para C++
- [OpenSSL](https://github.com/openssl/openssl): una biblioteca de cifrado y TLS/SSL
- [rang](https://github.com/agauniyal/rang): colores para tu Terminal
- [SJASMPlus](https://github.com/z00m128/sjasmplus): un compilador cruzado de línea de comandos de lenguaje ensamblador para CPU Z80
- [SSE2NEON](https://github.com/DLTcollab/sse2neon): Un archivo de encabezado C/C++ que convierte intrínsecos Intel SSE a intrínsecos Arm/Aarch64 NEON
- [xxhash_cpp](https://github.com/RedSpah/xxhash_cpp): un port de la biblioteca xxHash a C++17
- [z80](https://github.com/kosarev/z80): un emulador Z80/i8080 rápido y flexible
- [zlib](https://github.com/madler/zlib): una biblioteca de compresión espectacular pero delicadamente discreta
