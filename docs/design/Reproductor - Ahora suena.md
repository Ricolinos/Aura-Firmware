Depende de `Apple2026 — Sistema de diseño (v2).md` (tokens, tipografía, movimiento) y de `Reglas de comportamiento - iPod Classic original (2008).md` (continuidad de Cover Flow → Ahora suena). Este documento no redefine nada de ahí, solo lo aplica a esta pantalla.

# Reproductor — Ahora suena

**Punto de partida:** la maqueta del usuario (tarjeta "Right Here, Right Now" / Fatboy Slim) define la composición — tarjeta limpia, tipografía grande, fila de 5 íconos de modo. El firmware original ("The Weekenders" / The Hold Steady) aporta dos cosas que la maqueta no tenía: el **reflejo** bajo la carátula y la continuidad animada al entrar desde Cover Flow. Este documento fusiona ambas usando solo tokens de `a26_palette`.

---

## 1. Por qué el reflejo no es un anti-patrón aquí

El Principio 1 (`Apple2026 v2`) prohíbe marcos, biseles y cromo — pero el reflejo no es cromo decorativo: Liquid Glass, el material actual de Apple, se define precisamente por **reflejar y refractar lo que tiene detrás** (§0 del documento base). Un reflejo bajo la carátula no es un recuerdo de 2007, es coherente con el lenguaje de superficie de 2026 — mismo principio, sin necesidad de blur en tiempo real. Se implementa como imagen estática precalculada, no como efecto de composición: cabe en el presupuesto del dispositivo.

## 2. Tipo de pantalla y layout

`FULL`, sin barra de estado dividida — hereda la transición modal de Cover Flow (`Reglas de comportamiento…`, sección 2.6): al elegir una canción, el álbum gira, se ubica a la izquierda con su reflejo, y el resto de la interfaz aparece alrededor.

| Zona | Posición | Contenido |
|---|---|---|
| Barra de estado | y=0–20 | Título centrado "Ahora suena", batería derecha, candado/play junto a la batería |
| Carátula + reflejo | izquierda, y=28 | Ver §3 |
| Bloque de texto | derecha de la carátula | Título (Semibold), artista (Secondary, Regular), rating (§4) |
| Fila de modos | debajo del bloque de texto, alineada a la derecha | 5 íconos, ver §5 |
| Pastilla de progreso | y=alto−34 | Reutiliza §5.2 del documento base, con tiempos a los lados |
| Transporte | y=alto−14 | Repetir (izq.) / Play-Pausa-FF (centro) / Aleatorio (der.) |

## 3. Carátula y reflejo

- Carátula: cuadrado, radio de esquina 8 px (nivel 2 de la escala concéntrica, §5.4 del doc base).
- Reflejo: duplicado volteado verticalmente, alineado justo debajo con 2 px de separación.
  - Alto del reflejo: 35% del alto de la carátula.
  - Máscara de opacidad: 35% en el borde superior del reflejo → 0% en el borde inferior, en degradado por tramos (mismo enfoque que la sombra del arte, §5.4 del doc base — nunca un gradiente calculado en tiempo real).
  - El reflejo **cruza-desvanece** junto con la carátula en cada cambio de pista (fundido de 330 ms, ver Movimiento del doc base) — nunca aparece "de golpe" ni se recalcula con retraso respecto a la carátula.

## 4. Rating (estrellas)

Control de valor, no ícono de menú — por eso **sí** usa `star.fill` para las estrellas activas y `star` (lineal) para las vacías; la regla "nunca `.fill`" del documento base aplica solo a la tira de iconografía de menú, no a controles de cantidad como este.
- Color por defecto (pantalla en reposo, fuera del modo Estrellas): `TEXT_PRIMARY` para llenas, `SHELL_RAIL` para vacías — es contenido, no un estado activo.
- Color en modo Estrellas activo (§5.5): `ACCENT` para llenas — aquí sí se "gana" el acento, porque el control está siendo editado con la rueda en ese momento (Principio 2).

## 5. Fila de modos (rueda multipropósito)

Extiende la regla ya existente "SELECT en Reproduciendo cicla los modos de la rueda" (`Apple2026 v2`, Interacción de la rueda) de 3 a 5 modos. Un ícono a la vez está activo; los otros 4 quedan en `TEXT_TERTIARY`. Presionar Select avanza al siguiente modo.

**Transición entre modos:** el ícono que se activa se anima con el **resorte corto con sobrepaso** ya adoptado para la pastilla de selección (`Apple2026 v2`, Movimiento) — mismo lenguaje de motion en toda la capa de controles, sin inventar una curva nueva. El ícono que se desactiva vuelve a `TEXT_TERTIARY` con fundido lineal simple.

| # | Ícono (SF Symbol) | Modo | Qué hace la rueda | Feedback visual |
|---|---|---|---|---|
| 5.1 | `speaker.wave.2` | Volumen | Sube/baja el volumen | Pastilla de volumen temporal sobre la barra de progreso (mismo tratamiento que §5.2 del doc base, cápsula `SHELL_BG`/`SHELL_RAIL`) |
| 5.2 | `arrow.left.and.right` | Avance/retroceso | Escrubea la barra de progreso | El relleno de la pastilla de progreso seguidor en vivo; tiempos a los lados se actualizan sin parpadeo |
| 5.3 | `list.bullet` | Añadir a lista | Recorre las listas de reproducción existentes | Panel flotante corto con el nombre de la lista resaltada (pastilla `SELECTION_FILL`, igual que una fila de lista); Select confirma añadir |
| 5.4 | `text.bubble` | Letra | Select abre la vista de letra a pantalla completa (§6) | El ícono no reacciona a la rueda directamente, solo a Select |
| 5.5 | `star` / `star.fill` | Estrellas | Sube/baja el rating 0–5 | Estrellas del bloque de texto (§4) se llenan una a una con el mismo resorte corto; al soltar, la canción se añade a la lista automática "N estrellas" correspondiente |

**Nota de implementación — modo 5.3:** el panel flotante de listas no debe convertirse en pantalla completa (viola el Principio 3, "la carga convive, no interrumpe"); es una pastilla que aparece sobre el reproductor y desaparece con fundido lineal al confirmar o al cambiar de modo.

**Nota de implementación — modo 5.5 (listas automáticas):** son 5 listas inertes generadas por rating (1 a 5 estrellas), recalculadas cuando cambia el rating de cualquier canción — mismo patrón mental que "Canciones aleat." del firmware original, no hace falta una pantalla de gestión nueva, solo aparecen como playlists más en Música → Listas repr.

## 6. Vista de letra

Al entrar (Select en modo 5.4), el reproductor **no se reemplaza**: se comprime a un panel angosto y aparece un panel de letra a pantalla completa a su derecha — reutiliza el patrón `SPLIT` del documento de comportamiento, con los roles invertidos (el contenido grande va a la derecha, el "control" queda a la izquierda).

- Panel izquierdo (comprimido): carátula en miniatura, insignia de calidad opcional (p. ej. "Lossless", tipografía `TEXT_TERTIARY`, esquina superior), la misma fila de 5 íconos en columna, ícono de reproducción.
- Panel derecho: letra sincronizada, línea activa en `TEXT_PRIMARY` y tamaño mayor, líneas ya pasadas y futuras en `TEXT_TERTIARY`, tamaño base. Auto-scroll con la cadencia de paneo lento ya definida (HZ/6–HZ/8) — nunca un salto entre líneas.
- Transición de entrada/salida: igual que 2.4 (`FULL-CARRY`) del documento de comportamiento — el panel de reproductor no se va, se encoge; la letra entra desde la derecha.
- Menú o Select en modo 5.4 otra vez: cierra la letra, el panel de reproductor recupera su ancho completo.
