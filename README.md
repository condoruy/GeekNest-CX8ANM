# GeekNest-CX8ANM

Firmware personalizado de **CX8ANM** para la radio **GeekNest V5A** (ESP32 + SI4732, placa *GeekNest Radio V1.0.12*).
Une dos aplicaciones en un solo binario y se pasa de una a otra manteniendo la **tecla A 2 segundos**:

| Modo | Base | Qué hace |
|---|---|---|
| Receptor RF | [tinyradio](https://github.com/lsongdev/tinyradio) (lsongdev, MIT) | LW/MW/SW/VHF/AIR, AM/FM/SSB, RDS, espectro, RTC, sensor |
| Radio Internet | Código propio CX8ANM + [ESP32-audioI2S 2.0.6](https://github.com/schreibfaul1/ESP32-audioI2S) | MP3/AAC por WiFi, lista de 40 emisoras, página web de control, NTP → RTC |

## Instalar

1. Descargar el repositorio: botón verde **Code → Download ZIP**, y descomprimirlo.
2. Abrir la carpeta `flasheo/`: ahí están el firmware `CX8ANM-GeekNestV5A-v1.0.0-completo.bin` y los scripts.
3. Ejecutar `1_RESPALDAR_firmware_original.bat` y después `2_INSTALAR_CX8ANM.bat`.

Sin Python: con Chrome o Edge en https://espressif.github.io/esptool-js/ , grabar el `.bin` en la dirección `0x0`.

Guía completa de uso, controles y configuración WiFi: [`flasheo/LEEME.txt`](flasheo/LEEME.txt).

## Estructura

```
GeekNestCX8ANM/
  GeekNestCX8ANM.ino        vacío a propósito (el punto de entrada es src/main.cpp)
  src/main.cpp              elige la aplicación según NVS "cx8anm/bootmode"
  src/app/boot_mode.*       guarda el modo y reinicia
  src/app/receiver.c        tinyradio + splash CX8ANM + tecla A larga → Internet
  src/boards/v5a/           placa V5A; agrega board_begin_net() y GPIO2 (habilitación DAC)
  src/ui/splash.*           pantalla de inicio CX8ANM
  src/netradio/             radio por Internet
    netradio.cpp            WiFi, entradas, reloj, acciones
    player.cpp              tarea FreeRTOS que maneja el decodificador
    screen.cpp              interfaz 320x240 (LovyanGFX, sin framebuffer)
    webui.cpp               página web + API JSON + portal cautivo
    config.cpp              ajustes y emisoras en NVS "cx8net"
libraries/
  (LovyanGFX 1.2.7: instalar aparte, ver abajo)
  ESP32-audioI2S 2.0.6      PARCHE: psramInit() → psramFound()
```

## Decisiones de hardware

- **Sin PSRAM.** En la V5A el pin GPIO12 (tecla C) es también el strap de VDD_SDIO; inicializar la PSRAM con el strap en 1,8 V cuelga la placa. ESP32-audioI2S llamaba a `psramInit()`, por eso el parche. Consecuencia: no hay FLAC ni HLS (.m3u8).
- **Audio de Internet** por I2S en BCLK=25, LRCK=27, DOUT=26 y GPIO2 en alto, igual que el yoRadio del fabricante. La llave del parlante tiene que estar en **Digital**.
- En modo Internet el **XL9555 vuelve a su estado de arranque** (todos los pines como entradas), que es el estado en que el yoRadio del fabricante reproduce audio.
- Un solo binario de 2,4 MB con la tabla `huge_app` (3 MB de app, sin OTA). El modo RF arrastra unos 30 KB de RAM estática de la pila WiFi; si el framebuffer de tinyradio no entra, tinyradio dibuja directo en pantalla (funciona, con algo de parpadeo).

## Compilar

Se usa `arduino-cli` con el core ESP32 **2.0.17** (el mismo que usa `espressif32@6.12.0` de PlatformIO):

```
arduino-cli lib install "LovyanGFX@1.2.7"   # o clonar github.com/lovyan03/LovyanGFX tag 1.2.7 en libraries/
./build.sh
```

El archivo `.ino` está vacío porque el preprocesador de Arduino no debe generar prototipos para el código en `src/`.

## Créditos y licencia

- tinyradio © lsongdev, MIT (`LICENSE-tinyradio-MIT`).
- ESP32-audioI2S © schreibfaul1, GPL-3.0 (`LICENSE-GPL-3.0`).
- LovyanGFX © lovyan03, FreeBSD.
- Al incluir ESP32-audioI2S, el firmware combinado se distribuye bajo **GPL-3.0**.

73 de CX8ANM.
