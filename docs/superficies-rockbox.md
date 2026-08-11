# Checklist de superficies de Rockbox (Fase 25)

Estado real de cada fila del inventario de `PLAN-UX.md` §5 tras las Fases
12–24. Estrategias del plan original: **R** = reconfigurar (solo settings),
**P** = parchear/reemplazar con UI Aura, **N** = neutralizar/documentar.

Leyenda de estado: ✅ Resuelto y verificado · 🟡 Resuelto con matices/no
verificado visualmente · ⏸️ Fuera de alcance, documentado · 🔒 Deliberadamente
intacto (decisión de diseño, no una omisión).

## Nivel 1 — se ven seguro en uso normal

| Superficie | Estrategia | Fase | Estado |
|---|---|---|---|
| Logo Rockbox al encender | P | 12 | ✅ D-052: logo Aura propio, sin texto de versión |
| Texto del bootloader | P | 22 | ✅ D-064: camino feliz silencioso; ver nota de "pantallas fatales" abajo |
| Pantalla USB | P | 19 | 🟡 D-061: logo centrado + wordmark propio, bug real de solo-carga corregido — pero **no** se construyó la pantalla completa "Conectado" con selector Preguntar/Almacenamiento/Solo carga de §3.9 (queda para una fase futura con hardware para probarla) |
| "Loading..." al desconectar USB | P | 19 | 🟡 Cubierto transitivamente por el re-skin genérico de `splash()` (D-055) si `playlist_resume()` pasa por ahí — no verificado de forma dedicada |
| "Shutting down..." | R+P | 12→19 | ✅ D-051 (`show_shutdown_message=false`) + D-055/D-056 (splash re-skineado cubre los mensajes de batería al apagar) |
| Menú "MPEG Player" al abrir un video | P | 20 | ✅ D-062: `mpeg_start_menu()` entra directo, ~340 líneas de UI muerta removidas |
| OSD del video | P | 20 | ✅ D-062: paleta Aura, keymap ya estaba alineado |
| Backdrop Cabbie v2 | R | 12 | ✅ D-051: `backdrop_file="-"` |
| Statusbar clásica de Rockbox | R | 12 | ✅ D-051: `STATUSBAR_OFF` |

## Nivel 2 — muy probables

| Superficie | Estrategia | Fase | Estado |
|---|---|---|---|
| "Scanning disk..." / "Committing database [x/y]" | P | 14 | 🟡 D-055: el mecanismo (re-skin genérico de `splash_internal()`) cubre estos mensajes en principio, pero **no se logró verificar visualmente** — "Scanning disk..." no existe en el simulador (`HAVE_DIRCACHE` deshabilitado ahí) y "Committing database" se completó demasiado rápido para capturarlo con la biblioteca de prueba. D-055 ya dejó esto pendiente de hardware real o una biblioteca más grande; sigue pendiente tras esta fase, sin acceso a hardware físico en esta sesión |
| Splashes de batería | P | 14→19 | ✅ D-055/D-056: traducidos a wording Aura |
| "Database is not ready" cancela el apagado | P | 19 | 🟡 Cubierto por el mismo re-skin genérico (D-055); no probado con un escenario dedicado |
| "Loading..." al abrir video con disco dormido | P | 14 | 🟡 Cubierto por el mismo re-skin genérico (D-055); no probado con un escenario dedicado |
| Modo solo-carga silencioso con botón pulsado | N | 19 | ✅ D-061: `USBPOWER_BTN_IGNORE` corregido — bug real de nivel 2 que en el plan original se pensaba documentar nomás, terminó siendo un fix de código |
| HID sigue enumerándose ante la Mac | R (crítico) | 12 | ✅ D-051: `usb_set_hid(false)` real, no solo el flag de UI |
| Menús Settings/Display/Audio de mpegplayer | P | 20 | ✅ D-062: eliminados junto con el menú de inicio |

## Niveles 3–4 — raros o de fallo grave

| Superficie | Estrategia | Fase | Estado |
|---|---|---|---|
| Voz (.voice, talk clips, habla al apagar) | R | 12 | ✅ D-051: `talk_menu=false`, sin voces empaquetadas |
| Boot con HOLD ⇒ "Cleared" + reset de settings | R | 12 | ✅ D-051: `clear_settings_on_hold=false` |
| Errores de playlist | P | 14 | 🟡 Cubierto por el re-skin genérico (D-055); no probado con un escenario dedicado |
| Errores de plugin | P | 14 | 🟡 Cubierto por el re-skin genérico (D-055) — **la mitigación adicional que el plan asumía (D-045, empaquetar `.rockbox/` completo desde Studio) sigue siendo un gap conocido y no resuelto**, así que el escenario que dispara este error (plugin faltante) sigue siendo alcanzable en instalaciones reales hasta que Studio empaquete el árbol completo |
| `*PANIC*` + backtrace, errores de ATA/partición | P | 19 | 🔒 D-061: **deliberadamente intacto**, no una omisión — la pantalla de pánico no puede depender de código/temas de Aura que podrían ser la causa del propio fallo (evita "un fallo en el fallo") |
| Pantallas fatales del bootloader | P | 22 | 🟡 D-064: solo se tocó la **visibilidad** del camino feliz (pantalla negra hasta el logo Aura); los mensajes de `fatal_error()`/`battery_trap()` (batería crítica, "Hold MENU+SELECT to reboot") siguen mostrando el texto original de Rockbox sin re-skin de wording/paleta — desviación deliberada del plan original por el riesgo de tocar el bootloader (D-064 explícito) |
| Teclado virtual, yes/no, browsers, quickscreen del core | N | 12 (doc) | ✅ Inalcanzables por construcción — la navegación propia de Aura (`aura_screens.c`) nunca invoca estas pantallas del core; no se agregó ningún llamador nuevo en ninguna fase |

## Extras de plataforma

| Ítem | Fase | Estado |
|---|---|---|
| Fundido de retroiluminación | 19 | ✅ D-061: `CONFIG_BACKLIGHT_FADING BACKLIGHT_FADING_SW_HW_REG`, motor real de Rockbox reutilizado |
| Sleep timer del core expuesto en Ajustes | 18 | ✅ D-060: pantalla "Temporiz. reposo" |
| `battery_capacity` (550 mAh default) | 12 | 🟡 D-051: revisado y dejado sin cambios **a propósito** — no hay forma de verificar el valor real de la batería física sin el dispositivo delante; inventar un número falsearía el ícono en vez de corregirlo. Sigue pendiente de una sesión con hardware |
| `poweroff`, `backlight_timeout(_plugged)`, `volume_limit` | 12 | ✅ D-051: revisados y ajustados a defaults de producto |
| `keyclick` (software) | 18 | ✅ D-060: pantalla "Clicker" |
| `disk_spindown`, `keyclick_hardware` | 12 | 🟡 D-051: revisados y dejados sin cambios a propósito — el plan los marca como "revisar con criterio de producto/datos de uso real", no como un valor ya decidido; no hubo datos de uso real disponibles en esta sesión |

## Resumen

De 27 filas del inventario original: **17 resueltas y verificadas (✅)**,
**9 resueltas por mecanismo pero con verificación visual pendiente o con un
gap conocido en una dependencia (🟡)**, **1 deliberadamente intacta por
diseño (🔒)**. Cero filas quedaron simplemente sin tocar y sin explicación:
toda fila 🟡 tiene una razón concreta (limitación del simulador, falta de
hardware físico, o un gap ya documentado en otra decisión) citada en
`DECISIONS.md`.
