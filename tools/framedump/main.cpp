// framedump: reads the newest frame Minecraft published in Local\CraftV_Frame_v1 (PROTOCOL.md §11) and writes
//   <out>_composite.png  the world layer over a stand-in host picture (sky and checkerboard), the overlay on top
//   <out>_depth.png      the world depth (near = white)
//   <out>_overlay.png    the overlay alone over a checkerboard
// Usage: framedump <out prefix> [--wait-ms N] [--frame NAME] [--raw]
#include "craftv/protocol.h"

#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace craftv::proto;

namespace
{
	template <class T>
	T Read(const std::uint8_t* p)
	{
		T v;
		std::memcpy(&v, p, sizeof(T));
		return v;
	}

	// ---- a tiny PNG writer (stored deflate blocks: no compression, no dependencies) -------------
	std::uint32_t Crc(const std::uint8_t* d, std::size_t n, std::uint32_t c = 0xFFFFFFFFu)
	{
		static std::uint32_t table[256];
		static bool          init = false;
		if (!init) {
			for (std::uint32_t i = 0; i < 256; ++i) {
				std::uint32_t k = i;
				for (int j = 0; j < 8; ++j) k = (k & 1) ? 0xEDB88320u ^ (k >> 1) : k >> 1;
				table[i] = k;
			}
			init = true;
		}
		for (std::size_t i = 0; i < n; ++i) c = table[(c ^ d[i]) & 0xFF] ^ (c >> 8);
		return c;
	}

	void Be32(std::vector<std::uint8_t>& o, std::uint32_t v)
	{
		o.push_back(std::uint8_t(v >> 24));
		o.push_back(std::uint8_t(v >> 16));
		o.push_back(std::uint8_t(v >> 8));
		o.push_back(std::uint8_t(v));
	}

	void Chunk(std::vector<std::uint8_t>& o, const char* type, const std::vector<std::uint8_t>& data)
	{
		Be32(o, static_cast<std::uint32_t>(data.size()));
		std::vector<std::uint8_t> td(type, type + 4);
		td.insert(td.end(), data.begin(), data.end());
		o.insert(o.end(), td.begin(), td.end());
		Be32(o, Crc(td.data(), td.size()) ^ 0xFFFFFFFFu);
	}

	bool WritePng(const std::string& path, int w, int h, const std::vector<std::uint8_t>& rgb)
	{
		std::vector<std::uint8_t> raw;
		raw.reserve(static_cast<std::size_t>(h) * (w * 3 + 1));
		for (int y = 0; y < h; ++y) {
			raw.push_back(0);
			raw.insert(raw.end(), rgb.begin() + static_cast<std::size_t>(y) * w * 3, rgb.begin() + static_cast<std::size_t>(y + 1) * w * 3);
		}
		std::vector<std::uint8_t> z = { 0x78, 0x01 };
		std::uint32_t             a = 1, b = 0;
		for (auto c : raw) {
			a = (a + c) % 65521;
			b = (b + a) % 65521;
		}
		for (std::size_t i = 0; i < raw.size(); i += 65535) {
			const std::size_t n = std::min<std::size_t>(65535, raw.size() - i);
			z.push_back(i + n == raw.size() ? 1 : 0);
			z.push_back(std::uint8_t(n));
			z.push_back(std::uint8_t(n >> 8));
			z.push_back(std::uint8_t(~n));
			z.push_back(std::uint8_t(~n >> 8));
			z.insert(z.end(), raw.begin() + i, raw.begin() + i + n);
		}
		Be32(z, (b << 16) | a);
		std::vector<std::uint8_t> png = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
		std::vector<std::uint8_t> ihdr;
		Be32(ihdr, static_cast<std::uint32_t>(w));
		Be32(ihdr, static_cast<std::uint32_t>(h));
		ihdr.insert(ihdr.end(), { 8, 2, 0, 0, 0 });
		Chunk(png, "IHDR", ihdr);
		Chunk(png, "IDAT", z);
		Chunk(png, "IEND", {});
		FILE* f = nullptr;
		if (fopen_s(&f, path.c_str(), "wb") != 0 || !f) return false;
		std::fwrite(png.data(), 1, png.size(), f);
		std::fclose(f);
		return true;
	}
}

int main(int argc, char** argv)
{
	if (argc < 2) {
		std::fprintf(stderr, "usage: framedump <out prefix> [--wait-ms N] [--frame NAME] [--raw]\n");
		return 2;
	}
	const std::string out = argv[1];
	std::wstring      frameName = kFrameMappingName;  // --frame: a second CraftV Minecraft's mapping (the stand-in guest)
	DWORD             waitMs = 10000;
	bool              rawLayers = false;
	for (int i = 2; i < argc; ++i) {
		if (std::strcmp(argv[i], "--raw") == 0) rawLayers = true;
		else if (i + 1 >= argc) break;
		else if (std::strcmp(argv[i], "--wait-ms") == 0) waitMs = static_cast<DWORD>(std::atoi(argv[++i]));
		else if (std::strcmp(argv[i], "--frame") == 0) frameName = std::wstring(argv[i + 1], argv[i + 1] + std::strlen(argv[i + 1])), ++i;
	}
	HANDLE mapping = nullptr;
	for (DWORD t0 = GetTickCount(); !mapping && GetTickCount() - t0 < waitMs; Sleep(100)) {
		mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, frameName.c_str());
	}
	if (!mapping) {
		std::fprintf(stderr, "no frame mapping (is Minecraft rendering the passthrough? mock: cam on)\n");
		return 1;
	}
	const std::uint64_t bytes = kFrameHeaderBytes + kFrameSlotStride * kFrameSlots;
	const auto*         view = static_cast<const std::uint8_t*>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, static_cast<SIZE_T>(bytes)));
	if (!view || Read<std::uint32_t>(view) != kFrameMagic || Read<std::uint32_t>(view + 4) != kFrameVersion) {
		std::fprintf(stderr, "frame mapping has the wrong magic or version\n");
		return 1;
	}
	std::int32_t slot = -1;
	for (DWORD t0 = GetTickCount(); slot < 0 && GetTickCount() - t0 < waitMs; Sleep(50)) {
		slot = Read<std::int32_t>(view + 40);
	}
	if (slot < 0 || slot >= static_cast<std::int32_t>(kFrameSlots)) {
		std::fprintf(stderr, "no frame published yet\n");
		return 1;
	}
	const std::uint8_t* desc = view + kFrameSlotDescOffset + kFrameSlotDescBytes * slot;
	std::vector<std::uint8_t> world, depth, overlay;
	std::uint32_t             w = 0, h = 0, flags = 0;
	for (int attempt = 0; attempt < 50; ++attempt) {
		const std::uint64_t seq = Read<std::uint64_t>(desc);
		if (seq & 1) {
			Sleep(2);
			continue;
		}
		w = Read<std::uint32_t>(desc + 24);
		h = Read<std::uint32_t>(desc + 28);
		flags = Read<std::uint32_t>(desc + 44);
		if (static_cast<std::uint64_t>(w) * h > kViewMaxPixels || w == 0 || h == 0) {
			std::fprintf(stderr, "bad frame size %ux%u\n", w, h);
			return 1;
		}
		const std::size_t   n = static_cast<std::size_t>(w) * h * 4;
		const std::uint8_t* base = view + kFrameHeaderBytes + kFrameSlotStride * slot;
		world.assign(base, base + n);
		depth.assign(base + n, base + 2 * n);
		overlay.assign(base + 2 * n, base + 3 * n);
		if (Read<std::uint64_t>(desc) == seq) break;
	}
	std::printf("frame: slot %d, %ux%u, mcFrame %llu, hostFrame %llu, fov %.1f, flags %u, camera (%.2f, %.2f, %.2f) yaw %.1f pitch %.1f, first person %u\n",
		slot, w, h, static_cast<unsigned long long>(Read<std::uint64_t>(desc + 8)), static_cast<unsigned long long>(Read<std::uint64_t>(desc + 16)),
		Read<float>(desc + 40), flags, Read<double>(desc + 48), Read<double>(desc + 56), Read<double>(desc + 64), Read<float>(desc + 72),
		Read<float>(desc + 76), Read<std::uint32_t>(desc + 84));

	const bool                bottomUp = (flags & kFrameBottomUp) != 0;
	if (rawLayers) {
		// --raw: both layers as they are (premultiplied RGBA, rows as in the mapping), for tools/video/capture.py
		for (const auto& [suffix, layer] : { std::pair<const char*, const std::vector<std::uint8_t>*>{ "_world.rgba", &world }, { "_overlay.rgba", &overlay } }) {
			FILE* f = nullptr;
			if (fopen_s(&f, (out + suffix).c_str(), "wb") == 0 && f) {
				std::fwrite(layer->data(), 1, layer->size(), f);
				std::fclose(f);
			}
		}
		std::printf("raw %u %u %d\n", w, h, bottomUp ? 1 : 0);
	}
	std::vector<std::uint8_t> comp(static_cast<std::size_t>(w) * h * 3), dep(comp.size()), ovl(comp.size());
	float                     dmin = 1e9f, dmax = -1e9f;
	std::size_t               covered = 0, overlayPixels = 0;
	for (std::uint32_t i = 0; i < w * h; ++i) {
		const float d = Read<float>(depth.data() + i * 4);
		if (d > 0.0f && d < 1.0f) {
			dmin = std::min(dmin, d);
			dmax = std::max(dmax, d);
		}
	}
	for (std::uint32_t y = 0; y < h; ++y) {
		const std::uint32_t sy = bottomUp ? h - 1 - y : y;
		for (std::uint32_t x = 0; x < w; ++x) {
			const std::size_t s = (static_cast<std::size_t>(sy) * w + x) * 4, o = (static_cast<std::size_t>(y) * w + x) * 3;
			// stand-in host picture: sky above the middle, a grey checkerboard "street" below
			const bool  checker = ((x / 32) + (y / 32)) % 2 == 0;
			const float bg[3] = { y < h / 2 ? 0.55f : (checker ? 0.42f : 0.36f), y < h / 2 ? 0.72f : (checker ? 0.42f : 0.36f), y < h / 2 ? 0.92f : (checker ? 0.45f : 0.39f) };
			float       c[3];
			const float wa = world[s + 3] / 255.0f, oa = overlay[s + 3] / 255.0f;
			covered += world[s + 3] > 0 ? 1 : 0;
			overlayPixels += overlay[s + 3] > 0 ? 1 : 0;
			for (int k = 0; k < 3; ++k) {
				c[k] = world[s + k] / 255.0f + bg[k] * (1.0f - wa);     // premultiplied
				c[k] = overlay[s + k] / 255.0f + c[k] * (1.0f - oa);
				comp[o + k] = static_cast<std::uint8_t>(std::clamp(c[k], 0.0f, 1.0f) * 255.0f + 0.5f);
				const float cb = checker ? 0.8f : 0.6f;
				ovl[o + k] = static_cast<std::uint8_t>(std::clamp(overlay[s + k] / 255.0f + cb * (1.0f - oa), 0.0f, 1.0f) * 255.0f + 0.5f);
			}
			const float d = Read<float>(depth.data() + s);
			const float v = (d > 0.0f && d < 1.0f && dmax > dmin) ? 0.15f + 0.85f * (d - dmin) / (dmax - dmin) : 0.0f;
			dep[o] = dep[o + 1] = dep[o + 2] = static_cast<std::uint8_t>(v * 255.0f);
		}
	}
	std::printf("world covers %.1f%% of the picture, overlay %.1f%%, depth range %.6f..%.6f\n", 100.0 * covered / (w * h), 100.0 * overlayPixels / (w * h),
		dmin, dmax);
	const bool ok = WritePng(out + "_composite.png", w, h, comp) && WritePng(out + "_depth.png", w, h, dep) && WritePng(out + "_overlay.png", w, h, ovl);
	std::printf("%s %s_{composite,depth,overlay}.png\n", ok ? "wrote" : "FAILED to write", out.c_str());
	return ok ? 0 : 1;
}
