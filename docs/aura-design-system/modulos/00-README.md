# Módulos

⚪ Sin empezar todavía.

Un "módulo" en este sistema es una composición de varios componentes que se
replica igual (o casi igual) en distintas pantallas — no un componente
atómico individual (eso vive en `componentes/`).

Ejemplo hipotético de candidato a módulo (a confirmar): la combinación
`StatusBar + LeftPanel` como unidad que aparece igual en cualquier pantalla
que use el layout `split`, sin importar qué haya del lado derecho.

## Pendiente

- [ ] Inventariar qué combinaciones se repiten entre pantallas ya diseñadas
      (Now Playing, menús, etc.) para identificar los primeros módulos reales
- [ ] Definir si un módulo puede tener su propio estado interno o si solo
      orquesta los estados de sus componentes hijos
