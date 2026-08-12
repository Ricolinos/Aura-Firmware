# DateEditor (stub)

Componente que aparece a pantalla completa al entrar a Ajustes → Fecha y
Hora → Fecha. Todavía muy poco definido — este archivo existe para no perder
el ejemplo mientras se aclara.

## Lo que sabemos

- Se llega aquí desde un ícono dinámico de hoja de calendario (ver variante
  dinámica en `componentes/selection-summary.md`) montado en `split`.
- Al hacer clic, transiciona a `full`.
- **Confirmado: usa el patrón `Shift-and-Reveal`.** El ícono nunca sale de
  pantalla — es el mismo elemento que se reacomoda y se adapta a la nueva
  interfaz (persistente, no una salida + entrada de un elemento distinto).
  Spec completo del patrón en `transiciones/00-vocabulario.md`.
- En paralelo, `LeftPanel` sí sigue el comportamiento estándar: sale hacia
  la izquierda, como empujado.
- Los textos auxiliares (hora arriba del ícono, otro texto abajo) desaparecen
  al transicionar — no persisten ni se reposicionan junto con el ícono.
- `StatusBar (full)` entra en Drop desde arriba al completarse la transición
  — igual que en el caso ya documentado.

## Pendiente de definir (casi todo)

- [ ] Contenido y comportamiento de `DateEditor` en sí — no descrito todavía
- [ ] Si `Shift-and-Reveal` aplica solo a Fecha, o a más pantallas de
      Ajustes con el mismo esquema de ícono dinámico
