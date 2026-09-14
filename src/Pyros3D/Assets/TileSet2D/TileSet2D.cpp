//============================================================================
// Name        : TileSet2D.cpp
// Description : Reading and writing .p3dt - see the header for what one is.
//
//               JSON for the same reason a .p3d2d is: a tileset is small, and
//               it is exactly the kind of file a person wants to diff or fix
//               by hand after a bad re-slice.
//============================================================================

#include <Pyros3D/Assets/TileSet2D/TileSet2D.h>
#include <Pyros3D/Utils/Json/json.hpp>
#include <Pyros3D/Ext/stb/stb_image.h>
#include <fstream>
#include <sstream>

namespace p3d {

	namespace {

		using json = nlohmann::json;

		const int32 kTileSet2DVersion = 1;

		void Fail(std::string* errorOut, const std::string &msg)
		{
			if (errorOut) *errorOut = msg;
		}

		// How many whole cells fit along an axis. n cells occupy
		// margin + n*size + (n-1)*spacing + margin, so solving for n gives
		// this. The +spacing in the numerator is that (n-1) - drop it and a
		// sheet whose last cell ends flush against the margin loses that
		// cell, which looks like an off-by-one in the art rather than here.
		int32 FitCount(const int32 extent, const int32 size,
			const int32 margin, const int32 spacing)
		{
			if (size < 1) return 0;
			const int32 usable = extent - 2 * margin + spacing;
			if (usable < 1) return 0;
			const int32 n = usable / (size + spacing);
			return n > 0 ? n : 0;
		}

	}

	void TileSet2D::SetImageSize(const int32 w, const int32 h)
	{
		imageW = w > 0 ? w : 0;
		imageH = h > 0 ? h : 0;
	}

	int32 TileSet2D::Columns() const
	{
		if (columns > 0) return columns;
		if (!HaveImageSize()) return 0;
		return FitCount(imageW, tileW, margin, spacing);
	}

	int32 TileSet2D::Rows() const
	{
		if (!HaveImageSize()) return 0;
		return FitCount(imageH, tileH, margin, spacing);
	}

	int32 TileSet2D::TileCount() const
	{
		// Columns() can be the authored override, which may be narrower than
		// the sheet - so the count is the grid actually addressed, not what
		// the image could hold.
		return Columns() * Rows();
	}

	Vec4 TileSet2D::UVRect(const int32 index) const
	{
		const int32 cols = Columns();
		if (cols < 1 || index < 0 || index >= TileCount() || !HaveImageSize())
			return Vec4(0.f, 0.f, 0.f, 0.f);

		const int32 col = index % cols;
		const int32 row = index / cols;

		const f32 px = (f32)(margin + col * (tileW + spacing));
		const f32 py = (f32)(margin + row * (tileH + spacing));

		const f32 w = (f32)imageW;
		const f32 h = (f32)imageH;

		// Half a texel in from each edge - see the header.
		return Vec4(
			(px + 0.5f) / w,
			(py + 0.5f) / h,
			(px + (f32)tileW - 0.5f) / w,
			(py + (f32)tileH - 0.5f) / h);
	}

	const TileInfo2D* TileSet2D::Find(const int32 index) const
	{
		std::map<int32, TileInfo2D>::const_iterator it = tiles.find(index);
		return it == tiles.end() ? NULL : &it->second;
	}

	bool TileSet2D::IsSolid(const int32 index) const
	{
		const TileInfo2D* t = Find(index);
		return t != NULL && t->solid;
	}

	bool TileSet2DReadImageSize(const std::string &resolvedPath, int32 &w, int32 &h)
	{
		int iw = 0, ih = 0, comp = 0;
		if (!stbi_info(resolvedPath.c_str(), &iw, &ih, &comp)) return false;
		w = (int32)iw;
		h = (int32)ih;
		return iw > 0 && ih > 0;
	}

	std::string TileSet2DToString(const TileSet2D &set)
	{
		json j;
		j["version"] = kTileSet2DVersion;
		j["image"] = set.image;
		j["tileW"] = set.tileW;
		j["tileH"] = set.tileH;
		// Omitted when zero: the common sheet has neither, and writing two
		// zeroes into every file buries the two knobs that do matter.
		if (set.margin != 0) j["margin"] = set.margin;
		if (set.spacing != 0) j["spacing"] = set.spacing;
		if (set.columns > 0) j["columns"] = set.columns;

		json arr = json::array();
		for (std::map<int32, TileInfo2D>::const_iterator it = set.tiles.begin();
			it != set.tiles.end(); ++it)
		{
			// A cell that says nothing is not worth a line.
			if (!it->second.solid && it->second.tags.empty()) continue;
			json t;
			t["i"] = it->first;
			if (it->second.solid) t["solid"] = true;
			if (!it->second.tags.empty()) t["tags"] = it->second.tags;
			arr.push_back(t);
		}
		if (!arr.empty()) j["tiles"] = arr;

		return j.dump(1, '\t');
	}

	bool TileSet2DFromString(const std::string &text, TileSet2D &out,
		std::string *errorOut)
	{
		json j;
		try {
			j = json::parse(text);
		}
		catch (const std::exception &e) {
			Fail(errorOut, std::string("malformed tileset JSON: ") + e.what());
			return false;
		}
		if (!j.is_object()) { Fail(errorOut, "tileset is not a JSON object"); return false; }

		TileSet2D s;
		s.image = j.value("image", std::string());
		s.tileW = j.value("tileW", 16);
		s.tileH = j.value("tileH", 16);
		s.margin = j.value("margin", 0);
		s.spacing = j.value("spacing", 0);
		s.columns = j.value("columns", 0);

		// Checked rather than clamped. Every one of these silently cuts the
		// wrong cells instead of failing, and a tileset is authored once and
		// read forever - the report belongs at the point it can still be
		// fixed.
		if (s.tileW < 1 || s.tileH < 1)
		{
			Fail(errorOut, "tileW and tileH must be at least 1");
			return false;
		}
		if (s.margin < 0 || s.spacing < 0)
		{
			Fail(errorOut, "margin and spacing cannot be negative");
			return false;
		}
		if (s.columns < 0)
		{
			Fail(errorOut, "columns cannot be negative (0 means derive it)");
			return false;
		}

		if (j.contains("tiles") && j["tiles"].is_array())
		{
			for (size_t i = 0; i < j["tiles"].size(); i++)
			{
				const json &t = j["tiles"][i];
				if (!t.is_object() || !t.contains("i")) continue;
				const int32 idx = t["i"].get<int32>();
				if (idx < 0) continue;
				TileInfo2D info;
				info.solid = t.value("solid", false);
				if (t.contains("tags") && t["tags"].is_array())
					for (size_t k = 0; k < t["tags"].size(); k++)
						if (t["tags"][k].is_string())
							info.tags.push_back(t["tags"][k].get<std::string>());
				s.tiles[idx] = info;
			}
		}

		out = s;
		return true;
	}

	bool LoadTileSet2D(const std::string &filename, TileSet2D &out,
		std::string *errorOut)
	{
		std::ifstream in(filename.c_str());
		if (!in.is_open())
		{
			Fail(errorOut, "could not open " + filename);
			return false;
		}
		std::stringstream ss;
		ss << in.rdbuf();
		std::string err;
		if (!TileSet2DFromString(ss.str(), out, &err))
		{
			Fail(errorOut, filename + ": " + err);
			return false;
		}
		return true;
	}

	bool SaveTileSet2D(const std::string &filename, const TileSet2D &set,
		std::string *errorOut)
	{
		std::ofstream o(filename.c_str());
		if (!o.is_open())
		{
			Fail(errorOut, "could not write " + filename);
			return false;
		}
		o << TileSet2DToString(set);
		return true;
	}

}
