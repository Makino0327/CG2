# 死亡時の血だまり用テクスチャを生成する。
# 円を複数つないだ形に揺らぎを加え、まん丸ではない輪郭にする。
# 使い方: powershell -ExecutionPolicy Bypass -File tools/generate_blood_pool.ps1
param(
    [string]$OutputDirectory = (Join-Path $PSScriptRoot "..\project\Resources\blood")
)

Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class BloodPoolGenerator
{
    struct Ball { public double X, Y, R; public Ball(double x, double y, double r) { X = x; Y = y; R = r; } }

    static double Hash(int x, int y, int seed)
    {
        unchecked {
            uint h = (uint)(x * 374761393 + y * 668265263 + seed * 1442695041);
            h = (h ^ (h >> 13)) * 1274126177u;
            h ^= h >> 16;
            return (h & 0xffffff) / 16777215.0;
        }
    }

    static double Noise(double x, double y, int seed)
    {
        int xi = (int)Math.Floor(x), yi = (int)Math.Floor(y);
        double fx = x - xi, fy = y - yi;
        double u = fx * fx * (3 - 2 * fx), v = fy * fy * (3 - 2 * fy);
        double a = Hash(xi, yi, seed), b = Hash(xi + 1, yi, seed);
        double c = Hash(xi, yi + 1, seed), d = Hash(xi + 1, yi + 1, seed);
        return (a + (b - a) * u) + ((c + (d - c) * u) - (a + (b - a) * u)) * v;
    }

    static double Fbm(double x, double y, int seed)
    {
        double sum = 0, amp = 0.5, total = 0;
        for (int i = 0; i < 4; ++i) {
            sum += Noise(x, y, seed + i * 31) * amp;
            total += amp; amp *= 0.5; x *= 2.03; y *= 2.03;
        }
        return sum / total;
    }

    static double Smooth(double e0, double e1, double x)
    {
        double t = Math.Max(0, Math.Min(1, (x - e0) / (e1 - e0)));
        return t * t * (3 - 2 * t);
    }

    // mainRadius: 中心の溜まりの大きさ, lobeCount: 周りの膨らみの数
    public static void Generate(string path, int size, int seed, double mainRadius, int lobeCount, double elongation, int dropCount)
    {
        var random = new Random(seed);
        var balls = new List<Ball>();
        balls.Add(new Ball(0, 0, mainRadius));
        for (int i = 0; i < lobeCount; ++i) {
            double angle = random.NextDouble() * Math.PI * 2;
            double distance = mainRadius * (0.8 + random.NextDouble() * 0.5);
            double radius = mainRadius * (0.22 + random.NextDouble() * 0.3);
            balls.Add(new Ball(Math.Cos(angle) * distance * elongation, Math.Sin(angle) * distance, radius));
        }
        // 輪郭から少し離れた小さな飛び散りを置く。
        var drops = new List<Ball>();
        for (int i = 0; i < dropCount; ++i) {
            double angle = random.NextDouble() * Math.PI * 2;
            double distance = mainRadius * (1.9 + random.NextDouble() * 0.45);
            drops.Add(new Ball(Math.Cos(angle) * distance * elongation, Math.Sin(angle) * distance,
                0.018 + random.NextDouble() * 0.035));
        }

        var bitmap = new Bitmap(size, size, PixelFormat.Format32bppArgb);
        var data = bitmap.LockBits(new Rectangle(0, 0, size, size), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
        var pixels = new byte[size * size * 4];

        for (int py = 0; py < size; ++py) {
            for (int px = 0; px < size; ++px) {
                double x = (px + 0.5) / size * 2 - 1;
                double y = (py + 0.5) / size * 2 - 1;
                // 座標を揺らし、円をつないだだけの滑らかすぎる輪郭を崩す。
                double wx = x + (Fbm(x * 3.1 + 7.3, y * 3.1, seed) - 0.5) * 0.24 + (Fbm(x * 9, y * 9, seed + 5) - 0.5) * 0.05;
                double wy = y + (Fbm(x * 3.1, y * 3.1 + 2.9, seed + 11) - 0.5) * 0.24 + (Fbm(x * 9 + 3, y * 9, seed + 7) - 0.5) * 0.05;

                double field = 0;
                foreach (var b in balls) {
                    double dx = wx - b.X, dy = wy - b.Y;
                    field += b.R * b.R / (dx * dx + dy * dy + 1e-6);
                }
                // 1で縁、内側ほど大きくなる、厚み代わりの値。
                double s = 1 - 1 / Math.Sqrt(field);
                foreach (var d in drops) {
                    double dx = wx - d.X, dy = wy - d.Y;
                    s = Math.Max(s, (1 - Math.Sqrt(dx * dx + dy * dy) / d.R) * 0.35);
                }
                // 画像の端で切れないよう、外周付近は必ず透明にする。
                double border = Math.Min(Math.Min(px, size - 1 - px), Math.Min(py, size - 1 - py)) / (double)size;
                s = Math.Min(s, (border - 0.02) * 4);

                double depth = Smooth(0.0, 0.2, s);
                double alpha = Smooth(-0.005, 0.03, s) * (0.86 + 0.12 * depth);
                // 縁は薄く明るい赤、中央は厚く黒に近い赤にする。
                double r = 150 + (58 - 150) * depth;
                double g = 16 + (3 - 16) * depth;
                double bl = 16 + (6 - 16) * depth;
                // 乾き始めた縁の濃い輪と、表面のむらを加える。
                double rim = 1 - 0.22 * Smooth(0.0, 0.03, s) * (1 - Smooth(0.05, 0.12, s));
                double mottle = 0.9 + 0.2 * Fbm(x * 6 + 1.7, y * 6 - 4.1, seed + 23);
                double shade = rim * mottle;

                int index = (py * size + px) * 4;
                pixels[index + 0] = (byte)Math.Max(0, Math.Min(255, bl * shade));
                pixels[index + 1] = (byte)Math.Max(0, Math.Min(255, g * shade));
                pixels[index + 2] = (byte)Math.Max(0, Math.Min(255, r * shade));
                pixels[index + 3] = (byte)Math.Max(0, Math.Min(255, alpha * 255));
            }
        }
        Marshal.Copy(pixels, 0, data.Scan0, pixels.Length);
        bitmap.UnlockBits(data);
        bitmap.Save(path, ImageFormat.Png);
        bitmap.Dispose();
    }
}
"@

New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$directory = (Resolve-Path $OutputDirectory).Path
# 体の下に広がる大きな溜まりと、そこから流れ出す細長い溜まり。
[BloodPoolGenerator]::Generate((Join-Path $directory "bloodPoolA.png"), 512, 1207, 0.36, 9, 1.0, 6)
[BloodPoolGenerator]::Generate((Join-Path $directory "bloodPoolB.png"), 512, 4931, 0.28, 6, 1.3, 4)
Write-Host "Generated blood pool textures in $directory"
