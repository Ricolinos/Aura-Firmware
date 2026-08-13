// Renderiza un lote de SF Symbols a PNGs cuadrados, negro sobre alfa 0.
//
// SF Symbols no se distribuyen como archivos sueltos que se puedan
// versionar (la app SF Symbols exporta SVG de a uno a mano, y ni siquiera
// esta instalada en esta maquina); la via programatica y reproducible es
// pedirselos al sistema con NSImage(systemSymbolName:) -- la MISMA fuente
// que usa cualquier app de Apple. Requiere macOS 11+.
//
// Se procesa un lote entero por invocacion, leyendo la lista de trabajos
// como JSON por stdin: `swift file.swift` recompila el script en cada
// ejecucion (~2s), asi que 57 invocaciones costarian minutos y una sola
// cuesta segundos.
//
// Entrada (stdin):  [{"symbol":"music.note","px":20,"weight":"medium","out":"/ruta/x.png"}, ...]
// Salida: los PNG pedidos. Codigo != 0 si algun simbolo no existe.
//
// El color no importa aca: generate.py umbraliza el alfa y aplica el
// color del tema/variante. Este script solo define la FORMA.

import AppKit
import Foundation

struct Job: Decodable {
    let symbol: String
    let px: Int
    let weight: String
    let out: String
    // pointSize fijo opcional: se dibuja a tamano NATURAL centrado en el
    // lienzo, sin contain -- para familias cuyo cuerpo debe medir igual
    // entre variantes de distinto ancho (bocina dinamica, 2026-08-12).
    let pt: Double?
}

let weights: [String: NSFont.Weight] = [
    "ultralight": .ultraLight, "thin": .thin, "light": .light,
    "regular": .regular, "medium": .medium, "semibold": .semibold,
    "bold": .bold, "heavy": .heavy, "black": .black,
]

func fail(_ msg: String) -> Never {
    FileHandle.standardError.write((msg + "\n").data(using: .utf8)!)
    exit(1)
}

let input = FileHandle.standardInput.readDataToEndOfFile()
guard let jobs = try? JSONDecoder().decode([Job].self, from: input) else {
    fail("no se pudo leer la lista de trabajos JSON por stdin")
}

for job in jobs {
    guard let weight = weights[job.weight] else {
        fail("peso desconocido: \(job.weight)")
    }
    guard let symbol = NSImage(systemSymbolName: job.symbol, accessibilityDescription: nil) else {
        fail("SF Symbol no disponible en este macOS: \(job.symbol)")
    }

    // pointSize se pide algo menor que el lienzo: a un pointSize P la caja
    // de un SF Symbol es mas alta que P, asi que pedir P = px lo dejaria
    // recortado o con un relleno optico distinto al del resto del set.
    let config = NSImage.SymbolConfiguration(pointSize: job.pt ?? Double(job.px) * 0.82, weight: weight)
    guard let configured = symbol.withSymbolConfiguration(config) else {
        fail("no se pudo configurar \(job.symbol)")
    }

    guard let rep = NSBitmapImageRep(
        bitmapDataPlanes: nil, pixelsWide: job.px, pixelsHigh: job.px,
        bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false,
        colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0) else {
        fail("no se pudo crear el bitmap para \(job.symbol)")
    }
    rep.size = NSSize(width: job.px, height: job.px)

    NSGraphicsContext.saveGraphicsState()
    NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: rep)
    NSGraphicsContext.current?.imageInterpolation = .high

    // Escalar para caber (contain) y centrar: preserva las proporciones
    // nativas de cada simbolo, que es lo que los hace ver opticamente
    // consistentes entre si dentro de un mismo tamano.
    let natural = configured.size
    var scale = min(Double(job.px) / natural.width, Double(job.px) / natural.height)
    if job.pt != nil {
        if natural.width > Double(job.px) || natural.height > Double(job.px) {
            fail("\(job.symbol) a pointSize fijo no cabe en el lienzo de \(job.px)px "
                 + "(natural \(natural)) -- agranda el lienzo en tokens.json")
        }
        scale = 1.0 // tamano natural: el cuerpo mide igual en toda la familia
    }
    let drawn = NSSize(width: natural.width * scale, height: natural.height * scale)
    let origin = NSPoint(x: (Double(job.px) - drawn.width) / 2.0,
                         y: (Double(job.px) - drawn.height) / 2.0)

    NSColor.black.set()
    configured.draw(in: NSRect(origin: origin, size: drawn),
                    from: .zero, operation: .sourceOver, fraction: 1.0)

    NSGraphicsContext.restoreGraphicsState()

    guard let png = rep.representation(using: .png, properties: [:]) else {
        fail("no se pudo codificar el PNG de \(job.symbol)")
    }
    do {
        try png.write(to: URL(fileURLWithPath: job.out))
    } catch {
        fail("no se pudo escribir \(job.out): \(error)")
    }
}
