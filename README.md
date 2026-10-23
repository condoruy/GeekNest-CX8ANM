# GeekNest-CX8ANM

Firmware personalizado de **CX8ANM** para la radio **GeekNest V5A** (ESP32 + SI4732, placa *GeekNest Radio V1.0.12*).
Une dos aplicaciones en un solo firmware; se pasa de una a otra con la **tecla A apretada 2 segundos** o desde la página web.

| Modo | Base | Qué hace |
|---|---|---|
| Receptor RF | [tinyradio](https://github.com/lsongdev/tinyradio) (lsongdev, MIT) | LW/MW/SW/VHF/AIR, AM/FM/SSB, RDS, espectro, RTC, sensor |
| Radio Internet | Código propio CX8ANM + [ESP32-audioI2S 2.0.6](https://github.com/schreibfaul1/ESP32-audioI2S) | MP3/AAC por WiFi, hasta 40 emisoras, NTP → RTC |

**Página web** en `http://cx8anm.local`, en los dos modos:
- Radio Internet: qué suena, emisoras, volumen.
- Receptor RF: frecuencia, banda, modo, señal, RDS y favoritos.
- Los dos: WiFi, zona horaria y cambio de modo.

**Actualización por WiFi desde este repositorio:** la radio lee [`flasheo/ota.json`](flasheo/ota.json). Si hay una versión más nueva, baja `CX8ANM-GeekNestV5A-app.bin`, comprueba su SHA-256 y se reinicia. También se puede subir el `.bin` a mano desde la web.

## Instalar (por cable, la primera vez)

1. Descargar el repositorio: **Code → Download ZIP**, y descomprimirlo.
2. Abrir la carpeta `flasheo/`.
3. Ejecutar `1_RESPALDAR_firmware_original.bat` y después `2_INSTALAR_CX8ANM.bat`.

Sin Python: con Chrome o Edge en https://espressif.github.io/esptool-js/ , grabar `CX8ANM-GeekNestV5A-completo.bin` en la dirección `0x0`.

> **Si tenías la v1.0.0:** la v1.1.0 cambia la tabla de particiones, así que hay que grabarla por cable **una vez**. Las versiones siguientes ya se instalan por WiFi.

La guía de uso, los controles y la configuración WiFi están en [`flasheo/LEEME.txt`](flasheo/LEEME.txt).

## Publicar una versión nueva

```
tools/make_release.sh 1.2.0 "Qué cambió"
git add -A && git commit -m "v1.2.0" && git push
```

El script compila y deja en `flasheo/` la imagen completa, la imagen `-app.bin` y el `ota.json` con el tamaño y el SHA-256. Antes de compilar hay que subir `CX8_FIRMWARE_VERSION` en `src/ui/splash.h`. Apenas se hace el push, las radios ven la versión nueva.

## Estructura

```
GeekNestCX8ANM/
  GeekNestCX8ANM.ino        vacío a propósito (el punto de entrada es src/main.cpp)
  src/main.cpp              elige la aplicación según NVS "cx8anm/bootmode"
  src/app/boot_mode.*       guarda el modo y reinicia
  src/app/receiver.c        tinyradio + splash + tecla A larga + control remoto
  src/app/rf_remote.h       API del receptor para la web
  src/boards/v5a/           placa V5A; board_begin_net() y GPIO2 (habilitación DAC)
  src/ui/splash.*           pantalla de inicio y versión del firmware
  src/ui/efont_latin.*      efontCN_24_b reducido a latín (mismos glyphs, 9 KB)
  src/netradio/
    netradio.cpp            Radio Internet: WiFi, entradas, reloj, actualizaciones
    rfweb.cpp               WiFi y web dentro del modo RF
    player.cpp              tarea FreeRTOS del decodificador
    screen.cpp              interfaz 320x240 (fuentes Latin-1 propias)
    webui.cpp               página web, API JSON, portal cautivo, subida de .bin
    updater.cpp             OTA desde GitHub (raíces CA embebidas, SHA-256)
    config.cpp              ajustes, emisoras y favoritos en NVS "cx8net"
  tools/subset_efont.py     genera src/ui/efont_latin.c
  tools/make_gfxfont.py     genera src/netradio/fonts_cx8.*
libraries/
  ESP32-audioI2S 2.0.6      PARCHE: psramInit() → psramFound()
  (LovyanGFX 1.2.7: instalar aparte)
flasheo/                    imágenes, ota.json, scripts y LEEME
```

## Decisiones de hardware

- **Sin PSRAM.** En la V5A el pin GPIO12 (tecla C) es también el strap de VDD_SDIO, e inicializar la PSRAM con el strap en 1,8 V cuelga la placa. ESP32-audioI2S llamaba a `psramInit()`, por eso el parche. Consecuencia: no hay FLAC ni HLS (.m3u8).
- **Audio de Internet** por I2S en BCLK=25, LRCK=27, DOUT=26 y con GPIO2 en alto, igual que el yoRadio del fabricante. La llave del parlante tiene que estar en **Digital**.
- En modo Internet el **XL9555 vuelve a su estado de arranque** (todos los pines como entradas), que es el estado en que el yoRadio del fabricante reproduce audio.
- **Tabla `min_spiffs`:** dos slots OTA de 1,875 MB. Para que el firmware (≈1,4 MB) quepa, se reemplazaron las fuentes CJK de 1,1 MB por subconjuntos latinos.
- **WiFi en modo RF** solo si queda memoria después de los framebuffers de tinyradio, con *modem sleep* para generar menos ruido. Se puede apagar desde la web. Las actualizaciones pedidas desde RF se instalan en modo Internet, porque TLS necesita la RAM que ocupan los framebuffers.

## Compilar

Se usa `arduino-cli` con el core ESP32 **2.0.17** (el mismo de `espressif32@6.12.0` en PlatformIO):

```
arduino-cli lib install "LovyanGFX@1.2.7"
./build.sh
```

## Créditos y licencia

- tinyradio © lsongdev, MIT (`LICENSE-tinyradio-MIT`).
- ESP32-audioI2S © schreibfaul1, GPL-3.0 (`LICENSE-GPL-3.0`).
- LovyanGFX © lovyan03, FreeBSD. Fuentes efont (BSD) y DejaVu Sans (Bitstream Vera).
- Al incluir ESP32-audioI2S, el firmware combinado se distribuye bajo **GPL-3.0**.

73 de CX8ANM.
