# DateEditor

Componente que aparece a pantalla completa al entrar a Ajustes → Fecha y
hora → Fecha. La pantalla en sí **existe y funciona** (D-170); lo que sigue
pendiente es la transición de entrada (`Shift-and-Reveal`) y el ícono
dinámico de hoja de calendario que la anuncia desde `split` — ver abajo.

## Contenido y comportamiento (D-170, encargo del dueño 2026-08-13)

La pantalla reutiliza la **rejilla del mes de Calendarios**, pero como
**selector**, no como vista de solo lectura: el día resaltado es el borrador
en edición.

| Zona | Contenido |
|---|---|
| `StatusBar (full)` | Título "Fecha" |
| Cabecera, centrada bajo la barra | "Mes Año" en Bold 12 (ej. "agosto 2026"). Se pinta en **acento** mientras el campo activo es mes o año; en tinta primaria mientras se edita el día |
| Fila de días de la semana | Iniciales en Regular 8, tinta terciaria, semana desde lunes |
| Rejilla | 7 columnas × hasta 6 filas, celdas de 24px de alto. El día seleccionado lleva pastilla `SELECTION_FILL` con el radio de pastilla del sistema y su número en **acento** (solo mientras el campo activo es el día); el resto en tinta primaria, Regular 12 |
| Debajo de la rejilla | La fecha completa en español natural, Regular 12, tinta secundaria — "13 de agosto de 2026" — que **cambia en vivo** con el campo que se esté ajustando |

**Controles:**

| Botón | Efecto |
|---|---|
| Rueda | Ajusta el campo activo: día (envuelve dentro del mes), mes (envuelve; si el día ya no existe en el mes nuevo se recorta al último), año (no baja de 2000) |
| Select | Avanza al siguiente campo — día → mes → año — y en el último **confirma y sale** |
| Menu | **Cancela** sin aplicar y sale |

**Persistencia:** al confirmar escribe la fecha al RTC del dispositivo
(`rtc_write_datetime()`), real en hardware; en el simulador el editor
funciona igual pero no tiene RTC propio adónde persistir — comportamiento
honesto, no un bug (D-170). Al volver a entrar, el borrador arranca de la
fecha actual del sistema, no de lo último que se editó.

Comparte el mismo esquema con el editor de **Hora** hermano (mismo submenú):
reloj analógico en vivo con la LUT de senos ya usada en Alarmas y Reloj
internacional, Select avanza hora → minuto y confirma, Menu cancela.

## Cómo se llega (intención de diseño — todavía no implementado)

- Se llega aquí desde un ícono dinámico de hoja de calendario (ver variante
  dinámica en `componentes/selection-summary.md`) montado en `split`. Hoy la
  fila Fecha usa un ícono estático de calendario; la variante dinámica de
  hoja de calendario no existe todavía (solo existe la de reloj analógico,
  para la fila Hora).
- Al hacer clic, transiciona a `full`.
- **Confirmado: usa el patrón `Shift-and-Reveal`.** El ícono nunca sale de
  pantalla — es el mismo elemento que se reacomoda y se adapta a la nueva
  interfaz (persistente, no una salida + entrada de un elemento distinto).
  Spec completo del patrón en `transiciones/00-vocabulario.md`. Hoy la
  pantalla entra por la transición estándar de pantalla completa; el patrón
  sigue sin implementarse.
- En paralelo, `LeftPanel` sí sigue el comportamiento estándar: sale hacia
  la izquierda, como empujado.
- Los textos auxiliares (hora arriba del ícono, otro texto abajo) desaparecen
  al transicionar — no persisten ni se reposicionan junto con el ícono.
- `StatusBar (full)` entra en Drop desde arriba al completarse la transición
  — igual que en el caso ya documentado.

## Pendiente de definir

- [ ] Si `Shift-and-Reveal` aplica solo a Fecha, o a más pantallas de
      Ajustes con el mismo esquema de ícono dinámico
