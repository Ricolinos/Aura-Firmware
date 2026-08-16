# WorldClock (Reloj internacional)

Pantalla de Extras que muestra varios husos a la vez, cada uno con su
**reloj analógico**, su lugar y su hora en formato de 12 horas.
Confirmada 2026-08-13.

## Anatomía

Filas de 52px, cada una con:

| Elemento | Detalle |
|---|---|
| Esfera | Reloj analógico de 40px de diámetro (radio 20) |
| Lugar | Bold 12 |
| Hora | Regular 12 en tinta secundaria, formato `h:mm AM/PM` |

La fila seleccionada lleva la pastilla `SELECTION_FILL` con el nombre en
acento, como cualquier lista del sistema.

## Esfera clara vs oscura (regla del original)

- **La primera fila es la hora LOCAL** y va con la **esfera clara**
  (fondo del shell, manecillas en tinta primaria).
- **Los demás husos van con la esfera oscura** (invertida: fondo de
  tinta primaria, manecillas en el fondo del shell).

Ambas comparten el aro en tinta primaria, las marcas de 12/3/6/9 y dos
manecillas. La **horaria avanza con los minutos**, no a saltos de hora.
Las manecillas se calculan con la LUT de senos en punto fijo del
sistema (la misma de CoverFlow, `IANGLE 1024` = vuelta completa): cero
coma flotante, cero tabla nueva.

## Husos

Catálogo por continente (África, América del Norte, América del Sur,
Asia, Atlántico, Europa, Oceanía, Pacífico) con el desplazamiento
respecto de UTC en **cuartos de hora**, de modo que husos como +5:30 o
+5:45 caben sin coma flotante. **No hay horario de verano**: el original
tampoco lo resolvía por ciudad, y fingir que sí sería peor que no
tenerlo.

La hora de cada ciudad es `local + (huso_ciudad − huso_local)`, donde
el huso local es un ajuste propio (`tz_local_quarters`, por defecto
UTC−6). Ese mismo dato alimenta **Ajustes → Fecha y hora → Zona
horaria** (D-164): una sola tabla de husos/ciudades en todo el firmware —
la pantalla de Ajustes fija `tz_local_quarters` con la misma lista de 40
ciudades que este componente usa para añadir relojes.

## Interacción

- **Rueda**: recorre los relojes.
- **Select**: abre un **menú flotante** (nunca pantalla completa) con
  **Añadir · Editar · Eliminar**.
  - *Añadir*: continente → ciudad. Al elegir ciudad, vuelve directo a la
    lista de relojes (el gesto terminó, no hay que salir a mano de dos
    niveles).
  - *Editar*: el mismo flujo, pero reemplaza la ciudad de la fila
    seleccionada.
  - *Eliminar*: quita la fila (solo aplica a husos añadidos; la hora
    local no se puede borrar).
- **Menu**: cierra el menú flotante si está abierto; si no, sale.

Hasta 4 husos además de la hora local.
