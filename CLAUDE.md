## Sistema de diseño — Apple2026 / Aura

Antes de crear, modificar o revisar cualquier pantalla, animación o componente visual, consulta la skill `apple2026-design-system`. No asumas valores de color, tipografía, geometría o movimiento: la **fuente de verdad viva es `docs/aura-design-system/`** (mapa en su `00-INDICE.md`); `docs/design/` es la base histórica original — consúltala solo para lo que el sistema vivo aún no cubra, y ante cualquier conflicto **gana `docs/aura-design-system/`**.

Reglas que aplican siempre, sin excepción, trabajes en lo que trabajes:
- Ningún color RGB hardcodeado en C — todo sale de `a26_palette` (`firmware/rockbox/apps/aura/apple2026_shell.h`).
- Cero cromo de Rockbox visible en cualquier estado (logotipos, nombres crudos de directorio, jerga técnica).
- Todo texto de cara al usuario en español natural, añadido al final de ambos `.lang`.
- Toda animación respeta la puerta `lcd_active()`.
- El "vidrio" (translucidez) vive solo en la capa de controles (barra de estado, pastillas) — nunca en contenido.
- Todo texto de cara al usuario y toda documentación en español de México, natural, sin voseo.

Si encuentras algo que no respeta estas reglas o las de `docs/design/`, ajústalo sin preguntar — salvo que el propio documento marque la decisión como abierta.

## Este repositorio es solo el firmware

Aura Studio (app macOS) vive en un repositorio aparte. El contrato entre ambos está en `CONTRATO-firmware-studio.md` — léelo antes de tocar cualquier cosa que Studio consuma.

- Ningún script, `Makefile` ni generador de este repo asume la existencia de un checkout hermano de Aura Studio (`../studio` o similar) ni escribe fuera de este árbol. Lo que Studio necesita (binarios, `AuraPalette.swift`, `MODIFICATIONS.md`) sale por **GitHub Release**, nunca por ruta relativa — es la clase de acoplamiento que ya causó un bug real (`generate.py` creaba un `studio/` huérfano en checkouts sin Studio al lado).
- GPL v2: todo archivo `.c`/`.h` nuevo en `apps/aura/` lleva la cabecera de licencia; todo cambio a un archivo de Rockbox fuera de `apps/aura/` se registra en `MODIFICATIONS.md` en la misma pasada.
- Sin material de Apple en el árbol ni en `firmware/dist/`: nada de SF Pro, SF Symbols, ni sus derivados (`.fnt`, BMP horneados). El tema Apple es un constructor local fuera de este repo (`PLAN-theme-system.md`), nunca parte del build público.
- `DECISIONS-ARCHIVE.md` es histórico, de solo lectura (D-001…D-285). Las decisiones nuevas van a `DECISIONS.md` con numeración `D-286+`; una referencia a una decisión de Aura Studio se escribe `ST-NNN`.

## Sistema de temas (D-289)

Ver `docs/aura-design-system/sistema/05-temas.md` (diseño) y `CONTRATO-formato-tema.md` (formato exacto, copia idéntica en Aura Studio) antes de tocar rutas de fuentes/íconos/paleta.

- Ninguna ruta de fuente, ícono, fondo o tile se construye a mano fuera de `aura_style.c` (`aura_style_read_icon_bmp()` para íconos; las 14 rutas de fuente solo se resuelven en `build_default_font_paths()`/`build_style_font_paths()`). Un sitio nuevo que dibuje algo por tema pasa por ahí, nunca por un `snprintf` propio con `ICON_DIR`/`FONT_DIR` a secas.
- `aura_settings.theme` (claro/oscuro) y el sistema de temas (paquete de fuentes+íconos+paleta, "Estilo" en la UI) son conceptos **distintos y ortogonales** — no los mezcles ni en código ni en texto de cara al usuario.
- Un tema nunca puede dejar el dispositivo sin UI legible: cualquier cambio a la lógica de carga/activación de `aura_style.c` tiene que preservar el fallback al default (§ "Fallback de seguridad" del documento de diseño) — es un requisito de seguridad, no una comodidad.

## Biblioteca y sincronización (D-293)

Ver `docs/contracts/library-layout-v1.md` (contrato con Aura Studio, copia idéntica allá — estructura de directorios, carátulas, `.lrc`, marcador `/.aura/sync-pending.json`) y `docs/aura-design-system/componentes/library-sync.md` (pantalla) antes de tocar rutas de biblioteca, tagcache o el arranque.

- El firmware **nunca** se entera de una sincronización más que por el marcador `/.aura/sync-pending.json` (Studio lo escribe; `aura_sync.c` lo lee al arrancar y al volver de la pantalla USB — únicos dos momentos en que recupera el disco). Un archivo nuevo que Studio deje para el firmware va al contrato **antes** de leerlo aquí; su versión de esquema sube con el campo `version` del marcador y con la clave `sync_marker_supported` de `aura.cfg`.
- Toda reconstrucción de la base de datos de música pasa por `aura_sync.c` (`Q_UPDATE`/`Q_REBUILD` de tagcache, contador de intentos dentro del marcador, borrado del marcador solo al terminar bien). No se llama a `tagcache_rebuild()`/`tagcache_update()` desde ninguna otra pantalla nueva; `aura_music_db_ready()` conserva solo su disparo de "sin base al arrancar" y **cede** mientras `aura_sync_job_active()`.
- La pantalla "Actualizando biblioteca…" es la única pantalla completa de progreso del sistema (excepción documentada a la cápsula de espera) — no es precedente para otras esperas. No cancelable, posponible con Menú.
- Cambios en `apps/tagcache.c`/`.h` se registran en `MODIFICATIONS.md` en la misma pasada (GPL v2 §2a) y se marcan `Aura (D-NNN)` en el código.

