// macOS artwork maintenance: swift tools/refine_dark_icon.swift
// Rebuild the dark PNG family from the retained original artwork.
import Foundation
import CoreGraphics
import ImageIO
import UniformTypeIdentifiers

func read(_ path: String) -> CGImage {
    let source = CGImageSourceCreateWithURL(URL(fileURLWithPath: path) as CFURL, nil)!
    return CGImageSourceCreateImageAtIndex(source, 0, nil)!
}
func context(_ size: Int) -> CGContext {
    CGContext(data: nil, width: size, height: size, bitsPerComponent: 8,
              bytesPerRow: size * 4, space: CGColorSpace(name: CGColorSpace.sRGB)!,
              bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
}
func save(_ image: CGImage, _ path: String) {
    let output = CGImageDestinationCreateWithURL(URL(fileURLWithPath: path) as CFURL,
                                                 UTType.png.identifier as CFString, 1, nil)!
    CGImageDestinationAddImage(output, image, nil)
    precondition(CGImageDestinationFinalize(output))
}

struct RoundRect {
    var left, top, right, bottom, radius: Double
}

// Signed distance in top-left pixel space. Positive is outside. The normal points out.
func sdf(_ x: Double, _ y: Double, _ rr: RoundRect) -> (d: Double, nx: Double, ny: Double) {
    let halfW = (rr.right - rr.left) / 2
    let halfH = (rr.bottom - rr.top) / 2
    let px = x - (rr.left + rr.right) / 2
    let py = y - (rr.top + rr.bottom) / 2
    let qx = abs(px) - halfW + rr.radius
    let qy = abs(py) - halfH + rr.radius
    let ox = max(qx, 0), oy = max(qy, 0)
    let outside = (ox * ox + oy * oy).squareRoot()
    let d = min(max(qx, qy), 0) + outside - rr.radius
    let sx = px < 0 ? -1.0 : 1.0
    let sy = py < 0 ? -1.0 : 1.0
    var nx = 0.0, ny = 0.0
    if ox > 0 || oy > 0 {
        if outside > 1e-8 {
            nx = sx * ox / outside
            ny = sy * oy / outside
        }
    } else if qx > qy {
        nx = sx
    } else {
        ny = sy
    }
    return (d, nx, ny)
}

func topLeftBytes(_ image: CGImage) -> (w: Int, h: Int, pixels: [UInt8]) {
    let w = image.width, h = image.height
    var pixels = [UInt8](repeating: 0, count: w * h * 4)
    let ctx = CGContext(data: &pixels, width: w, height: h, bitsPerComponent: 8,
                        bytesPerRow: w * 4, space: CGColorSpace(name: CGColorSpace.sRGB)!,
                        bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
    // This bitmap context stores row 0 as the top. Drawing without a flip keeps it there.
    ctx.draw(image, in: CGRect(x: 0, y: 0, width: w, height: h))
    return (w, h, pixels)
}

func imageFromTopLeft(_ pixels: [UInt8], _ w: Int, _ h: Int) -> CGImage {
    var storage = pixels
    let ctx = CGContext(data: &storage, width: w, height: h, bitsPerComponent: 8,
                        bytesPerRow: w * 4, space: CGColorSpace(name: CGColorSpace.sRGB)!,
                        bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
    return ctx.makeImage()!
}

let root = URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent().path
let assets = root + "/assets/images"
let source = read(assets + "/candidates/SpentIt_dark_source.png")
precondition(source.width == 1024 && source.height == 1024)
let (w, h, original) = topLeftBytes(source)
// Opaque silhouette of the retained artwork: flush with the top and sides,
// four pixels short of the bottom, circular corners of radius 214.
let outer = RoundRect(left: 0, top: 0, right: 1024, bottom: 1020, radius: 214)

func straight(_ pixels: [UInt8], _ x: Int, _ y: Int) -> (r: Double, g: Double, b: Double, a: Double) {
    let i = (y * w + x) * 4
    let a = Double(pixels[i + 3]) / 255
    if a < 1e-4 { return (0, 0, 0, 0) }
    return (Double(pixels[i]) / a, Double(pixels[i + 1]) / a, Double(pixels[i + 2]) / a, a)
}

// The rim is white. The tile a pixel or two inside it is dark. A soft highlight
// sits further in, so the replacement color has to come from the near dark pixel.
func inwardDark(_ x: Int, _ y: Int, _ nx: Double, _ ny: Double) -> (UInt8, UInt8, UInt8) {
    var fallback = (UInt8(20), UInt8(24), UInt8(28))
    for step in 1...8 {
        let sx = min(max(Int((Double(x) + 0.5 - nx * Double(step)).rounded(.down)), 0), w - 1)
        let sy = min(max(Int((Double(y) + 0.5 - ny * Double(step)).rounded(.down)), 0), h - 1)
        let p = straight(original, sx, sy)
        let lum = (p.r + p.g + p.b) / 3
        fallback = (UInt8(min(max(p.r, 0), 255).rounded()),
                    UInt8(min(max(p.g, 0), 255).rounded()),
                    UInt8(min(max(p.b, 0), 255).rounded()))
        if p.a > 0.94 && lum < 72 { return fallback }
    }
    return fallback
}

var cleaned = [UInt8](repeating: 0, count: w * h * 4)
var replaced = 0
for y in 0..<h {
    for x in 0..<w {
        let (d, nx, ny) = sdf(Double(x) + 0.5, Double(y) + 0.5, outer)
        let src = straight(original, x, y)
        let lum = (src.r + src.g + src.b) / 3
        // Coins stay well inside the curve. Refuse to repaint one.
        if src.a > 0.78 && src.r > 160 && src.g > 110 && src.b < 140 && src.r > src.g && src.g > src.b {
            precondition(d < -20, "coin pixel entered the rim repair")
        }
        if d > 4 { continue }
        let fringe = d > -0.75 || (d > -6 && (lum > 100 || (src.a < 0.94 && lum > 80)))
        let rgb: (UInt8, UInt8, UInt8)
        if fringe {
            rgb = inwardDark(x, y, nx, ny)
            replaced += 1
        } else if src.a < 1e-4 {
            continue
        } else {
            rgb = (UInt8(src.r.rounded()), UInt8(src.g.rounded()), UInt8(src.b.rounded()))
        }
        let i = (y * w + x) * 4
        cleaned[i] = rgb.0
        cleaned[i + 1] = rgb.1
        cleaned[i + 2] = rgb.2
        cleaned[i + 3] = 255
    }
}
precondition(replaced > 0)

let cleanedImage = imageFromTopLeft(cleaned, w, h)
let high = context(4096)
high.interpolationQuality = .high
high.scaleBy(x: 4, y: 4)
// CoreGraphics y grows upward. The tile is flush with the top of the 1024 canvas.
let tile = CGRect(x: outer.left, y: Double(h) - outer.bottom,
                  width: outer.right - outer.left, height: outer.bottom - outer.top)
high.addPath(CGPath(roundedRect: tile, cornerWidth: outer.radius, cornerHeight: outer.radius, transform: nil))
high.clip()
high.draw(cleanedImage, in: CGRect(x: 0, y: 0, width: 1024, height: 1024))
let master = high.makeImage()!

for size in [16, 32, 64, 128, 256, 512, 1024] {
    let output = context(size)
    output.interpolationQuality = .high
    output.draw(master, in: CGRect(x: 0, y: 0, width: size, height: size))
    save(output.makeImage()!, assets + "/SpentIt_dark_\(size).png")
}
print("Generated seven dark PNGs with the white corner rim removed.")
