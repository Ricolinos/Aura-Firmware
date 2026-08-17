# firmware/dist/

Este directorio recibe los artefactos empaquetados del firmware cuando corres
`firmware/tools/package_dist.sh`. **No se versionan aquí** (ver `.gitignore`)
porque cada versión pesa varios MB y el historial ya cambió de tamaño una vez
por este motivo (ver `AUDIT-pre-split.md` de la separación de repositorios).

Artefactos que produce el script:

- `rockbox.ipod` — binario del firmware para el iPod Classic 6G.
- `rockbox.zip` — árbol `.rockbox/` completo (fuentes, íconos, códecs,
  plugins) para copiar al disco del dispositivo.
- `bootloader-ipod6g.ipod` — bootloader de arranque dual (compilación manual,
  ver nota abajo).
- `mks5lboot` — herramienta para grabar el bootloader por DFU.
- `checksums.txt` — SHA-256 de los cuatro artefactos anteriores.

Para instalarlos en un dispositivo real, la vía sancionada del proyecto es
Aura Studio (repositorio aparte), que descarga estos artefactos desde un
[Release](../../../releases) de este repositorio, verifica sus checksums y
los copia/flashea siguiendo el procedimiento documentado en
`docs/guia-flasheo-restauracion.md` del repositorio de Aura Studio.

**Nota sobre el bootloader**: `package_dist.sh` compila `rockbox.ipod` y
`mks5lboot` de forma automática, pero compilar `bootloader-ipod6g.ipod`
requiere un toolchain específico del bootloader (`configure --type=B` en
`firmware/rockbox/`) que el script no automatiza todavía — se documenta el
paso manual dentro del propio script.
