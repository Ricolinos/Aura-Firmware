# Guía de medición de rendimiento en hardware — morph del Modo 4 (Letras)

Cómo medir, en el iPod real, los milisegundos por cuadro de la transición
de entrada/salida del modo Letras del reproductor (el "morph"), para
decidir con datos si hacen falta las optimizaciones que quedaron
pendientes del plan de D-315. Contexto: D-300 (instrumentación),
D-315 (perfil estático), D-361 (ritmo por reloj + caché de íconos +
LUT del vidrio).

## Por qué hace falta un build especial

La instrumentación usa `DEBUGF`, que en un build normal es un no-op.
Pero incluso en un build DEBUG, `DEBUGF` **no tiene salida legible en
el iPod 6g**: `debug()` del target s5l8702 no escribe a ningún lado y
Aura no expone el menú de depuración de Rockbox. Por eso, en builds
DEBUG las líneas de medición se escriben además a un archivo de texto
plano en el disco del iPod (`/.aura/morph-log.txt`), que se lee
conectando el iPod por USB — sin menús especiales ni consola serial.

## 1. Conseguir el build DEBUG

Desde la raíz de este repositorio (toolchain en `firmware/toolchain/bin`):

```sh
mkdir -p firmware/build-ipod6g-debug && cd firmware/build-ipod6g-debug
PATH="$PWD/../toolchain/bin:$PATH" ../rockbox/tools/configure --target=ipod6g --type=ad
PATH="$PWD/../toolchain/bin:$PATH" make -j"$(sysctl -n hw.ncpu)"
PATH="$PWD/../toolchain/bin:$PATH" make zip
```

(`--type=ad` = build avanzado con (D)EBUG. Ojo: `--type=d` a secas es
la herramienta de base de datos, **no** un build debug.)

El resultado es `firmware/build-ipod6g-debug/rockbox.zip`.

⚠️ **Es un build de desarrollo**: más lento en general y no candidato a
release. Instálalo solo para medir y vuelve al build normal al terminar
(`firmware/build-ipod6g/`, mismo procedimiento de instalación).

## 2. Instalarlo en el iPod

1. Arranca el iPod en **modo Bootloader USB** (SELECT + RIGHT al
   encender — ver `guia-flasheo-restauracion.md`) con el cable conectado.
2. Con el volumen FAT32 del iPod montado en la Mac, descomprime
   `rockbox.zip` en la **raíz** del iPod (sobrescribe `.rockbox/`).
3. Expulsa el volumen y reinicia el iPod.

## 3. Medir

1. Reproduce una canción **que tenga letras** (`.lrc`) — el modo Letras
   solo existe si la pista las trae.
2. En "Ahora suena", entra y sal del modo Letras con Select **unas 5-10
   veces** (cada entrada corre el morph de ida; cada salida, el de
   regreso). Hazlo con **Animaciones = Todas**, que es el nivel que se
   está evaluando.
3. Deja la reproducción corriendo o pausada, da igual: el log ya quedó
   escrito en disco.

Notas:
- El archivo se **trunca una vez por arranque**: cada reinicio del iPod
  empieza una sesión de medición limpia. Si quieres comparar "antes y
  después" de un cambio, anota los números (o copia el archivo) antes
  de reiniciar con otro build.
- La escritura del log corre **después** de terminar cada fase medida,
  fuera de la ventana cronometrada — no contamina la medición.

## 4. Leer los resultados

1. Conecta el iPod en modo disco (cualquiera de los modos USB de la
   tabla de arranque).
2. Abre `/.aura/morph-log.txt`. Cada morph deja dos líneas:

```
mode4_morph barrido principal: 33 frames en 34 ticks
mode4_morph fase extra (sombra): 16 frames en 17 ticks
```

`HZ = 100`, así que **1 tick = 10 ms**.

## 5. Interpretar

El presupuesto de pared del morph (D-361) es **33 ticks** (330 ms) para
el barrido principal y **16 ticks** (~160 ms) para la fase de sombra,
más el costo de render del último cuadro de cada fase.

| Lo que ves | Significado | Qué sigue |
|---|---|---|
| Ticks ≈ 33/16 y frames ≈ 25-33 | Ritmo perfecto: un cuadro por slot de 60fps, render sobrado | Nada — el morph ya va como la spec |
| Ticks ≈ 33-40, frames ~15-25 | Duración correcta; el render cuesta 1-2 ticks por cuadro y se saltan cuadros intermedios | Aceptable; las optimizaciones pendientes darían más cuadros (más suavidad), no más velocidad |
| Ticks ≈ 33-45, frames < 12 | Duración casi correcta pero pocos cuadros: render muy caro (≥3 ticks/cuadro) | Vale la pena ejecutar lo pendiente del plan D-315: StatusBar compuesta una vez, prerender de estados finales, LUT de la sombra SDF, remaquetado invariante de letras |
| Ticks ≫ 45 | Algo escapa al presupuesto (no debería pasar con el ritmo por reloj) | Reportar con el log completo: es un bug del ritmo, no de render |

El dato clave a reportar de vuelta es la pareja **frames / ticks** de
una entrada al modo Letras (la línea "barrido principal"), con el nivel
de Animaciones usado.

## 6. Volver al build normal

Repite el paso 2 con `firmware/build-ipod6g/rockbox.zip` (o reinstala
desde Aura Studio). Puedes borrar `/.aura/morph-log.txt` del iPod
cuando quieras — se regenera solo en el próximo arranque de un build
DEBUG.
