Se mantienen los estilos de las pantallas y algunos de los menús del firmware original, pero se actualiza y se ordena de otra forma.

# Apple2026 — Sistema de diseño (v2)

**La premisa:** este proyecto es un concepto: *el software que Apple enviaría con un iPod Classic en 2026*. No es "Rockbox con otro tema" ni el iPod de 2007 restaurado: es el lenguaje visual actual de Apple traducido con rigor a una pantalla de 320×240 y una rueda física. Cada decisión se juzga con una sola pregunta: **¿esto lo firmaría Apple?**

Si una pantalla, texto o ícono deja ver el cromo de Rockbox (logotipos, cuadros de texto, nombres crudos de directorios, jerga técnica), es un bug de diseño aunque funcione. "Cuando encuentres algo que no respeta las reglas del sistema de diseño de Apple y de nuestra interfaz, no me preguntes y ajústalo" — salvo las decisiones marcadas explícitamente como abiertas en la sección 9, que sí quiero revisar contigo.

---

## 0. Qué cambió en Apple entre 2007 y 2026 (contexto que fundamenta este documento)

El iPod Classic original nació en la era del *skeuomorfismo* (relojes con manecillas, cueros, brillos). En 2013, iOS 7 lo reemplazó por diseño plano. Desde 2025, Apple está en una tercera era: **Liquid Glass**, un lenguaje unificado en iOS, iPadOS, macOS, watchOS y tvOS que introduce un material traslúcido que refleja y refracta lo que tiene detrás. Dos matices importan mucho para Aura:

1. **El vidrio es solo para la capa de navegación/controles flotantes — nunca para el contenido.** Listas, tarjetas, texto y medios se mantienen sólidos y legibles; lo que flota encima (barras, pastillas de selección, controles) es lo que puede adoptar el material de vidrio.
2. **Apple ya no publica valores de color fijos.** Los colores del sistema son *tokens semánticos* (representan un propósito — fondo, texto primario, acento — no un valor RGB), y el propio sistema decide el matiz exacto según el modo claro/oscuro, el contraste del usuario y lo que hay detrás. Esto es exactamente lo que ya hace `a26_palette`: es la aproximación correcta para un dispositivo empotrado con paleta fija, solo que en Aura los valores sí deben fijarse a mano (no hay compositor dinámico).

Este documento parte de esos dos principios y los traduce a las limitaciones reales de un iPod Classic: pantalla LCD de 320×240 sin aceleración para desenfoque en tiempo real, y una SoC de gama baja de 2007.

---

## 1. Principios

1. **Contenido antes que cromo.** Las listas van a sangre, los separadores son rayas de 1 px, la selección es una pastilla — nada de marcos, biseles ni cajas.
2. **El acento se gana.** El rosa solo aparece donde hay significado: íconos de menú y estado activo, la letra vigente del riel. Jamás como decoración de superficie.
3. **La carga convive, no interrumpe.** Ninguna operación tapa la pantalla: la pastilla flotante avanza sobre lo que ya está a la vista. Las páginas completas se reservan para estados del aparato (apagado, USB, base de datos), no para esperas.
4. **Continuidad de movimiento.** Nada aparece de golpe si puede fundirse; nada parpadea jamás (un parpadeo = dos repintados donde debía haber uno: buscar la causa, no taparla).
5. **Dos temas, un diseño.** Todo componente nace en claro y oscuro. El oscuro no es el claro invertido ni recoloreado: sus assets se generan con su propio antialias contra su propio fondo.
6. **El vidrio es un privilegio de los controles, no un efecto decorativo.** Solo la capa flotante (barra de estado, pastillas, sheets) puede insinuar translucidez; el contenido (listas, carátulas, texto) se queda siempre sólido y opaco.
7. **Español impecable.** El aparato habla español natural de cara al usuario ("Guardar como…", "Temas"), nunca jerga ("themes", "Buscando... 0 encontrado (Play/Select para cancelar)").

---

## 2. Color

Todos los colores del código C salen de `a26_palette` (tokens de `apps/apple2026_shell.h`). **Prohibido hardcodear RGB en C**; en los skins van en hex y el convertidor (`tools/apple2026_dark_skin.py`) traduce al oscuro.

| Token          | Claro       | Oscuro      | Uso                                        |
| -------------- | ----------- | ----------- | ------------------------------------------ |
| SHELL_BG       | 255,255,255 | 28,28,30    | fondo de todo                              |
| TEXT_PRIMARY   | 0,0,0       | 255,255,255 | texto principal                            |
| TEXT_SECONDARY | 110,110,115 | 152,152,157 | metadatos, subtítulos                      |
| TEXT_TERTIARY  | 60,60,67    | 199,199,204 | énfasis medio, tinta de páginas de símbolo |
| ACCENT         | 255,45,85   | 255,69,108  | íconos, estados activos                    |
| SHELL_RAIL     | 198,198,200 | 58,58,60    | separadores, bordes finos                  |
| PROGRESS_FILL  | 60,60,67    | 229,229,234 | progreso recorrido                         |
| PROGRESS_TRACK | 229,229,234 | 72,72,74    | carril de progreso                         |
| SELECTION_FILL | 229,229,234 | 44,44,46    | fila resaltada flotante                    |

- El oscuro **sube la luminosidad del acento** (el mismo rosa sobre negro pierde contraste) y usa gris muy oscuro, no negro puro, para que esquinas y sombras sigan leyéndose.
- Magenta (255,0,255) = clave de transparencia en todos los bitmaps.
- Los colores dinámicos (acento derivado de la carátula) pasan por `dynamic_colors_resolve` — solo desde el hilo de dibujo.

### 2.1 — Vidrio: dónde sí, dónde no ✅ *(decidido)*
**Resuelto: plano, sin transparencia.** La barra de estado, la pastilla de progreso flotante y la pastilla de selección de lista siguen siendo la "capa de controles" conceptualmente — son la capa que en iOS llevaría Liquid Glass — pero en Aura se renderizan con relleno sólido (`SELECTION_FILL`, `SHELL_BG`) y sin ningún halo ni alfa parcial. La referencia a Liquid Glass queda solo como principio de **jerarquía** (controles flotan sobre contenido, nunca al revés), no como efecto visual. Esto también simplifica el blitting: nada de composición alfa por línea, coherente con la RAM disponible.

---

## 3. Tipografía

- **13 px SF Compact Text**: listas y cuerpo. **7 px SF Pro**: riel A-Z. Relojes grandes: SF Pro Display (ranuras 35 pt del skin).
- No hay tamaños intermedios: si un diseño "necesita" 10 px, es que el diseño está mal — reencuadrar antes que regenerar fuentes.
- Jerarquía por color (primary/secondary/tertiary), no por tamaño.
- **Peso tipográfico** (nuevo — antes ambiguo): al ser fuentes de bitmap, el "peso" es un archivo distinto, no una variable en tiempo real. Se limitan a dos por rol, para no multiplicar el número de assets:
  - Regular → cuerpo de listas, metadatos, texto de párrafo (páginas de Copyright, Genius, audiolibros).
  - Semibold → títulos de pantalla, números grandes (reloj, cronómetro, contador de EQ), la letra activa del riel A-Z.
  - Nunca Bold ni Light: en 320×240 ambos extremos rompen el antialias a 13 px.

---

## 4. Iconografía

- **Solo SF Symbols**, rasterizados de macOS (`tools/apple2026_sf_render.swift`). Nunca dibujar glifos a mano ni usar paquetes externos.
- **Siempre la variante lineal, nunca `.fill`.** El juego de íconos de menú es de trazo; una variante rellena al lado del resto se lee como una mancha maciza y rompe la fila. Si un símbolo solo existe relleno, se busca otro que signifique lo mismo.
- Íconos de menú: frame 30×30, tinta de 18 px centrada, color acento, antialias por cobertura (supermuestreo ≥4×4; un test binario deja dientes de sierra). Viven en las DOS tiras (`icons/Apple2026Icons*.bmp`) — proceso de ampliación en `CLAUDE.md`.
- Símbolos que ya representan algo en iOS se respetan: `gear` = Configuración, `square.grid.2x2` = Extras, `play.rectangle` = Videos, `sun.max.circle`/`moon.circle` = temas, `cable.connector.horizontal` = USB, `magnifyingglass` = solo búsqueda.
- **Hermanos se distinguen**: si dos entradas de un submenú comparten símbolo, se varía con una marca pequeña (guion, punto) — nunca el mismo ícono dos veces en la misma lista.
- Páginas de estado: símbolo 96×96 en tinta terciaria (son aviso, no acción), generadas con `tools/apple2026_symbol_page.py`.

---

## 5. Geometría

- **Barra de estado: 20 px.** Título a la izquierda, reloj al centro, batería a la derecha; candado en el clúster derecho (216,4). La barra nunca cambia de forma entre pantallas.
- **Todo el texto de la barra comparte una línea base: y = 16.** Como el texto arranca en el borde superior de su viewport, la `y` de cada uno compensa el ascenso de su fuente (título 16 px → y=0; reloj completo 14 px → y=2; reloj partido 13 px → y=3).
- El indicador ▶/⏸ vive **junto a la batería solo en la barra completa**. En vista dividida no cabe —el hueco entre reloj y batería son 15 px y es del candado—, así que lo lleva la tarjeta de reproducción del panel derecho, centrado sobre la barra de progreso.
- **Vista dividida** en los dos primeros niveles de navegación: lista de 160 px + panel visual derecho que reacciona a la selección. Del tercer nivel en adelante, ancho completo. *(Esta regla ya coincide con el comportamiento observado en el firmware original — ver `Reglas de comportamiento - iPod Classic original.md`, sección 1.)*
- Listas: inset 16 px, sin chevrones, barra de deslizamiento a la derecha, riel A-Z con lupa en listas indexadas.
- Esquinas de pantalla redondeadas globales (estampado idempotente).

### 5.1 — Pastilla de selección de lista *(especificación nueva — resuelve la ambigüedad del selector)*
Ya no es una barra de ancho completo como en 2007: es un rectángulo que **no toca los bordes**, con esquinas redondeadas.
- Alto: alto de fila − 4 px (2 px de margen arriba y abajo).
- Márgenes laterales: 8 px respecto al inset de 16 px de la lista — es decir, nunca toca ni el borde de la lista ni el de la pantalla.
- Radio de esquina: 8 px.
- Relleno: `SELECTION_FILL`, sin borde ni sombra.
- Se desplaza entre filas con fundido/deslizamiento corto (ver Movimiento), nunca con un salto.

### 5.2 — Pastilla de progreso canónica
4 px de alto, extremos redondeados (filas 0 y 3 con inset 1), carril `PROGRESS_TRACK` + relleno `PROGRESS_FILL`. La flotante va en x=40, ancho−80, y=alto−14, dentro de una cápsula (alto 12, radio 6, retranqueos {4,2,1,1,0,0}, fondo `SHELL_BG`, borde `SHELL_RAIL`).

### 5.3 — Barra de deslizamiento *(nuevo — comportamiento heredado del original, grosor actualizado)*
El comportamiento se conserva del firmware original: la barra **aparece y desaparece según el avance en la lista**, nunca está fija en pantalla. Lo que cambia es el grosor, alineado a la propuesta actual de Apple (indicadores de scroll más gruesos que en 2007-2013):
- Grosor: 3 px (antes ~1 px hairline).
- Forma: cápsula, extremos redondeados (radio 1.5 px).
- Inset del borde derecho de pantalla: 2 px.
- Color: `SHELL_RAIL`, sólido — sin transparencia, consistente con la decisión de vidrio plano (§2.1).
- Alto proporcional al contenido visible sobre el total, con un mínimo de 24 px para que siga siendo legible en listas muy largas.
- Aparece con fundido lineal de 150 ms al iniciar el scroll o cambiar de selección; permanece visible mientras hay actividad; se desvanece con fundido lineal de 330 ms tras ~800 ms de inactividad (mismo timing que el resto del sistema — ver Movimiento).

### 5.4 — Radio concéntrico ✅ *(decidido: sí, sistemático)*
El HIG actual de Apple alinea el radio de un elemento con el de su contenedor: el radio del hijo es el del padre menos su propio margen interno, no un valor fijo arbitrario por tipo de componente. Escala adoptada para Aura, de afuera hacia adentro — **todo componente nuevo debe derivar su radio de esta tabla, nunca inventar uno suelto:**

| Nivel | Elemento | Radio |
|---|---|---|
| 1 | Esquinas de pantalla | 12 px (ya definido) |
| 2 | Tarjetas/paneles internos (p. ej. carátula en panel derecho) | 8 px |
| 3 | Pastillas y chips (selección, progreso) | 6–8 px, ver §5.1/5.2 |

Sombra del arte: campo de distancia con caída 2 px, pintada por tramos.

---

## 6. Movimiento

- Fundidos: ~HZ/3 (330 ms). Deriva del panel: ~4 px/s. Insignia de letra: destello centrado mientras se hojea.
- Cadencias: 20 fps para fundidos, HZ/6–HZ/8 para paneos lentos, HZ/12 para scroll de texto del mini-reproductor.
- **Toda animación se detiene con la pantalla dormida** (`lcd_active()`) — regla de energía, ver `CLAUDE.md`.
- **Easing de controles vs. contenido** ✅ *(decidido)*: la pastilla de selección se mueve con un **resorte corto, curva con sobrepaso** (~380ms, ease-out con overshoot — en C, una tabla de easing precomputada, no una física de resorte con estado). Carátulas, fundidos de pantalla y páginas de símbolo **siguen en fundido lineal** — el resorte queda reservado a la capa de controles (pastilla de selección, y por extensión la de progreso si se anima su aparición), nunca al contenido, para no romper el Principio 4.

---

## 7. Interacción de la rueda

- Girar lento = precisión absoluta, un elemento por paso. Girar fuerte (>420 º/s) = hojear por letras a ritmo legible (HZ/10). Aceleración intermedia suave (v², ×2-3 máximo). La sensación es de Apple: fluida, jamás "se me fue".
- SELECT en Reproduciendo cicla los modos de la rueda; PLAY en cualquier lista es reproducir/pausar global.
- Quickscreen: rueda = volumen; brillo con MENU/PLAY.
- La pantalla de texto adapta su cabecera al propósito (buscar / escribir / guardar como) — nunca presentar un guardado como una búsqueda.

---

## 8. Anti-patrones (no repetir)

- Cuadro de texto de sistema sobre la pantalla ("Buscando… N encontrado").
- Página completa con animación para una espera (el engrane).
- Título de lista con nombre crudo de directorio ("themes").
- Logotipo o cromo de Rockbox visible en cualquier estado, incluido USB.
- Ícono repetido sin variación entre hermanos; ícono que "desaparece" por errata en el nombre del cfg.
- Halo blanco en assets sobre fondo oscuro (antialias sin regenerar).
- Recuadro pegado al texto de la fila seleccionada (`DRMODE_SOLID` — usar `DRMODE_FG`).
- Parpadeo al cambiar de estado (dos repintados; buscar la causa).
- Selección inicial activa en listas destructivas (añadir a playlist debe entrar sin selección).
- **Vidrio (blur/translucidez) aplicado a contenido** — listas, texto o carátulas nunca llevan el material de vidrio; es exclusivo de la capa de controles (Principio 6).
- **Radios de esquina inconsistentes entre contenedor e hijo** — si un panel tiene 8 px, su chip interno no puede tener un radio arbitrario de 4 o 12 px sin relación (ver §5.3).

---

## 9. Decisiones de diseño

Las tres decisiones abiertas de la v1 de este documento ya están resueltas — quedan registradas aquí como historial.

### 9.1 — Adopción de Liquid Glass en la capa de controles ✅ resuelto
**Plano, sin transparencia.** Ver §2.1.

### 9.2 — Movimiento de los controles (resorte vs. fundido lineal) ✅ resuelto
**Resorte corto con curva de sobrepaso**, decidido tras comparar ambas opciones en una simulación lado a lado. Se implementa como tabla de easing precomputada (no física de resorte con estado), así que no compromete el presupuesto de 64 MB de RAM. Ver §6.

### 9.3 — Radio de esquina concéntrico ✅ resuelto
**Sí, jerarquía concéntrica sistemática.** Ver §5.3.

---

## 10. Lista de control para toda pantalla nueva

1. ¿La firmaría Apple? (premisa)
2. Colores por token, nada hardcodeado.
3. Se ve correcta en claro **y** oscuro (capturas de ambos).
4. Sus esperas usan la pastilla flotante; sus estados, página de símbolo.
5. Íconos SF Symbols en ambas tiras, sin repetirse entre hermanos.
6. Textos en español natural, añadidos al FINAL de ambos `.lang`.
7. Animaciones con puerta `lcd_active()` y cadencia de la tabla.
8. El vidrio, si aparece, está solo en la capa de controles — nunca en contenido.
9. Contrato del auditor actualizado si cambió geometría o assets.
10. Verificada en el simulador con el arnés (`tools/apple2026_sim_*`).
