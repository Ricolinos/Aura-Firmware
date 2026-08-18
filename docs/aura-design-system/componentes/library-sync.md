# LibrarySync

🟢 Definido (D-293, 2026-08-17). Pantalla "Actualizando biblioteca…": la
reconstrucción de los índices de la biblioteca después de una
sincronización con Aura Studio (marcador `/.aura/sync-pending.json`) o
del disparo manual "Ajustes → Reconstruir biblioteca". Nombre interno en
código: `draw_library_sync()`/`handle_library_sync()`
(`apps/aura/aura_screens.c`); el avance vive en `apps/aura/aura_sync.c`
(máquina de estados) y el formato del marcador en
`docs/contracts/library-layout-v1.md` §4 (contrato con Aura Studio,
copia idéntica en su repositorio).

## Dónde vive

- **`FULL`** con `StatusBar` (título "Actualizando biblioteca…"). No es
  `SPLIT`: no hay panel derecho del que venir — aparece **sola**, al
  arrancar o al volver de la pantalla USB, encima de la raíz; y desde
  Ajustes reemplaza al aviso Sí/No de la fila "Reconstruir biblioteca"
  (se hace `pop` del aviso antes del `push`, así Menú vuelve a Ajustes).
- **Excepción deliberada al patrón de esperas del sistema** (cápsula
  flotante, `status-bar.md` §"estados de espera"): la cápsula existe para
  no tapar contenido que sigue siendo útil mientras algo carga. Aquí lo
  que se reconstruye ES la biblioteca — no hay contenido debajo que valga
  la pena mostrar, y el usuario necesita ver **qué sección** va y
  **cuánto falta**. Es la única pantalla completa de progreso del
  sistema; no es precedente para usarla en otras esperas.

## Cuándo aparece

| Momento | Condición |
|---|---|
| Arranque | Existe `/.aura/sync-pending.json` con alguna sección en `true` (o con error de versión/intentos, ver abajo). Con candado activo se empuja debajo del bloqueo y aparece al desbloquear |
| Al volver de la pantalla USB | Idem — es el único momento en que el firmware se entera de que Studio pudo escribir en el disco |
| Ajustes → Reconstruir biblioteca → Sí | Siempre (escribe el marcador con las tres secciones y arranca de inmediato) |

Se **cierra sola** al terminar bien (`pop`, sin pantalla de "listo": la
lista de Música ya refleja el resultado). Menú la cierra antes
(posponer, ver Comportamiento).

## Anatomía

| Elemento | Medida / token | Notas |
|---|---|---|
| Título | `StatusBar` v2, texto `AURA_STR_LIBRARY_UPDATING` | "Actualizando biblioteca…" |
| Filas de sección | 3 filas (Música, Videos, Fotos), `BODY`, desde `STATUSBAR_HEIGHT + XXL`, separación `TYPE_BODY + MD` | Nombre a la izquierda `TEXT_PRIMARY` (`TEXT_TERTIARY` si "Sin cambios"); estado a la derecha `TEXT_SECONDARY`, o **`ACCENT`** en la sección en curso |
| Estados de sección | "En espera" · "Leyendo…" · "Listo" · "Sin cambios" | `AURA_STR_LIBRARY_STATE_*`. Videos/Fotos pasan a "Listo" al instante (solo invalidan su listado); Música es la única que tarda |
| Barra de progreso (Música) | Debajo de las filas, `MD` de separación, alto `MD`, ancho `SCREEN_WIDTH - 2·XXL`, cápsula real (D-277) | `PROGRESS_TRACK` / `PROGRESS_FILL` — mismos tokens y misma construcción que el slider (`aura_widgets_draw_slider`). Dos tramos: escaneo de disco 0…200/256, indexado 200…256 |
| Detalle | `CAPTION`, `TEXT_SECONDARY`, bajo la barra | "N elementos leídos" durante el escaneo (conteo real, siempre avanza aunque la estimación de porcentaje sea 0 sin dircache) · "Indexando k/K" durante el commit |
| Pista inferior | `CAPTION`, `TEXT_TERTIARY`, centrada, a `XL` del borde | "Menú: seguir en el próximo encendido" (progreso) · "Menú: cerrar" (errores) |

Sin colores RGB propios: todo sale de `a26_palette` (regla siempre-on).

## Estados terminales (misma pantalla, cuerpo de texto envuelto)

| Estado | Texto | Qué pasa con el marcador |
|---|---|---|
| Versión mayor a la conocida | "Aura Studio dejó una actualización de biblioteca en un formato más nuevo (versión N) que este iPod no entiende. Actualiza Aura para aplicarla." | Se queda (lo procesará un firmware más nuevo). Se vuelve a mostrar en cada arranque/desconexión hasta entonces |
| Tres intentos fallidos | "No se pudo actualizar la biblioteca después de tres intentos. Puedes intentarlo de nuevo desde Ajustes → Reconstruir biblioteca." | Se queda con `attempts: 3`; el disparo manual escribe uno nuevo con `0` |
| Guardado pospuesto por tagcache | "La biblioteca ya se leyó completa; el índice se terminará de guardar la próxima vez que enciendas el iPod." | Se queda; el intento **no** cuenta; el hilo de tagcache confirma el índice al arrancar (sin diálogo de Rockbox: D-293 lo silenció) y la pasada siguiente lo cierra |

## Comportamiento

- **No cancelable**: SELECT y la rueda no hacen nada. Una base a medias es
  peor que esperar.
- **Posponible (Menú)**: cierra la pantalla, el marcador queda intacto y
  el trabajo ya encolado en tagcache se cierra en fondo (`aura_sync_tick()`
  sigue corriendo desde el loop principal): si termina bien, borra el
  marcador; si tagcache lo aborta, descarta el temporal a medias (la base
  vieja está intacta) y restaura el contador de intentos — posponer nunca
  cuenta como fallo. En el próximo arranque se retoma.
- **Sin animación propia**: la cadencia de refresco (`HZ/2`) la pone el
  loop principal mientras la pantalla está arriba, **fuera** de la puerta
  `lcd_active()` a propósito, igual que el escaneo inicial de la base — el
  trabajo debe seguir con la pantalla dormida.
- **Al terminar bien** (solo si se tocó Música): vacía
  `/.rockbox/aura/cfcache/` (la caché de carátulas se indexa por
  `album_seek`, que cambia con la base) y rearma la pasada de "primera vez"
  de `aura_music_db_ready()` (calificaciones de Studio + precache de
  carátulas), así que a continuación puede verse un instante "Preparando
  carátulas N/M" — comportamiento del precache, no de esta pantalla.

## Capturas

`docs/screenshots/library-sync/` — `00` el síntoma (archivos en `/Music`,
"Sin música todavía"), `01` esperando a tagcache, `02` progreso de Música
con la barra, `03`/`04` Canciones y Cover Flow después, `05` el aviso de
Ajustes, `06`/`07` los dos errores, `08`/`09` extremo a extremo con el
`LibrarySync` real de Aura Studio contra el `simdisk`: canciones
sincronizadas visibles tras la reconstrucción, y las letras `.lrc` en el
Modo 4.
